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

#include "src/carnot/planner/ir/empty_source_ir.h"

namespace px {
namespace carnot {
namespace planner {

Status EmptySourceIR::Init(const Relation& relation) {
  PL_RETURN_IF_ERROR(SetRelation(relation));
  return Status::OK();
}
Status EmptySourceIR::ToProto(planpb::Operator* op) const {
  auto pb = op->mutable_empty_source_op();
  op->set_op_type(planpb::EMPTY_SOURCE_OPERATOR);

  for (size_t i = 0; i < relation().NumColumns(); ++i) {
    pb->add_column_names(relation().col_names()[i]);
    pb->add_column_types(relation().col_types()[i]);
  }

  return Status::OK();
}

Status EmptySourceIR::CopyFromNodeImpl(const IRNode* source,
                                       absl::flat_hash_map<const IRNode*, IRNode*>*) {
  const EmptySourceIR* empty = static_cast<const EmptySourceIR*>(source);
  return SetRelation(empty->relation());
}

StatusOr<absl::flat_hash_set<std::string>> EmptySourceIR::PruneOutputColumnsToImpl(
    const absl::flat_hash_set<std::string>&) {
  return error::Unimplemented("Cannot prune columns for empty source.");
}

}  // namespace planner
}  // namespace carnot
}  // namespace px
