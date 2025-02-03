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

#include <atomic>
#include <memory>
#include <string>

namespace vrs {

using std::string;

/// Context description for telemetry events.
struct OperationContext {
  string operation;
  string sourceLocation;

  OperationContext() = default;
  OperationContext(string op, string sourceLoc)
      : operation{std::move(op)}, sourceLocation{std::move(sourceLoc)} {}
  OperationContext(const OperationContext& rhs) = default;
  OperationContext(OperationContext&& rhs) noexcept
      : operation{std::move(rhs.operation)}, sourceLocation{std::move(rhs.sourceLocation)} {}

  bool operator<(const OperationContext& rhs) const {
    auto tie = [](const OperationContext& oc) { return std::tie(oc.operation, oc.sourceLocation); };
    return tie(*this) < tie(rhs);
  }
  OperationContext& operator=(OperationContext&& rhs) noexcept {
    operation = std::move(rhs.operation);
    sourceLocation = std::move(rhs.sourceLocation);
    return *this;
  }
};

/// General purpose telemetry event.
struct LogEvent {
  LogEvent() = default;
  LogEvent(std::string type, OperationContext opContext, std::string message, string serverReply)
      : type{std::move(type)},
        operationContext{std::move(opContext)},
        message{std::move(message)},
        serverReply{std::move(serverReply)} {}
  LogEvent(LogEvent&& rhs) noexcept
      : type{std::move(rhs.type)},
        operationContext{std::move(rhs.operationContext)},
        message{std::move(rhs.message)},
        serverReply{std::move(rhs.serverReply)} {}

  LogEvent& operator=(LogEvent&& rhs) noexcept {
    type = std::move(rhs.type);
    operationContext = std::move(rhs.operationContext);
    message = std::move(rhs.message);
    serverReply = std::move(rhs.serverReply);
    return *this;
  }

  std::string type;
  OperationContext operationContext;
  std::string message;
  std::string serverReply;
};

/// \brief Telemetry event specialized to report cloud traffic
///
/// A key goal of telemetry is to monitor traffic to cloud storage solutions, so we can measure
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
  TrafficEvent& setAttemptStartTime();
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

  static constexpr const char* kErrorType = "error";
  static constexpr const char* kWarningType = "warning";
  static constexpr const char* kInfoType = "info";

  /// set logger and get back the previous one, making sure the assignment is performed
  /// before the previous logger is deleted.
  static void setLogger(std::unique_ptr<TelemetryLogger>&& telemetryLogger);

  /// methods for clients to use without having to get an instance, etc
  static inline void error(
      const OperationContext& operationContext,
      const string& message,
      const string& serverMessage = {}) {
    getInstance()->logEvent(LogEvent(kErrorType, operationContext, message, serverMessage));
  }
  static inline void warning(
      const OperationContext& operationContext,
      const string& message,
      const string& serverMessage = {}) {
    getInstance()->logEvent(LogEvent(kWarningType, operationContext, message, serverMessage));
  }
  static inline void info(
      const OperationContext& operationContext,
      const string& message,
      const string& serverMessage = {}) {
    getInstance()->logEvent(LogEvent(kInfoType, operationContext, message, serverMessage));
  }
  static inline void event(
      const std::string& eventType,
      const OperationContext& operationContext,
      const string& message,
      const string& serverMessage = {}) {
    getInstance()->logEvent(LogEvent(eventType, operationContext, message, serverMessage));
  }
  static inline void traffic(const OperationContext& operationContext, const TrafficEvent& event) {
    getInstance()->logTraffic(operationContext, event);
  }
  static inline void flush() {
    getInstance()->flushEvents();
  }

  /// Actual methods that implement the behaviors
  virtual void logEvent(LogEvent&& event);
  virtual void logTraffic(const OperationContext& operationContext, const TrafficEvent& event);
  virtual void flushEvents() {}

  /// End telemetry: All background threads should be stopped, and all pending events should be
  /// flushed, and further events should be ignored.
  virtual void stop() {}

 private:
  static std::atomic<TelemetryLogger*>& instance();
  static inline TelemetryLogger* getInstance() {
    return instance().load(std::memory_order_relaxed);
  }
};

} // namespace vrs
