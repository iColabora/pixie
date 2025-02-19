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

#include <sys/socket.h>

#include <memory>
#include <string>

#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/socket_trace.hpp"
#include "src/stirling/source_connectors/socket_tracer/testing/clock.h"

namespace px {
namespace stirling {
namespace testing {

constexpr uint32_t kPID = 12345;
constexpr uint32_t kFD = 3;
constexpr uint64_t kPIDStartTimeTicks = 112358;

// Convenience functions and predefined data for generating events expected from BPF socket probes.
class EventGenerator {
 public:
  explicit EventGenerator(Clock* clock, uint32_t pid = kPID, int32_t fd = kFD)
      : clock_(clock), pid_(pid), fd_(fd) {}

  struct socket_control_event_t InitConn(EndpointRole role = kRoleUnknown) {
    struct socket_control_event_t conn_event {};
    conn_event.type = kConnOpen;
    conn_event.timestamp_ns = clock_->now();
    conn_event.conn_id.upid.pid = pid_;
    conn_event.conn_id.fd = fd_;
    conn_event.conn_id.tsid = ++tsid_;
    conn_event.conn_id.upid.start_time_ticks = kPIDStartTimeTicks;
    conn_event.open.addr.sa.sa_family = AF_INET;
    conn_event.open.role = role;
    return conn_event;
  }

  template <TrafficProtocol TProtocol, EndpointRole TRole = kRoleClient>
  std::unique_ptr<SocketDataEvent> InitSendEvent(std::string_view msg) {
    return InitDataEvent<TProtocol, TRole>(TrafficDirection::kEgress, &send_pos_, msg);
  }

  template <TrafficProtocol TProtocol, EndpointRole TRole = kRoleClient>
  std::unique_ptr<SocketDataEvent> InitRecvEvent(std::string_view msg) {
    return InitDataEvent<TProtocol, TRole>(TrafficDirection::kIngress, &recv_pos_, msg);
  }

  template <TrafficProtocol TProtocol, EndpointRole TRole>
  std::unique_ptr<SocketDataEvent> InitDataEvent(TrafficDirection direction, uint64_t* pos,
                                                 std::string_view msg) {
    socket_data_event_t event = {};
    event.attr.direction = direction;
    event.attr.protocol = TProtocol;
    event.attr.role = TRole;
    event.attr.timestamp_ns = clock_->now();
    event.attr.conn_id.upid.pid = pid_;
    event.attr.conn_id.fd = fd_;
    event.attr.conn_id.tsid = tsid_;
    event.attr.conn_id.upid.start_time_ticks = kPIDStartTimeTicks;
    event.attr.pos = *pos;
    event.attr.msg_size = msg.size();
    event.attr.msg_buf_size = msg.size();
    msg.copy(event.msg, msg.size());

    *pos += msg.size();

    return std::make_unique<SocketDataEvent>(&event);
  }

  std::unique_ptr<SocketDataEvent> InitSendEvent(TrafficProtocol protocol, EndpointRole role,
                                                 std::string_view msg) {
    auto res =
        InitDataEvent<kProtocolUnknown, kRoleUnknown>(TrafficDirection::kEgress, &send_pos_, msg);
    res->attr.protocol = protocol;
    res->attr.role = role;
    return res;
  }

  std::unique_ptr<SocketDataEvent> InitRecvEvent(TrafficProtocol protocol, EndpointRole role,
                                                 std::string_view msg) {
    auto res =
        InitDataEvent<kProtocolUnknown, kRoleUnknown>(TrafficDirection::kIngress, &recv_pos_, msg);
    res->attr.protocol = protocol;
    res->attr.role = role;
    return res;
  }

  socket_control_event_t InitClose() {
    struct socket_control_event_t close_event {};
    close_event.type = kConnClose;
    close_event.timestamp_ns = clock_->now();
    close_event.conn_id.upid.pid = pid_;
    close_event.conn_id.fd = fd_;
    close_event.conn_id.tsid = tsid_;
    close_event.conn_id.upid.start_time_ticks = kPIDStartTimeTicks;
    close_event.close.rd_bytes = recv_pos_;
    close_event.close.wr_bytes = send_pos_;
    return close_event;
  }

 private:
  Clock* clock_;
  uint32_t pid_ = 0;
  int32_t fd_ = 0;
  uint64_t tsid_ = 0;
  uint64_t send_pos_ = 0;
  uint64_t recv_pos_ = 0;
};

constexpr std::string_view kHTTPReq0 =
    "GET /index.html HTTP/1.1\r\n"
    "Host: www.pixielabs.ai\r\n"
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64)\r\n"
    "\r\n";

constexpr std::string_view kHTTPResp0 =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: application/json; charset=utf-8\r\n"
    "Content-Length: 5\r\n"
    "\r\n"
    "pixie";

constexpr std::string_view kHTTPReq1 =
    "GET /foo.html HTTP/1.1\r\n"
    "Host: www.pixielabs.ai\r\n"
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64)\r\n"
    "\r\n";

constexpr std::string_view kHTTPResp1 =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: application/json; charset=utf-8\r\n"
    "Content-Length: 3\r\n"
    "\r\n"
    "foo";

constexpr std::string_view kHTTPReq2 =
    "GET /bar.html HTTP/1.1\r\n"
    "Host: www.pixielabs.ai\r\n"
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64)\r\n"
    "\r\n";

constexpr std::string_view kHTTPResp2 =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: application/json; charset=utf-8\r\n"
    "Content-Length: 3\r\n"
    "\r\n"
    "bar";

constexpr std::string_view kHTTPUpgradeReq =
    "GET /index.html HTTP/1.1\r\n"
    "Host: www.pixielabs.ai\r\n"
    "Connection: Upgrade\r\n"
    "Upgrade: websocket\r\n"
    "\r\n";

constexpr std::string_view kHTTPUpgradeResp =
    "HTTP/1.1 101 Switching Protocols\r\n"
    "Upgrade: websocket\r\n"
    "Connection: Upgrade\r\n"
    "\r\n";

static constexpr std::string_view kHTTP2EndStreamHeadersFrame =
    ConstStringView("\x0\x0\x0\x1\x5\x0\x0\x0\x1");
static constexpr std::string_view kHTTP2EndStreamDataFrame =
    ConstStringView("\x0\x0\x0\x0\x1\x0\x0\x0\x1");

void SetIPv4RemoteAddr(struct socket_control_event_t* conn, std::string_view addr_str,
                       uint16_t port = 123) {
  // Set an address that falls in the intra-cluster address range.
  conn->open.addr.in4.sin_family = AF_INET;
  conn->open.addr.in4.sin_port = htons(port);
  // Note that address is outside of the CIDR block specified below.
  PL_CHECK_OK(ParseIPv4Addr(addr_str, &conn->open.addr.in4.sin_addr));
}

void SetIPv6RemoteAddr(struct socket_control_event_t* conn, std::string_view addr_str,
                       uint16_t port = 123) {
  // Set an address that falls in the intra-cluster address range.
  conn->open.addr.in6.sin6_family = AF_INET6;
  conn->open.addr.in6.sin6_port = htons(port);
  // Note that address is outside of the CIDR block specified below.
  PL_CHECK_OK(ParseIPv6Addr(addr_str, &conn->open.addr.in6.sin6_addr));
}

}  // namespace testing
}  // namespace stirling
}  // namespace px
