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

#pragma once

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "src/carnot/planner/compiler_error_context/compiler_error_context.h"
#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/compilerpb/compiler_status.pb.h"
#include "src/carnot/planner/ir/expression_ir.h"
#include "src/carnot/planner/ir/ir_node.h"
#include "src/carnot/planner/ir/operator_ir.h"
#include "src/carnot/planner/types/types.h"

namespace px {
namespace carnot {
namespace planner {

class FilterIR : public OperatorIR {
 public:
  FilterIR() = delete;
  explicit FilterIR(int64_t id) : OperatorIR(id, IRNodeType::kFilter) {}
  std::string DebugString() const override;

  ExpressionIR* filter_expr() const { return filter_expr_; }
  Status SetFilterExpr(ExpressionIR* expr);
  Status ToProto(planpb::Operator*) const override;
  Status ResolveType(CompilerState* compiler_state);

  Status Init(OperatorIR* parent, ExpressionIR* expr);

  Status CopyFromNodeImpl(const IRNode* node,
                          absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) override;

  StatusOr<std::vector<absl::flat_hash_set<std::string>>> RequiredInputColumns() const override;

 protected:
  StatusOr<absl::flat_hash_set<std::string>> PruneOutputColumnsToImpl(
      const absl::flat_hash_set<std::string>& output_cols) override;

 private:
  ExpressionIR* filter_expr_ = nullptr;
};

absl::flat_hash_set<std::string> ColumnsFromRelation(Relation r);

}  // namespace planner
}  // namespace carnot
}  // namespace px
