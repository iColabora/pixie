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

#include <gtest/gtest.h>

#include <vector>

#include "src/carnot/planner/distributed/splitter/presplit_optimizer/filter_push_down_rule.h"
#include "src/carnot/planner/test_utils.h"
#include "src/carnot/udf_exporter/udf_exporter.h"

namespace px {
namespace carnot {
namespace planner {
namespace distributed {

using ::testing::ElementsAre;

using FilterPushDownTest = testutils::DistributedRulesTest;
TEST_F(FilterPushDownTest, simple_no_op) {
  Relation relation({types::DataType::INT64, types::DataType::INT64}, {"abc", "xyz"});
  MemorySourceIR* src = MakeMemSource(relation);

  auto constant = MakeInt(10);
  auto column = MakeColumn("column", 0);
  column->ResolveColumnType(types::DataType::INT64);

  auto eq_func = MakeEqualsFunc(column, constant);
  eq_func->SetRegistryArgTypes({types::DataType::INT64, types::DataType::INT64});
  EXPECT_OK(eq_func->SplitInitArgs(0));
  FilterIR* filter = MakeFilter(src, eq_func);
  MakeMemSink(filter, "foo", {});

  FilterPushdownRule rule(compiler_state_.get());
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_FALSE(result.ValueOrDie());
}

TEST_F(FilterPushDownTest, simple) {
  Relation relation({types::DataType::INT64, types::DataType::INT64}, {"abc", "xyz"});
  MemorySourceIR* src = MakeMemSource(relation);
  MapIR* map =
      MakeMap(src, {{"abc_1", MakeColumn("abc", 0)}, {"abc", MakeColumn("abc", 0)}}, false);
  auto col = MakeColumn("abc", 0);
  col->ResolveColumnType(types::DataType::INT64);
  auto eq_func = MakeEqualsFunc(col, MakeInt(2));
  eq_func->SetRegistryArgTypes({types::DataType::INT64, types::DataType::INT64});
  EXPECT_OK(eq_func->SplitInitArgs(0));
  FilterIR* filter = MakeFilter(map, eq_func);
  MemorySinkIR* sink = MakeMemSink(filter, "foo", {});

  FilterPushdownRule rule(compiler_state_.get());
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());
  EXPECT_THAT(sink->parents(), ElementsAre(map));
  EXPECT_THAT(map->parents(), ElementsAre(filter));
  EXPECT_THAT(filter->parents(), ElementsAre(src));
  EXPECT_MATCH(filter->filter_expr(), Equals(ColumnNode("abc"), Int(2)));
}

TEST_F(FilterPushDownTest, two_col_filter) {
  Relation relation({types::DataType::INT64}, {"abc"});
  MemorySourceIR* src = MakeMemSource(relation);
  MapIR* map1 = MakeMap(src, {{"abc", MakeColumn("abc", 0)}}, false);
  MapIR* map2 = MakeMap(map1, {{"xyz", MakeInt(3)}, {"abc", MakeColumn("abc", 0)}}, false);
  MapIR* map3 =
      MakeMap(map2, {{"xyz", MakeColumn("xyz", 0)}, {"abc", MakeColumn("abc", 0)}}, false);
  MapIR* map4 =
      MakeMap(map3, {{"xyz", MakeColumn("xyz", 0)}, {"abc", MakeColumn("abc", 0)}}, false);
  auto col1 = MakeColumn("abc", 0);
  col1->ResolveColumnType(types::DataType::INT64);
  auto col2 = MakeColumn("xyz", 0);
  col2->ResolveColumnType(types::DataType::INT64);
  auto eq_func = MakeEqualsFunc(col1, col2);
  eq_func->SetRegistryArgTypes({types::DataType::INT64, types::DataType::INT64});
  EXPECT_OK(eq_func->SplitInitArgs(0));
  FilterIR* filter = MakeFilter(map4, eq_func);
  MemorySinkIR* sink = MakeMemSink(filter, "foo", {});

  FilterPushdownRule rule(compiler_state_.get());
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());
  EXPECT_THAT(sink->parents(), ElementsAre(map4));
  EXPECT_THAT(map4->parents(), ElementsAre(map3));
  EXPECT_THAT(map3->parents(), ElementsAre(filter));
  EXPECT_THAT(filter->parents(), ElementsAre(map2));
  EXPECT_THAT(map2->parents(), ElementsAre(map1));
  EXPECT_THAT(map1->parents(), ElementsAre(src));
  EXPECT_MATCH(filter->filter_expr(), Equals(ColumnNode("abc"), ColumnNode("xyz")));
}

TEST_F(FilterPushDownTest, multi_condition_filter) {
  Relation relation({types::DataType::INT64, types::DataType::INT64}, {"abc", "xyz"});
  MemorySourceIR* src = MakeMemSource(relation);
  MapIR* map1 = MakeMap(src, {{"abc", MakeColumn("abc", 0)}}, false);
  MapIR* map2 = MakeMap(map1, {{"xyz", MakeInt(3)}, {"abc", MakeColumn("abc", 0)}}, false);
  MapIR* map3 =
      MakeMap(map2, {{"xyz", MakeColumn("xyz", 0)}, {"abc", MakeColumn("abc", 0)}}, false);

  auto col1 = MakeColumn("abc", 0);
  col1->ResolveColumnType(types::DataType::INT64);
  auto col2 = MakeColumn("xyz", 0);
  col2->ResolveColumnType(types::DataType::INT64);

  auto equals1 = MakeEqualsFunc(col1, MakeInt(2));
  equals1->SetOutputDataType(types::DataType::BOOLEAN);
  equals1->SetRegistryArgTypes({types::DataType::INT64, types::DataType::INT64});
  EXPECT_OK(equals1->SplitInitArgs(0));
  auto equals2 = MakeEqualsFunc(col2, MakeInt(3));
  equals2->SetOutputDataType(types::DataType::BOOLEAN);
  equals2->SetRegistryArgTypes({types::DataType::INT64, types::DataType::INT64});
  EXPECT_OK(equals2->SplitInitArgs(0));

  auto and_func = MakeAndFunc(equals1, equals2);
  and_func->SetRegistryArgTypes({types::DataType::BOOLEAN, types::DataType::BOOLEAN});
  EXPECT_OK(and_func->SplitInitArgs(0));
  FilterIR* filter = MakeFilter(map3, and_func);
  MemorySinkIR* sink = MakeMemSink(filter, "foo", {});

  FilterPushdownRule rule(compiler_state_.get());
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());
  EXPECT_THAT(sink->parents(), ElementsAre(map3));
  EXPECT_THAT(map3->parents(), ElementsAre(filter));
  EXPECT_THAT(filter->parents(), ElementsAre(map2));
  EXPECT_THAT(map2->parents(), ElementsAre(map1));
  EXPECT_THAT(map1->parents(), ElementsAre(src));
  EXPECT_MATCH(filter->filter_expr(),
               LogicalAnd(Equals(ColumnNode("abc"), Int(2)), Equals(ColumnNode("xyz"), Int(3))));
}

TEST_F(FilterPushDownTest, column_rename) {
  // abc -> def
  // filter on def
  Relation relation({types::DataType::INT64, types::DataType::INT64}, {"abc", "xyz"});
  MemorySourceIR* src = MakeMemSource(relation);
  MapIR* map1 = MakeMap(src, {{"def", MakeColumn("abc", 0)}}, false);
  MapIR* map2 = MakeMap(map1, {{"xyz", MakeInt(3)}, {"def", MakeColumn("def", 0)}}, false);

  auto col = MakeColumn("def", 0);
  col->ResolveColumnType(types::DataType::INT64);
  auto eq_func = MakeEqualsFunc(col, MakeInt(2));
  eq_func->SetRegistryArgTypes({types::DataType::INT64, types::DataType::INT64});
  EXPECT_OK(eq_func->SplitInitArgs(0));
  FilterIR* filter = MakeFilter(map2, eq_func);
  MemorySinkIR* sink = MakeMemSink(filter, "foo", {});

  FilterPushdownRule rule(compiler_state_.get());
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());
  EXPECT_THAT(sink->parents(), ElementsAre(map2));
  EXPECT_THAT(map2->parents(), ElementsAre(map1));
  EXPECT_THAT(map1->parents(), ElementsAre(filter));
  EXPECT_THAT(filter->parents(), ElementsAre(src));
  EXPECT_MATCH(filter->filter_expr(), Equals(ColumnNode("abc"), Int(2)));
}

