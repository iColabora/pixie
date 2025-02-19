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
#include "src/common/base/base.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/kafka/parse.h"

namespace px {
namespace stirling {
namespace protocols {
namespace kafka {
namespace testdata {
// Raw Kafka packets with header.

// APIKey: 0, APIVersion: 9
constexpr uint8_t kProduceRequest[] = {
    0x00, 0x00, 0x00, 0x98, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x04, 0x00, 0x10, 0x63, 0x6f,
    0x6e, 0x73, 0x6f, 0x6c, 0x65, 0x2d, 0x70, 0x72, 0x6f, 0x64, 0x75, 0x63, 0x65, 0x72, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x00, 0x05, 0xdc, 0x02, 0x12, 0x71, 0x75, 0x69, 0x63, 0x6b, 0x73, 0x74, 0x61,
    0x72, 0x74, 0x2d, 0x65, 0x76, 0x65, 0x6e, 0x74, 0x73, 0x02, 0x00, 0x00, 0x00, 0x00, 0x5b, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4e, 0xff, 0xff, 0xff, 0xff, 0x02,
    0xc0, 0xde, 0x91, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x7a, 0x1b, 0xc8,
    0x2d, 0xaa, 0x00, 0x00, 0x01, 0x7a, 0x1b, 0xc8, 0x2d, 0xaa, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x01, 0x38, 0x00, 0x00, 0x00,
    0x01, 0x2c, 0x54, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x6d, 0x79, 0x20, 0x66, 0x69, 0x72,
    0x73, 0x74, 0x20, 0x65, 0x76, 0x65, 0x6e, 0x74, 0x00, 0x00, 0x00, 0x00};

constexpr uint8_t kProduceResponse[] = {
    0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x04, 0x00, 0x02, 0x12, 0x71, 0x75, 0x69,
    0x63, 0x6b, 0x73, 0x74, 0x61, 0x72, 0x74, 0x2d, 0x65, 0x76, 0x65, 0x6e, 0x74, 0x73,
    0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// APIKey: 3, APIVersion: 11
constexpr uint8_t kMetaDataRequest[] = {
    0x00, 0x00, 0x00, 0x1c, 0x00, 0x03, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x01, 0x00, 0x0d, 0x61, 0x64,
    0x6d, 0x69, 0x6e, 0x63, 0x6c, 0x69, 0x65, 0x6e, 0x74, 0x2d, 0x31, 0x00, 0x01, 0x01, 0x00, 0x00};

constexpr uint8_t kMetaDataResponse[] = {
    0x00, 0x00, 0x00, 0x3b, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
    0x00, 0x00, 0x0a, 0x6c, 0x6f, 0x63, 0x61, 0x6c, 0x68, 0x6f, 0x73, 0x74, 0x00, 0x00, 0x23, 0x84,
    0x00, 0x00, 0x17, 0x5a, 0x65, 0x76, 0x76, 0x4e, 0x66, 0x47, 0x45, 0x52, 0x30, 0x4f, 0x73, 0x51,
    0x4d, 0x34, 0x77, 0x71, 0x48, 0x5f, 0x6f, 0x75, 0x77, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00};

// APIKey: 18, APIVersion: 3
constexpr uint8_t kAPIVersionRequest[] = {
    0x00, 0x00, 0x00, 0x31, 0x00, 0x12, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x0d,
    0x61, 0x64, 0x6d, 0x69, 0x6e, 0x63, 0x6c, 0x69, 0x65, 0x6e, 0x74, 0x2d, 0x31, 0x00,
    0x12, 0x61, 0x70, 0x61, 0x63, 0x68, 0x65, 0x2d, 0x6b, 0x61, 0x66, 0x6b, 0x61, 0x2d,
    0x6a, 0x61, 0x76, 0x61, 0x06, 0x32, 0x2e, 0x38, 0x2e, 0x30, 0x00};

constexpr uint8_t kAPIVersionResponse[] = {
    0x00, 0x00, 0x01, 0x9e, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x39, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x09, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x06, 0x00,
    0x00, 0x03, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x05,
    0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x07, 0x00, 0x00,
    0x00, 0x03, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x07,
    0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00,
    0x0c, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x0e, 0x00,
    0x00, 0x00, 0x05, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x03, 0x00,
    0x00, 0x13, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x15,
    0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x16, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x17, 0x00, 0x00,
    0x00, 0x04, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x19, 0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x1a, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x1b, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
    0x1c, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x1d, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x1e, 0x00,
    0x00, 0x00, 0x02, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x21, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00, 0x02, 0x00,
    0x00, 0x23, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x25,
    0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x26, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x27, 0x00, 0x00,
    0x00, 0x02, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x29, 0x00, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x2a, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x2b, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
    0x2c, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x2d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2e, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x2f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x31, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x39,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3d, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00};

template <size_t N>
Packet GenPacket(MessageType type, const uint8_t (&raw_packet)[N], int timestamp,
                 int correlation_id) {
  Packet result;
  State state = {{correlation_id}};
  auto packet_view = CreateStringView<char>(CharArrayStringView<uint8_t>(raw_packet));
  ParseState parse_state = ParseFrame(type, &packet_view, &result, &state);
  EXPECT_EQ(parse_state, ParseState::kSuccess);

  result.timestamp_ns = timestamp;
  return result;
}

const Packet kProduceReqPacket = GenPacket(MessageType::kRequest, kProduceRequest, 0, 4);
const Packet kProduceRespPacket = GenPacket(MessageType::kResponse, kProduceResponse, 1, 4);
const Packet kMetaDataReqPacket = GenPacket(MessageType::kRequest, kMetaDataRequest, 2, 1);
const Packet kMetaDataRespPacket = GenPacket(MessageType::kResponse, kMetaDataResponse, 3, 1);
const Packet kAPIVersionReqPacket = GenPacket(MessageType::kRequest, kAPIVersionRequest, 4, 2);
const Packet kAPIVersionRespPacket = GenPacket(MessageType::kResponse, kAPIVersionResponse, 5, 2);

}  // namespace testdata
}  // namespace kafka
}  // namespace protocols
}  // namespace stirling
}  // namespace px
