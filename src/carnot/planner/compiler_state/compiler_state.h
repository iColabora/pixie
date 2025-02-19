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

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "src/carnot/planner/compiler_state/registry_info.h"

#include "src/common/base/base.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace planner {

/**
 * IDRegistryKey is the class used to uniquely refer to a UDF or UDA in the ID registry.
 * Distinct from the normal RegistryKey since that registry only cares about types, whereas the ID
 * registry has to keep track of the values of init args as well.
 */
class IDRegistryKey {
 public:
  /**
   * IDRegistryKey constructor.
   *
   * @param name of the UDF/UDA.
   * @param registry_arg_types the types used for registry resolution.
   * @param init_arg_vals the values of the init args.
   */
  IDRegistryKey(std::string name, const std::vector<types::DataType>& registry_arg_types,
                const std::vector<uint64_t>& init_arg_hashes)
      : name_(std::move(name)),
        registry_arg_types_(registry_arg_types),
        init_arg_hashes_(init_arg_hashes) {}

  /**
   * Access name of the UDF/UDA.
   * @return The name of the udf/uda.
   */
  const std::string& name() const { return name_; }

  const std::vector<types::DataType>& registry_arg_types() { return registry_arg_types_; }

  /**
   * LessThan operator overload so we can use this in maps.
   * @param lhs is the other RegistryKey.
   * @return a stable less than compare.
   */
  bool operator<(const IDRegistryKey& lhs) const {
    if (name_ == lhs.name_) {
      if (registry_arg_types_ == lhs.registry_arg_types_) {
        return init_arg_hashes_ < lhs.init_arg_hashes_;
      }
      return registry_arg_types_ < lhs.registry_arg_types_;
    }
    return name_ < lhs.name_;
  }

 protected:
  std::string name_;
  std::vector<types::DataType> registry_arg_types_;
  std::vector<uint64_t> init_arg_hashes_;
};

using RelationMap = std::unordered_map<std::string, table_store::schema::Relation>;
class CompilerState : public NotCopyable {
 public:
  /**
   * CompilerState manages the state needed to compile a single query. A new one will
   * be constructed for every query compiled in Carnot and it will not be reused.
   */
  CompilerState(std::unique_ptr<RelationMap> relation_map, RegistryInfo* registry_info,
                types::Time64NSValue time_now, std::string_view result_address,
                std::string_view result_ssl_targetname = "")
      : CompilerState(std::move(relation_map), registry_info, time_now,
                      /* max_output_rows_per_table */ 0, result_address, result_ssl_targetname) {}

  CompilerState(std::unique_ptr<RelationMap> relation_map, RegistryInfo* registry_info,
                types::Time64NSValue time_now, int64_t max_output_rows_per_table,
                std::string_view result_address, std::string_view result_ssl_targetname)
      : relation_map_(std::move(relation_map)),
        registry_info_(registry_info),
        time_now_(time_now),
        max_output_rows_per_table_(max_output_rows_per_table),
        result_address_(std::string(result_address)),
        result_ssl_targetname_(std::string(result_ssl_targetname)) {}

  CompilerState() = delete;

  RelationMap* relation_map() const { return relation_map_.get(); }
  RegistryInfo* registry_info() const { return registry_info_; }
  types::Time64NSValue time_now() const { return time_now_; }
  const std::string& result_address() const { return result_address_; }
  const std::string& result_ssl_targetname() const { return result_ssl_targetname_; }

  std::map<IDRegistryKey, int64_t> udf_to_id_map() const { return udf_to_id_map_; }
  std::map<IDRegistryKey, int64_t> uda_to_id_map() const { return uda_to_id_map_; }

  int64_t GetUDFID(const IDRegistryKey& key) {
    auto id = udf_to_id_map_.find(key);
    if (id != udf_to_id_map_.end()) {
      return id->second;
    }
    auto new_id = udf_to_id_map_.size();
    udf_to_id_map_[key] = new_id;
    return new_id;
  }

  int64_t GetUDAID(const IDRegistryKey& key) {
    auto id = uda_to_id_map_.find(key);
    if (id != uda_to_id_map_.end()) {
      return id->second;
    }
    auto new_id = uda_to_id_map_.size();
    uda_to_id_map_[key] = new_id;
    return new_id;
  }

  int64_t max_output_rows_per_table() { return max_output_rows_per_table_; }
  bool has_max_output_rows_per_table() { return max_output_rows_per_table_ > 0; }

 private:
  std::unique_ptr<RelationMap> relation_map_;
  RegistryInfo* registry_info_;
  types::Time64NSValue time_now_;
  std::map<IDRegistryKey, int64_t> udf_to_id_map_;
  std::map<IDRegistryKey, int64_t> uda_to_id_map_;

  int64_t max_output_rows_per_table_ = 0;
  const std::string result_address_;
  const std::string result_ssl_targetname_;
};

}  // namespace planner
}  // namespace carnot
}  // namespace px