TEST_F(FilterPushDownTest, two_filters_different_cols) {
  // create abc
  // create def
  // create ghi
  // filter on def
  // filter on abc
  Relation relation({types::DataType::INT64}, {"abc"});
  MemorySourceIR* src = MakeMemSource(relation);
  MapIR* map1 = MakeMap(src, {{"abc", MakeColumn("abc", 0)}, {"def", MakeInt(2)}}, false);
  MapIR* map2 = MakeMap(
      map1, {{"abc", MakeColumn("abc", 0)}, {"def", MakeColumn("def", 0)}, {"ghi", MakeInt(2)}},
      false);

  auto col1 = MakeColumn("def", 0);
  col1->ResolveColumnType(types::DataType::INT64);
  auto eq_func = MakeEqualsFunc(col1, MakeInt(2));
  eq_func->SetRegistryArgTypes({types::DataType::INT64, types::DataType::INT64});
  EXPECT_OK(eq_func->SplitInitArgs(0));
  FilterIR* filter1 = MakeFilter(map2, eq_func);

  auto col2 = MakeColumn("abc", 0);
  col2->ResolveColumnType(types::DataType::INT64);
  auto eq_func2 = MakeEqualsFunc(col2, MakeInt(3));
  eq_func2->SetRegistryArgTypes({types::DataType::INT64, types::DataType::INT64});
  EXPECT_OK(eq_func2->SplitInitArgs(0));
  FilterIR* filter2 = MakeFilter(filter1, eq_func2);
  MemorySinkIR* sink = MakeMemSink(filter2, "foo", {});

  FilterPushdownRule rule(compiler_state_.get());
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());
  EXPECT_THAT(sink->parents(), ElementsAre(map2));
  EXPECT_THAT(map2->parents(), ElementsAre(filter1));
  EXPECT_THAT(filter1->parents(), ElementsAre(map1));
  EXPECT_THAT(map1->parents(), ElementsAre(filter2));
  EXPECT_THAT(filter2->parents(), ElementsAre(src));

  EXPECT_MATCH(filter1->filter_expr(), Equals(ColumnNode("def"), Int(2)));
  EXPECT_MATCH(filter2->filter_expr(), Equals(ColumnNode("abc"), Int(3)));
}

