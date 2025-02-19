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

#include "src/stirling/source_connectors/socket_tracer/protocols/cql/stitcher.h"

#include <deque>
#include <string>
#include <utility>

#include "src/common/base/base.h"
#include "src/common/json/json.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/cql/frame_body_decoder.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/cql/types.h"

namespace px {
namespace stirling {
namespace protocols {
namespace cass {

using ::px::utils::ToJSONString;

namespace {
std::string BytesToString(std::basic_string_view<uint8_t> x) {
  return px::BytesToString<bytes_format::HexCompact>(CreateStringView<char>(x));
}
}  // namespace

Status ProcessStartupReq(Frame* req_frame, Request* req) {
  PL_ASSIGN_OR_RETURN(StartupReq r, ParseStartupReq(req_frame));

  DCHECK(req->msg.empty());
  req->msg = ToJSONString(r.options);

  return Status::OK();
}

Status ProcessAuthResponseReq(Frame* req_frame, Request* req) {
  PL_ASSIGN_OR_RETURN(AuthResponseReq r, ParseAuthResponseReq(req_frame));

  std::string_view token_str = CreateStringView<char>(r.token);

  DCHECK(req->msg.empty());
  req->msg = token_str;

  return Status::OK();
}

Status ProcessOptionsReq(Frame* req_frame, Request* req) {
  PL_ASSIGN_OR_RETURN(OptionsReq r, ParseOptionsReq(req_frame));

  PL_UNUSED(r);
  DCHECK(req->msg.empty());

  return Status::OK();
}

Status ProcessRegisterReq(Frame* req_frame, Request* req) {
  PL_ASSIGN_OR_RETURN(RegisterReq r, ParseRegisterReq(req_frame));

  DCHECK(req->msg.empty());
  req->msg = ToJSONString(r.event_types);

  return Status::OK();
}

Status ProcessQueryReq(Frame* req_frame, Request* req) {
  PL_ASSIGN_OR_RETURN(QueryReq r, ParseQueryReq(req_frame));

  // TODO(oazizi): This is just a placeholder.
  // Real implementation should figure out what type each value is, and cast into the appropriate
  // type. This, however, will be hard unless we have observed the preceding Prepare request.
  std::vector<std::string> hex_values;
  for (const auto& value_i : r.qp.values) {
    hex_values.push_back(BytesToString(value_i.value));
  }

  DCHECK(req->msg.empty());
  req->msg = r.query;

  // For now, just tag the parameter values to the end.
  if (!hex_values.empty()) {
    absl::StrAppend(&req->msg, "\n");
    absl::StrAppend(&req->msg, ToJSONString(hex_values));
  }

  return Status::OK();
}

Status ProcessPrepareReq(Frame* req_frame, Request* req) {
  PL_ASSIGN_OR_RETURN(PrepareReq r, ParsePrepareReq(req_frame));

  DCHECK(req->msg.empty());
  req->msg = r.query;

  return Status::OK();
}

Status ProcessExecuteReq(Frame* req_frame, Request* req) {
  PL_ASSIGN_OR_RETURN(ExecuteReq r, ParseExecuteReq(req_frame));

  // TODO(oazizi): This is just a placeholder.
  // Real implementation should figure out what type each value is, and cast into the appropriate
  // type. This, however, will be hard unless we have observed the preceding Prepare request.
  std::vector<std::string> hex_values;
  for (const auto& value_i : r.qp.values) {
    hex_values.push_back(BytesToString(value_i.value));
  }

  DCHECK(req->msg.empty());
  req->msg = ToJSONString(hex_values);

  return Status::OK();
}

Status ProcessBatchReq(Frame* req_frame, Request* req) {
  PL_ASSIGN_OR_RETURN(BatchReq r, ParseBatchReq(req_frame));

  // TODO(oazizi): Should we add other fields?

  // Copy to vector so we can use ToJSONString().
  // TODO(oazizi): Find a cleaner way that avoids the copying.
  std::vector<std::pair<std::string, std::string>> tmp;
  for (const auto& q : r.queries) {
    switch (q.kind) {
      case BatchQueryKind::kString:
        tmp.push_back({"query", std::move(std::get<std::string>(q.query_or_id))});
        break;
      case BatchQueryKind::kID:
        tmp.push_back({"id", BytesToString(std::get<std::basic_string<uint8_t>>(q.query_or_id))});
        break;
      default:
        LOG(DFATAL) << absl::Substitute("Unrecognized BatchQueryKind $0", static_cast<int>(q.kind));
    }
  }
  req->msg = ToJSONString(tmp);

  return Status::OK();
}

Status ProcessErrorResp(Frame* resp_frame, Response* resp) {
  PL_ASSIGN_OR_RETURN(ErrorResp r, ParseErrorResp(resp_frame));

  DCHECK(resp->msg.empty());
  resp->msg = absl::Substitute("[$0] $1", r.error_code, r.error_msg);

  return Status::OK();
}

Status ProcessReadyResp(Frame* resp_frame, Response* resp) {
  PL_ASSIGN_OR_RETURN(ReadyResp r, ParseReadyResp(resp_frame));

  PL_UNUSED(r);
  DCHECK(resp->msg.empty());

  return Status::OK();
}

Status ProcessSupportedResp(Frame* resp_frame, Response* resp) {
  PL_ASSIGN_OR_RETURN(SupportedResp r, ParseSupportedResp(resp_frame));

  DCHECK(resp->msg.empty());
  resp->msg = ToJSONString(r.options);

  return Status::OK();
}

Status ProcessAuthenticateResp(Frame* resp_frame, Response* resp) {
  PL_ASSIGN_OR_RETURN(AuthenticateResp r, ParseAuthenticateResp(resp_frame));

  DCHECK(resp->msg.empty());
  resp->msg = std::move(r.authenticator_name);

  return Status::OK();
}

Status ProcessAuthSuccessResp(Frame* resp_frame, Response* resp) {
  PL_ASSIGN_OR_RETURN(AuthSuccessResp r, ParseAuthSuccessResp(resp_frame));

  std::string token_hex = BytesToString(r.token);

  DCHECK(resp->msg.empty());
  resp->msg = token_hex;

  return Status::OK();
}

Status ProcessAuthChallengeResp(Frame* resp_frame, Response* resp) {
  PL_ASSIGN_OR_RETURN(AuthChallengeResp r, ParseAuthChallengeResp(resp_frame));

  std::string token_hex = BytesToString(r.token);

  DCHECK(resp->msg.empty());
  resp->msg = token_hex;

  return Status::OK();
}

Status ProcessResultResp(Frame* resp_frame, Response* resp) {
  PL_ASSIGN_OR_RETURN(ResultResp r, ParseResultResp(resp_frame));

  DCHECK(resp->msg.empty());

  switch (r.kind) {
    case ResultRespKind::kVoid: {
      resp->msg = "Response type = VOID";
      break;
    }
    case ResultRespKind::kRows: {
      const auto& r_resp = std::get<ResultRowsResp>(r.resp);

      // Copy to vector so we can use ToJSONString().
      // TODO(oazizi): Find a cleaner way. This is temporary anyways.

      std::vector<std::string> names;
      for (auto& c : r_resp.metadata.col_specs) {
        names.push_back(std::move(c.name));
      }

      resp->msg = absl::StrCat("Response type = ROWS\n",
                               "Number of columns = ", r_resp.metadata.columns_count, "\n",
                               ToJSONString(names), "\n", "Number of rows = ", r_resp.rows_count);
      // TODO(oazizi): Consider which other parts of metadata would be interesting to record into
      // resp.
      break;
    }
    case ResultRespKind::kSetKeyspace: {
      const auto& r_resp = std::get<ResultSetKeyspaceResp>(r.resp);
      resp->msg =
          absl::StrCat("Response type = SET_KEYSPACE\n", "Keyspace = ", r_resp.keyspace_name);
      break;
    }
    case ResultRespKind::kPrepared: {
      resp->msg = "Response type = PREPARED";
      // TODO(oazizi): Add more information.
      break;
    }
    case ResultRespKind::kSchemaChange: {
      resp->msg = "Response type = SCHEMA_CHANGE";
      // TODO(oazizi): Add more information.
      break;
    }
    default:
      LOG(DFATAL) << absl::Substitute("Unrecognized ResultRespKind $0", static_cast<int>(r.kind));
  }

  return Status::OK();
}

Status ProcessEventResp(Frame* resp_frame, Response* resp) {
  PL_ASSIGN_OR_RETURN(EventResp r, ParseEventResp(resp_frame));

  if (r.event_type == "TOPOLOGY_CHANGE" || r.event_type == "STATUS_CHANGE") {
    DCHECK(resp->msg.empty());
    resp->msg = absl::StrCat(r.event_type, " ", r.change_type, " ", r.addr.AddrStr());

    return Status::OK();
  } else if (r.event_type == "SCHEMA_CHANGE") {
    DCHECK(resp->msg.empty());
    resp->msg = absl::StrCat(r.event_type, " ", r.sc.change_type, " keyspace=", r.sc.keyspace,
                             " name=", r.sc.name);
    // TODO(oazizi): Add sc.arg_types to the response string.

    return Status::OK();
  } else {
    return error::Internal("Unknown event_type $0", r.event_type);
  }
}

Status ProcessReq(Frame* req_frame, Request* req) {
  req->op = static_cast<ReqOp>(req_frame->hdr.opcode);
  req->timestamp_ns = req_frame->timestamp_ns;

  switch (req->op) {
    case ReqOp::kStartup:
      return ProcessStartupReq(req_frame, req);
    case ReqOp::kAuthResponse:
      return ProcessAuthResponseReq(req_frame, req);
    case ReqOp::kOptions:
      return ProcessOptionsReq(req_frame, req);
    case ReqOp::kQuery:
      return ProcessQueryReq(req_frame, req);
    case ReqOp::kPrepare:
      return ProcessPrepareReq(req_frame, req);
    case ReqOp::kExecute:
      return ProcessExecuteReq(req_frame, req);
    case ReqOp::kBatch:
      return ProcessBatchReq(req_frame, req);
    case ReqOp::kRegister:
      return ProcessRegisterReq(req_frame, req);
    default:
      return error::Internal("Unhandled opcode $0", magic_enum::enum_name(req->op));
  }
}

Status ProcessResp(Frame* resp_frame, Response* resp) {
  resp->op = static_cast<RespOp>(resp_frame->hdr.opcode);
  resp->timestamp_ns = resp_frame->timestamp_ns;

  switch (resp->op) {
    case RespOp::kError:
      return ProcessErrorResp(resp_frame, resp);
    case RespOp::kReady:
      return ProcessReadyResp(resp_frame, resp);
    case RespOp::kAuthenticate:
      return ProcessAuthenticateResp(resp_frame, resp);
    case RespOp::kSupported:
      return ProcessSupportedResp(resp_frame, resp);
    case RespOp::kResult:
      return ProcessResultResp(resp_frame, resp);
    case RespOp::kEvent:
      return ProcessEventResp(resp_frame, resp);
    case RespOp::kAuthChallenge:
      return ProcessAuthChallengeResp(resp_frame, resp);
    case RespOp::kAuthSuccess:
      return ProcessAuthSuccessResp(resp_frame, resp);
    default:
      return error::Internal("Unhandled opcode $0", magic_enum::enum_name(resp->op));
  }
}

StatusOr<Record> ProcessReqRespPair(Frame* req_frame, Frame* resp_frame) {
  ECHECK_LT(req_frame->timestamp_ns, resp_frame->timestamp_ns);

  Record r;
  PL_RETURN_IF_ERROR(ProcessReq(req_frame, &r.req));
  PL_RETURN_IF_ERROR(ProcessResp(resp_frame, &r.resp));

  return r;
}

StatusOr<Record> ProcessSolitaryResp(Frame* resp_frame) {
  Record r;

  // For now, Event is the only supported solitary response.
  // If this ever changes, the code below needs to be adapted.
  DCHECK(resp_frame->hdr.opcode == Opcode::kEvent);

  // Make a fake request to go along with the response.
  // - Use REGISTER op, since that was what set up the events in the first place.
  // - Use response timestamp, so any calculated latencies are reported as 0.
  r.req.op = ReqOp::kRegister;
  r.req.msg = "-";
  r.req.timestamp_ns = resp_frame->timestamp_ns;

  // A little inefficient because it will go through a switch statement again,
  // when we actually know the op. But keep it this way for consistency.
  PL_RETURN_IF_ERROR(ProcessResp(resp_frame, &r.resp));

  return r;
}

// Currently StitchFrames() uses a response-led matching algorithm.
// For each response that is at the head of the deque, there should exist a previous request with
// the same stream. Find it, and consume both frames.
// TODO(oazizi): Does it make sense to sort to help the matching?
// Considerations:
//  - Request and response deques are likely (confirm?) to be mostly ordered.
//  - Stream values can be re-used, so sorting would have to consider times too.
//  - Stream values need not be in any sequential order.
RecordsWithErrorCount<Record> StitchFrames(std::deque<Frame>* req_frames,
                                           std::deque<Frame>* resp_frames) {
  std::vector<Record> entries;
  int error_count = 0;

  for (auto iter = resp_frames->begin(); iter != resp_frames->end();
       iter = resp_frames->erase(resp_frames->begin())) {
    bool found_match = false;
    Frame resp_frame = *iter;

    // Event responses are special: they have no request.
    if (resp_frame.hdr.opcode == Opcode::kEvent) {
      StatusOr<Record> record_status = ProcessSolitaryResp(&resp_frame);
      if (record_status.ok()) {
        entries.push_back(record_status.ConsumeValueOrDie());
      } else {
        VLOG(1) << record_status.msg();
        ++error_count;
      }
      continue;
    }

    // Search for matching req frame
    for (auto& req_frame : *req_frames) {
      if (resp_frame.hdr.stream == req_frame.hdr.stream) {
        VLOG(2) << absl::Substitute("req_op=$0 msg=$1", magic_enum::enum_name(req_frame.hdr.opcode),
                                    req_frame.msg);

        StatusOr<Record> record_status = ProcessReqRespPair(&req_frame, &resp_frame);
        if (record_status.ok()) {
          entries.push_back(record_status.ConsumeValueOrDie());
        } else {
          VLOG(1) << record_status.ToString();
          ++error_count;
        }

        // Found a match, so remove both request and response.
        // We don't remove request frames on the fly, however,
        // because it could otherwise cause unnecessary churn/copying in the deque.
        // This is due to the fact that responses can come out-of-order.
        // Just mark the request as consumed, and clean-up when they reach the head of the queue.
        // Note that responses are always head-processed, so they don't require this optimization.
        found_match = true;
        req_frame.consumed = true;
        break;
      }
    }

    if (!found_match) {
      VLOG(1) << absl::Substitute("Did not find a request matching the response. Stream = $0",
                                  resp_frame.hdr.stream);
      ++error_count;
    }

    // Clean-up consumed frames at the head.
    // Do this inside the resp loop to aggressively clean-out req_frames whenever a frame consumed.
    // Should speed up the req_frames search for the next iteration.
    for (auto& req_frame : *req_frames) {
      if (!req_frame.consumed) {
        break;
      }
      req_frames->pop_front();
    }

    // TODO(oazizi): Consider removing requests that are too old, otherwise a lost response can mean
    // the are never processed. This would result in a memory leak until the more drastic connection
    // tracker clean-up mechanisms kick in.
  }

  return {entries, error_count};
}

}  // namespace cass
}  // namespace protocols
}  // namespace stirling
}  // namespace px
