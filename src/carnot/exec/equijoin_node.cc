#include <algorithm>
#include <string>
#include <unordered_set>

#include "src/carnot/exec/equijoin_node.h"

namespace pl {
namespace carnot {
namespace exec {

using table_store::schema::RowBatch;
using table_store::schema::RowDescriptor;

std::string EquijoinNode::DebugStringImpl() {
  return absl::Substitute("Exec::JoinNode<$0>", absl::StrJoin(plan_node_->column_names(), ","));
}

bool EquijoinNode::IsProbeTable(size_t parent_index) {
  return (parent_index == 0) == (probe_table_ == EquijoinNode::JoinInputTable::kLeftTable);
}

Status EquijoinNode::InitImpl(
    const plan::Operator& plan_node, const table_store::schema::RowDescriptor& output_descriptor,
    const std::vector<table_store::schema::RowDescriptor>& input_descriptors) {
  CHECK(plan_node.op_type() == planpb::OperatorType::JOIN_OPERATOR);
  if (input_descriptors.size() != 2) {
    return error::InvalidArgument("Join operator expects a two input relations, got $0",
                                  input_descriptors.size());
  }
  const auto* join_plan_node = static_cast<const plan::JoinOperator*>(&plan_node);
  plan_node_ = std::make_unique<plan::JoinOperator>(*join_plan_node);
  output_descriptor_ = std::make_unique<RowDescriptor>(output_descriptor);
  output_rows_per_batch_ =
      plan_node_->rows_per_batch() == 0 ? kDefaultJoinRowBatchSize : plan_node_->rows_per_batch();

  if (plan_node_->order_by_time() && plan_node_->time_column().parent_index() == 0) {
    // Make the probe table the left table when we need to preserve the order of the left table in
    // the output.
    probe_table_ = EquijoinNode::JoinInputTable::kLeftTable;
  } else {
    probe_table_ = EquijoinNode::JoinInputTable::kRightTable;
  }

  switch (plan_node_->type()) {
    case planpb::JoinOperator::INNER:
      build_spec_.emit_unmatched_rows = false;
      probe_spec_.emit_unmatched_rows = false;
      break;
    case planpb::JoinOperator::LEFT_OUTER:
      build_spec_.emit_unmatched_rows = probe_table_ != EquijoinNode::JoinInputTable::kLeftTable;
      probe_spec_.emit_unmatched_rows = probe_table_ == EquijoinNode::JoinInputTable::kLeftTable;
      break;
    case planpb::JoinOperator::FULL_OUTER:
      build_spec_.emit_unmatched_rows = true;
      probe_spec_.emit_unmatched_rows = true;
      break;
    default:
      return error::Internal(absl::Substitute("EquijoinNode: Unknown Join Type $0",
                                              static_cast<int>(plan_node_->type())));
  }

  for (const auto& eq_condition : plan_node_->equality_conditions()) {
    int64_t left_index = eq_condition.left_column_index();
    int64_t right_index = eq_condition.right_column_index();

    CHECK_EQ(input_descriptors[0].type(left_index), input_descriptors[1].type(right_index));
    key_data_types_.emplace_back(input_descriptors[0].type(left_index));

    build_spec_.key_indices.emplace_back(
        probe_table_ == EquijoinNode::JoinInputTable::kLeftTable ? right_index : left_index);
    probe_spec_.key_indices.emplace_back(
        probe_table_ == EquijoinNode::JoinInputTable::kLeftTable ? left_index : right_index);
  }

  const auto& output_cols = plan_node_->output_columns();
  for (size_t i = 0; i < output_cols.size(); ++i) {
    auto parent_index = output_cols[i].parent_index();
    auto input_column_index = output_cols[i].column_index();
    auto dt = input_descriptors[parent_index].type(input_column_index);

    TableSpec& selected_spec = IsProbeTable(parent_index) ? probe_spec_ : build_spec_;
    selected_spec.input_col_indices.emplace_back(input_column_index);
    selected_spec.input_col_types.emplace_back(dt);
    selected_spec.output_col_indices.emplace_back(i);
  }

  return Status::OK();
}

Status EquijoinNode::InitializeColumnBuilders() {
  for (size_t i = 0; i < output_descriptor_->size(); ++i) {
    column_builders_[i] =
        MakeArrowBuilder(output_descriptor_->type(i), arrow::default_memory_pool());
    PL_RETURN_IF_ERROR(column_builders_[i]->Reserve(output_rows_per_batch_));
  }
  return Status::OK();
}

Status EquijoinNode::PrepareImpl(ExecState* /*exec_state*/) {
  column_builders_.resize(output_descriptor_->size());
  PL_RETURN_IF_ERROR(InitializeColumnBuilders());

  return Status::OK();
}

Status EquijoinNode::OpenImpl(ExecState* /*exec_state*/) { return Status::OK(); }

Status EquijoinNode::CloseImpl(ExecState* /*exec_state*/) {
  join_keys_chunk_.clear();
  build_buffer_.clear();
  key_values_pool_.Clear();
  return Status::OK();
}

template <types::DataType DT>
void ExtractIntoRowTuples(std::vector<RowTuple*>* row_tuples, arrow::Array* input_col,
                          int rt_col_idx) {
  auto num_rows = input_col->length();
  for (auto row_idx = 0; row_idx < num_rows; ++row_idx) {
    ExtractIntoRowTuple<DT>((*row_tuples)[row_idx], input_col, rt_col_idx, row_idx);
  }
}

Status EquijoinNode::ExtractJoinKeysForBatch(const table_store::schema::RowBatch& rb,
                                             bool is_probe) {
  // Reset the row tuples
  for (auto& rt : join_keys_chunk_) {
    if (rt == nullptr) {
      rt = key_values_pool_.Add(new RowTuple(&key_data_types_));
    } else {
      rt->Reset();
    }
  }

  // Grow the join_keys_chunk_ to be the size of the RowBatch.
  size_t num_rows = rb.num_rows();
  if (join_keys_chunk_.size() < num_rows) {
    int prev_size = join_keys_chunk_.size();
    join_keys_chunk_.reserve(num_rows);
    for (size_t idx = prev_size; idx < num_rows; ++idx) {
      auto tuple_ptr = key_values_pool_.Add(new RowTuple(&key_data_types_));
      join_keys_chunk_.emplace_back(tuple_ptr);
    }
  }

  const TableSpec& spec = is_probe ? probe_spec_ : build_spec_;

  // Scan through all the group args in column order and extract the entire column.
  for (size_t tuple_col_idx = 0; tuple_col_idx < spec.key_indices.size(); ++tuple_col_idx) {
    auto input_col_idx = spec.key_indices[tuple_col_idx];
    auto dt = key_data_types_[tuple_col_idx];
    auto col = rb.ColumnAt(input_col_idx).get();

#define TYPE_CASE(_dt_) ExtractIntoRowTuples<_dt_>(&join_keys_chunk_, col, tuple_col_idx);
    PL_SWITCH_FOREACH_DATATYPE(dt, TYPE_CASE);
#undef TYPE_CASE
  }

  return Status::OK();
}

std::vector<types::SharedColumnWrapper>* CreateWrapper(ObjectPool* pool,
                                                       const std::vector<types::DataType>& types) {
  auto ptr = pool->Add(new std::vector<types::SharedColumnWrapper>(types.size()));
  for (size_t col_idx = 0; col_idx < types.size(); ++col_idx) {
    (*ptr)[col_idx] = types::ColumnWrapper::Make(types[col_idx], 0);
  }
  return ptr;
}

Status EquijoinNode::HashRowBatch(const table_store::schema::RowBatch& rb) {
  if (rb.num_rows() > static_cast<int64_t>(build_wrappers_chunk_.size())) {
    build_wrappers_chunk_.resize(rb.num_rows());
  }
  for (auto row_idx = 0; row_idx < rb.num_rows(); ++row_idx) {
    if (build_wrappers_chunk_[row_idx] == nullptr) {
      build_wrappers_chunk_[row_idx] =
          CreateWrapper(&column_values_pool_, build_spec_.input_col_types);
    }
  }

  // Make sure the map has constructed the necessary column wrappers for all of the tuples.
  for (auto row_idx = 0; row_idx < rb.num_rows(); ++row_idx) {
    auto& rt = join_keys_chunk_[row_idx];
    auto& current = build_buffer_[rt];
    auto wrappers_ptr = current != nullptr ? current : build_wrappers_chunk_[row_idx];

    // Now extract the values into the corresponding column wrappers.
    for (size_t i = 0; i < build_spec_.input_col_indices.size(); ++i) {
      const auto& rb_col_idx = build_spec_.input_col_indices[i];
      auto arr = rb.ColumnAt(rb_col_idx).get();
      const auto& dt = build_spec_.input_col_types[i];

#define TYPE_CASE(_dt_) \
  types::ExtractValueToColumnWrapper<_dt_>(wrappers_ptr->at(i).get(), arr, row_idx);
      PL_SWITCH_FOREACH_DATATYPE(dt, TYPE_CASE);
#undef TYPE_CASE
    }

    if (current == nullptr) {
      std::swap(build_wrappers_chunk_[row_idx], current);
      // Reset the new tuples that we added
      join_keys_chunk_[row_idx] = nullptr;
    }
  }

  return Status::OK();
}

// Create a new output row batch from the column builders, and flush the pending row batch.
// We hold on to a pending row batch because it is difficult to know a priori whether a given
// output batch will be eos/eow.
Status EquijoinNode::NextOutputBatch(ExecState* exec_state) {
  PL_UNUSED(exec_state);
  return error::Internal("NextOutputBatch not implemented yet");
}

Status EquijoinNode::DoProbe(ExecState* exec_state, const table_store::schema::RowBatch& rb) {
  PL_UNUSED(exec_state);
  PL_UNUSED(rb);
  return error::Internal("Not implemented yet");
}

Status EquijoinNode::EmitUnmatchedBuildRows(ExecState* exec_state) {
  PL_UNUSED(exec_state);
  return error::Internal("EmitUnmatchedBuildRows not implemented yet");
}

Status EquijoinNode::ConsumeBuildBatch(ExecState* exec_state,
                                       const table_store::schema::RowBatch& rb) {
  if (rb.eos()) {
    build_eos_ = true;
  }

  PL_RETURN_IF_ERROR(ExtractJoinKeysForBatch(rb, false));
  PL_RETURN_IF_ERROR(HashRowBatch(rb));

  if (build_eos_) {
    while (probe_batches_.size()) {
      PL_RETURN_IF_ERROR(DoProbe(exec_state, probe_batches_.front()));
      probe_batches_.pop();
    }
  }
  return Status::OK();
}

Status EquijoinNode::ConsumeProbeBatch(ExecState* exec_state,
                                       const table_store::schema::RowBatch& rb) {
  PL_UNUSED(exec_state);
  PL_UNUSED(rb);
  return error::Internal("Not implemented yet");
}

Status EquijoinNode::ConsumeNextImpl(ExecState* exec_state, const table_store::schema::RowBatch& rb,
                                     size_t parent_index) {
  if (IsProbeTable(parent_index)) {
    DCHECK(!probe_eos_);
    PL_RETURN_IF_ERROR(ConsumeProbeBatch(exec_state, rb));
  } else {
    DCHECK(!build_eos_);
    PL_RETURN_IF_ERROR(ConsumeBuildBatch(exec_state, rb));
  }

  if (build_eos_ && probe_eos_) {
    if (build_spec_.emit_unmatched_rows) {
      PL_RETURN_IF_ERROR(EmitUnmatchedBuildRows(exec_state));
    }

    if (column_builders_[0]->length()) {
      PL_RETURN_IF_ERROR(NextOutputBatch(exec_state));
    }

    if (pending_output_batch_ == nullptr) {
      // This should only happen when there are no output rows.
      pending_output_batch_ = std::make_unique<RowBatch>(*output_descriptor_, 0);
    }
    // Now send the last row batch and we know it is EOS/EOW.
    pending_output_batch_->set_eos(true);
    pending_output_batch_->set_eow(true);
    PL_RETURN_IF_ERROR(SendRowBatchToChildren(exec_state, *pending_output_batch_));
  }

  return Status::OK();
}

}  // namespace exec
}  // namespace carnot
}  // namespace pl