TEST_F(FilterPushDownTest, two_filters_same_cols) {
  // create def
  // filter on def
  // create ghi
  // filter on def again
  Relation relation({types::DataType::INT64}, {"abc"});
  MemorySourceIR* src = MakeMemSource(relation);
  MapIR* map1 = MakeMap(src, {{"abc", MakeColumn("abc", 0)}, {"def", MakeInt(2)}}, false);
  MapIR* map2 =
      MakeMap(map1, {{"abc", MakeColumn("abc", 0)}, {"def", MakeColumn("def", 0)}}, false);

  auto col1 = MakeColumn("def", 0);
  col1->ResolveColumnType(types::DataType::INT64);
  auto eq_func = MakeEqualsFunc(col1, MakeInt(2));
  eq_func->SetRegistryArgTypes({types::DataType::INT64, types::DataType::INT64});
  EXPECT_OK(eq_func->SplitInitArgs(0));
  FilterIR* filter1 = MakeFilter(map2, eq_func);
  MapIR* map3 = MakeMap(
      filter1, {{"abc", MakeColumn("abc", 0)}, {"def", MakeColumn("def", 0)}, {"ghi", MakeInt(2)}},
      false);

  auto col2 = MakeColumn("def", 0);
  col2->ResolveColumnType(types::DataType::INT64);
  auto eq_func2 = MakeEqualsFunc(MakeInt(3), col2);
  eq_func2->SetRegistryArgTypes({types::DataType::INT64, types::DataType::INT64});
  EXPECT_OK(eq_func2->SplitInitArgs(0));
  FilterIR* filter2 = MakeFilter(map3, eq_func2);
  MemorySinkIR* sink = MakeMemSink(filter2, "foo", {});

  FilterPushdownRule rule(compiler_state_.get());
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ConsumeValueOrDie());

  EXPECT_THAT(sink->parents(), ElementsAre(map3));
  EXPECT_THAT(map3->parents(), ElementsAre(map2));
  EXPECT_THAT(map2->parents(), ElementsAre(filter1));
  EXPECT_THAT(filter1->parents(), ElementsAre(filter2));
  EXPECT_THAT(filter2->parents(), ElementsAre(map1));
  EXPECT_THAT(map1->parents(), ElementsAre(src));

  EXPECT_MATCH(filter1->filter_expr(), Equals(ColumnNode("def"), Int(2)));
  EXPECT_MATCH(filter2->filter_expr(), Equals(ColumnNode("def"), Int(3)));
}

