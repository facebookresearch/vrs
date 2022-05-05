/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
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
 */

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <vrs/os/Time.h>

namespace vrs {

using std::string;

/// Context description for telemetry events.
struct OperationContext {
  string operation;
  string sourceLocation;

  OperationContext() {}
  OperationContext(const string& op, const string& sourceLoc)
      : operation{op}, sourceLocation{sourceLoc} {}
  OperationContext(const OperationContext& rhs)
      : operation{rhs.operation}, sourceLocation{rhs.sourceLocation} {}
  OperationContext(OperationContext&& rhs)
      : operation{move(rhs.operation)}, sourceLocation{move(rhs.sourceLocation)} {}

  bool operator<(const OperationContext& rhs) const {
    return operation < rhs.operation ||
        (operation == rhs.operation && sourceLocation < rhs.sourceLocation);
  }
  OperationContext& operator=(OperationContext&& rhs) {
    operation = move(rhs.operation);
    sourceLocation = move(rhs.sourceLocation);
    return *this;
  }
};

/// General purpose telemetry event.
struct LogEvent {
  LogEvent() = default;
  LogEvent(
      const std::string& type,
      const OperationContext& opContext,
      const std::string& message,
      const string& serverReply)
      : type{type}, operationContext{opContext}, message{message}, serverReply{serverReply} {}
  LogEvent(LogEvent&& rhs)
      : type{move(rhs.type)},
        operationContext{std::move(rhs.operationContext)},
        message{move(rhs.message)},
        serverReply{move(rhs.serverReply)} {}

  LogEvent& operator=(LogEvent&& rhs) {
    type = move(rhs.type);
    operationContext = std::move(rhs.operationContext);
    message = move(rhs.message);
    serverReply = move(rhs.serverReply);
    return *this;
  }

  std::string type;
  OperationContext operationContext;
  std::string message;
  std::string serverReply;
};

/// \brief Telemetry event specialized to report cloud traffic
///
/// A key goal of telemetry is to monitor traffic to cloud storage solutions, so we can mesure
/// resource usage and detect excessive traffic. This requires logging every network transaction,
/// as opposed to sparse events, giving leverage to custom implementation optimizations not possible
/// with a generic event.
struct TrafficEvent {
  bool isSuccess = false;
  bool uploadNotDownload = false;
  int64_t transferStartTime = 0; // start time
  int64_t totalDurationMs = -1; // overall request duration, including retries
  int64_t transferDurationMs = -1; // last network transfer duration (last attempt)
  size_t transferOffset = 0; // offset to read from
  size_t transferRequestSize = 0; // bytes
  size_t transferSize = 0; // bytes
  size_t retryCount = 0;
  size_t errorCount = 0;
  size_t error429Count = 0;
  long httpStatus = -1;
  string serverName;

  TrafficEvent& setIsSuccess(bool success) {
    isSuccess = success;
    return *this;
  }
  TrafficEvent& setIsUpload() {
    uploadNotDownload = true;
    return *this;
  }
  TrafficEvent& setIsDownload() {
    uploadNotDownload = false;
    return *this;
  }
  TrafficEvent& setAttemptStartTime() {
    transferStartTime = os::getCurrentTimeSecSinceEpoch();
    return *this;
  }
  TrafficEvent& setTotalDurationMs(int64_t durationMs) {
    totalDurationMs = durationMs;
    return *this;
  }
  TrafficEvent& setTransferDurationMs(int64_t aTransferDurationMs) {
    transferDurationMs = aTransferDurationMs;
    return *this;
  }
  TrafficEvent& setTransferOffset(size_t offset) {
    transferOffset = offset;
    return *this;
  }
  TrafficEvent& setTransferRequestSize(size_t aTransferRequestSize) {
    transferRequestSize = aTransferRequestSize;
    return *this;
  }
  TrafficEvent& setTransferSize(size_t aTransferSize) {
    transferSize = aTransferSize;
    return *this;
  }
  TrafficEvent& setRetryCount(size_t aRetryCount) {
    retryCount = aRetryCount;
    return *this;
  }
  TrafficEvent& setError429Count(size_t anError429Count) {
    error429Count = anError429Count;
    return *this;
  }
  TrafficEvent& setErrorCount(size_t anErrorCount) {
    errorCount = anErrorCount;
    return *this;
  }
  TrafficEvent& setHttpStatus(long status) {
    httpStatus = status;
    return *this;
  }
  TrafficEvent& setUrl(const string& aServerName);
};

/// \brief TelemetryLogger to report important events
///
/// Telemetry building block infra to report events from VRS operations.
/// The default implementation simply logs using XR_LOGI and XR_LOGE, XR_LOGW,
/// but can easily be augmented to implement telemetry in a central database.
class TelemetryLogger {
 public:
  virtual ~TelemetryLogger();

  static TelemetryLogger& getInstance();

  static constexpr const char* kErrorType = "error";
  static constexpr const char* kWarningType = "warning";
  static constexpr const char* kInfoType = "info";

  /// set logger and get back the previous one, making sure the assignment is performed
  /// before the previous logger is deleted.
  static std::unique_ptr<TelemetryLogger> setLogger(
      std::unique_ptr<TelemetryLogger>&& telemetryLogger);

  /// methods for clients to use without having to get an instance, etc
  static inline void error(
      const OperationContext& operationContext,
      const string& message,
      const string& serverMessage = {}) {
    static const string sErrorType{kErrorType};
    getStaticInstance()->logEvent(LogEvent(sErrorType, operationContext, message, serverMessage));
  }
  static inline void warning(
      const OperationContext& operationContext,
      const string& message,
      const string& serverMessage = {}) {
    static const string sWarningType{kWarningType};
    getStaticInstance()->logEvent(LogEvent(sWarningType, operationContext, message, serverMessage));
  }
  static inline void info(
      const OperationContext& operationContext,
      const string& message,
      const string& serverMessage = {}) {
    static const string sInfoType{kInfoType};
    getStaticInstance()->logEvent(LogEvent(sInfoType, operationContext, message, serverMessage));
  }
  static inline void traffic(const OperationContext& operationContext, const TrafficEvent& event) {
    getStaticInstance()->logTraffic(operationContext, event);
  }

  /// Actuall methods that implement the behaviors
  virtual void logEvent(LogEvent&& event);
  virtual void logTraffic(const OperationContext& operationContext, const TrafficEvent& event);

 private:
  static std::unique_ptr<TelemetryLogger>& getStaticInstance();
};

} // namespace vrs
