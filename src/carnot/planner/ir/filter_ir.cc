/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "src/carnot/planner/ir/filter_ir.h"
#include "src/carnot/planner/ir/ir.h"

namespace px {
namespace carnot {
namespace planner {

Status FilterIR::Init(OperatorIR* parent, ExpressionIR* expr) {
  PL_RETURN_IF_ERROR(AddParent(parent));
  return SetFilterExpr(expr);
}

std::string FilterIR::DebugString() const {
  return absl::Substitute("$0(id=$1, expr=$2)", type_string(), id(), filter_expr_->DebugString());
}

Status FilterIR::SetFilterExpr(ExpressionIR* expr) {
  auto old_filter_expr = filter_expr_;
  if (old_filter_expr) {
    PL_RETURN_IF_ERROR(graph()->DeleteEdge(this, old_filter_expr));
  }
  PL_ASSIGN_OR_RETURN(filter_expr_, graph()->OptionallyCloneWithEdge(this, expr));
  if (old_filter_expr) {
    PL_RETURN_IF_ERROR(graph()->DeleteOrphansInSubtree(old_filter_expr->id()));
  }
  return Status::OK();
}

absl::flat_hash_set<std::string> ColumnsFromRelation(Relation r) {
  auto col_names = r.col_names();
  return {col_names.begin(), col_names.end()};
}

StatusOr<std::vector<absl::flat_hash_set<std::string>>> FilterIR::RequiredInputColumns() const {
  DCHECK(IsRelationInit());
  PL_ASSIGN_OR_RETURN(auto filter_cols, filter_expr_->InputColumnNames());
  auto relation_cols = ColumnsFromRelation(relation());
  filter_cols.insert(relation_cols.begin(), relation_cols.end());
  return std::vector<absl::flat_hash_set<std::string>>{filter_cols};
}

StatusOr<absl::flat_hash_set<std::string>> FilterIR::PruneOutputColumnsToImpl(
    const absl::flat_hash_set<std::string>& output_cols) {
  return output_cols;
}

Status FilterIR::ToProto(planpb::Operator* op) const {
  auto pb = op->mutable_filter_op();
  op->set_op_type(planpb::FILTER_OPERATOR);
  DCHECK_EQ(parents().size(), 1UL);

  auto col_names = relation().col_names();
  for (const auto& col_name : col_names) {
    planpb::Column* col_pb = pb->add_columns();
    col_pb->set_node(parents()[0]->id());
    auto parent_relation = parents()[0]->relation();
    DCHECK(parent_relation.HasColumn(col_name)) << absl::Substitute(
        "Don't have $0 in parent_relation $1", col_name, parent_relation.DebugString());
    col_pb->set_index(parent_relation.GetColumnIndex(col_name));
  }

  auto expr = pb->mutable_expression();
  PL_RETURN_IF_ERROR(filter_expr_->ToProto(expr));
  return Status::OK();
}

Status FilterIR::CopyFromNodeImpl(const IRNode* node,
                                  absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) {
  const FilterIR* filter = static_cast<const FilterIR*>(node);
  PL_ASSIGN_OR_RETURN(ExpressionIR * new_node,
                      graph()->CopyNode(filter->filter_expr_, copied_nodes_map));
  PL_RETURN_IF_ERROR(SetFilterExpr(new_node));
  return Status::OK();
}

Status FilterIR::ResolveType(CompilerState* compiler_state) {
  PL_RETURN_IF_ERROR(ResolveExpressionType(filter_expr_, compiler_state, parent_types()));
  PL_ASSIGN_OR_RETURN(auto type_ptr, OperatorIR::DefaultResolveType(parent_types()));
  return SetResolvedType(type_ptr);
}

}  // namespace planner
}  // namespace carnot
}  // namespace px