TEST_F(FilterPushDownTest, single_col_rename_collision) {
  // 0: abc, def
  // 1: abc->def, drop first def, xyz->2
  // 2: abc=xyz, def=def
  // 3: filter on def (bool col) becomes filter on abc at position 1.
  Relation relation({types::DataType::INT64, types::DataType::INT64}, {"abc", "def"});
  MemorySourceIR* src = MakeMemSource(relation);
  MapIR* map1 = MakeMap(src, {{"def", MakeColumn("abc", 0)}, {"xyz", MakeInt(2)}}, false);
  MapIR* map2 =
      MakeMap(map1, {{"def", MakeColumn("def", 0)}, {"abc", MakeColumn("xyz", 0)}}, false);
  FilterIR* filter = graph->CreateNode<FilterIR>(ast, map2, MakeColumn("def", 0)).ValueOrDie();
  MemorySinkIR* sink = MakeMemSink(filter, "foo", {});

  FilterPushdownRule rule(compiler_state_.get());
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());
  EXPECT_THAT(sink->parents(), ElementsAre(map2));
  EXPECT_THAT(map2->parents(), ElementsAre(map1));
  EXPECT_THAT(map1->parents(), ElementsAre(filter));
  EXPECT_THAT(filter->parents(), ElementsAre(src));

  // Make sure the former name of the filter column gets used.
  EXPECT_MATCH(filter->filter_expr(), ColumnNode("abc"));
}

TEST_F(FilterPushDownTest, single_col_rename_collision_swap) {
  // abc -> xyz, xyz -> abc
  // filter on abc
  Relation relation({types::DataType::INT64, types::DataType::INT64}, {"abc", "xyz"});
  MemorySourceIR* src = MakeMemSource(relation);
  MapIR* map = MakeMap(src, {{"xyz", MakeColumn("abc", 0)}, {"abc", MakeColumn("xyz", 0)}}, false);

  auto col = MakeColumn("abc", 0);
  col->ResolveColumnType(types::DataType::INT64);
  auto eq_func = MakeEqualsFunc(col, MakeInt(2));
  eq_func->SetRegistryArgTypes({types::DataType::INT64, types::DataType::INT64});
  EXPECT_OK(eq_func->SplitInitArgs(0));
  FilterIR* filter = MakeFilter(map, eq_func);
  MemorySinkIR* sink = MakeMemSink(filter, "foo", {});

  FilterPushdownRule rule(compiler_state_.get());
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());
  EXPECT_THAT(sink->parents(), ElementsAre(map));
  EXPECT_THAT(map->parents(), ElementsAre(filter));
  EXPECT_THAT(filter->parents(), ElementsAre(src));
  EXPECT_MATCH(filter->filter_expr(), Equals(ColumnNode("xyz"), Int(2)));
}

TEST_F(FilterPushDownTest, multicol_rename_collision) {
  // abc -> def, def -> abc
  // abc -> def, def -> abc
  // filter on abc
  Relation relation({types::DataType::INT64, types::DataType::INT64}, {"abc", "xyz"});
  MemorySourceIR* src = MakeMemSource(relation);
  MapIR* map1 = MakeMap(src, {{"xyz", MakeColumn("abc", 0)}, {"abc", MakeColumn("xyz", 0)}}, false);
  MapIR* map2 =
      MakeMap(map1, {{"xyz", MakeColumn("abc", 0)}, {"abc", MakeColumn("xyz", 0)}}, false);

  auto col = MakeColumn("abc", 0);
  col->ResolveColumnType(types::DataType::INT64);
  auto eq_func = MakeEqualsFunc(col, MakeInt(2));
  eq_func->SetRegistryArgTypes({types::DataType::INT64, types::DataType::INT64});
  EXPECT_OK(eq_func->SplitInitArgs(0));
  FilterIR* filter = MakeFilter(map2, eq_func);
  MemorySinkIR* sink = MakeMemSink(filter, "foo", {});

  FilterPushdownRule rule(compiler_state_.get());
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());

  EXPECT_THAT(sink->parents(), ElementsAre(map2));
  EXPECT_THAT(map2->parents(), ElementsAre(map1));
  EXPECT_THAT(map1->parents(), ElementsAre(filter));
  EXPECT_THAT(filter->parents(), ElementsAre(src));

  EXPECT_MATCH(filter->filter_expr(), Equals(ColumnNode("abc"), Int(2)));
}

TEST_F(FilterPushDownTest, agg_group) {
  Relation relation({types::DataType::INT64, types::DataType::INT64}, {"abc", "xyz"});
  MemorySourceIR* src = MakeMemSource(relation);
  auto col0 = MakeColumn("xyz", 0);
  col0->ResolveColumnType(types::DataType::INT64);
  auto mean_func = MakeMeanFunc(col0);
  mean_func->SetRegistryArgTypes({types::INT64});
  EXPECT_OK(mean_func->SplitInitArgs(0));
  BlockingAggIR* agg = MakeBlockingAgg(src, {MakeColumn("abc", 0)}, {{"out", mean_func}});
  auto col1 = MakeColumn("abc", 0);
  col1->ResolveColumnType(types::DataType::INT64);
  auto eq_func = MakeEqualsFunc(col1, MakeInt(2));
  eq_func->SetRegistryArgTypes({types::DataType::INT64, types::DataType::INT64});
  EXPECT_OK(eq_func->SplitInitArgs(0));
  FilterIR* filter = MakeFilter(agg, eq_func);
  MemorySinkIR* sink = MakeMemSink(filter, "");

  FilterPushdownRule rule(compiler_state_.get());
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());

  EXPECT_THAT(sink->parents(), ElementsAre(agg));
  EXPECT_THAT(agg->parents(), ElementsAre(filter));
  EXPECT_THAT(filter->parents(), ElementsAre(src));
  EXPECT_MATCH(filter->filter_expr(), Equals(ColumnNode("abc"), Int(2)));
}

TEST_F(FilterPushDownTest, agg_expr_no_push) {
  Relation relation({types::DataType::INT64, types::DataType::INT64}, {"abc", "xyz"});
  MemorySourceIR* src = MakeMemSource(relation);
  auto col0 = MakeColumn("xyz", 0);
  col0->ResolveColumnType(types::DataType::INT64);
  auto mean_func = MakeMeanFunc(col0);
  mean_func->SetRegistryArgTypes({types::INT64});
  EXPECT_OK(mean_func->SplitInitArgs(0));
  BlockingAggIR* agg = MakeBlockingAgg(src, {MakeColumn("abc", 0)}, {{"xyz", mean_func}});
  auto col1 = MakeColumn("xyz", 0);
  col1->ResolveColumnType(types::DataType::INT64);
  auto col2 = MakeColumn("abc", 0);
  col2->ResolveColumnType(types::DataType::INT64);
  auto eq_func = MakeEqualsFunc(col1, col2);
  eq_func->SetRegistryArgTypes({types::DataType::INT64, types::DataType::INT64});
  EXPECT_OK(eq_func->SplitInitArgs(0));
  FilterIR* filter = MakeFilter(agg, eq_func);
  MakeMemSink(filter, "");

  FilterPushdownRule rule(compiler_state_.get());
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_FALSE(result.ValueOrDie());
}

TEST_F(FilterPushDownTest, multiple_children_dont_push) {
  Relation relation({types::DataType::INT64, types::DataType::INT64}, {"abc", "xyz"});
  MemorySourceIR* src = MakeMemSource(relation);
  auto mean_func = MakeMeanFunc(MakeColumn("xyz", 0));
  mean_func->SetRegistryArgTypes({types::INT64});
  EXPECT_OK(mean_func->SplitInitArgs(0));
  BlockingAggIR* agg = MakeBlockingAgg(src, {MakeColumn("abc", 0)}, {{"out", mean_func}});
  auto col = MakeColumn("abc", 0);
  col->ResolveColumnType(types::DataType::INT64);
  auto eq_func = MakeEqualsFunc(col, MakeInt(2));
  eq_func->SetRegistryArgTypes({types::DataType::INT64, types::DataType::INT64});
  EXPECT_OK(eq_func->SplitInitArgs(0));
  FilterIR* filter = MakeFilter(agg, eq_func);
  MakeMemSink(filter, "");
  MakeMemSink(agg, "2");

  FilterPushdownRule rule(compiler_state_.get());
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_FALSE(result.ValueOrDie());

  // Don't push it anywhere.
  EXPECT_MATCH(filter->Children()[0], MemorySink());
  EXPECT_MATCH(filter->parents()[0], BlockingAgg());
}

TEST_F(FilterPushDownTest, kelvin_only_filter) {
  Relation relation1({types::DataType::INT64, types::DataType::INT64}, {"abc", "xyz"});
  Relation relation2({types::DataType::INT64}, {"abc"});

  MemorySourceIR* src = MakeMemSource(relation1);
  auto func = MakeFunc("pem_only", {});
  EXPECT_OK(func->SplitInitArgs(0));
  MapIR* map1 = MakeMap(src, {{"abc", func}}, false);
  ASSERT_OK(map1->SetRelation(relation2));
  MapIR* map2 = MakeMap(map1, {{"def", MakeColumn("abc", 0)}}, false);
  auto func2 = MakeFunc("kelvin_only", {});
  EXPECT_OK(func2->SplitInitArgs(0));
  FilterIR* filter = MakeFilter(map2, func2);
  MemorySinkIR* sink = MakeMemSink(filter, "foo", {});

  FilterPushdownRule rule(compiler_state_.get());
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());

  EXPECT_THAT(sink->parents(), ElementsAre(map2));
  EXPECT_EQ(1, map2->parents().size());
  EXPECT_MATCH(map2->parents()[0], Filter());
  EXPECT_THAT(map2->parents()[0]->parents(), ElementsAre(map1));
}

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
