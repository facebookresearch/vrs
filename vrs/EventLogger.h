// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <vrs/os/Time.h>

namespace vrs {

using std::string;

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

struct LogEvent {
  LogEvent() = default;
  LogEvent(
      const std::string& type,
      const OperationContext& opContext,
      const std::string& message,
      const string& serverMessage)
      : type{type}, operationContext{opContext}, message{message}, serverMessage{serverMessage} {}
  LogEvent(LogEvent&& rhs)
      : type{move(rhs.type)},
        operationContext{std::move(rhs.operationContext)},
        message{move(rhs.message)},
        serverMessage{move(rhs.serverMessage)} {}

  LogEvent& operator=(LogEvent&& rhs) {
    type = move(rhs.type);
    operationContext = std::move(rhs.operationContext);
    message = move(rhs.message);
    serverMessage = move(rhs.serverMessage);
    return *this;
  }

  std::string type;
  OperationContext operationContext;
  std::string message;
  std::string serverMessage;
};

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

/// The EventLogger class
/// Helper class to log events for VRS operations.
/// By default, logs to use XR_LOGI and XR_LOGE, XR_LOGW, but can
/// be easily overwritten to log anywhere.
class EventLogger {
 public:
  virtual ~EventLogger();
  static EventLogger& getInstance();

  static constexpr const char* kErrorType = "error";
  static constexpr const char* kWarningType = "warning";

  /// set logger and get back the previous one, making sure the assignment is performed
  /// before the previous logger is deleted.
  static std::unique_ptr<EventLogger> setLogger(std::unique_ptr<EventLogger>&& eventLogger);

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
  static inline void traffic(const OperationContext& operationContext, const TrafficEvent& event) {
    getStaticInstance()->logTraffic(operationContext, event);
  }

  /// Actuall methods that implement the behaviors
  virtual void logEvent(LogEvent&& event);
  virtual void logTraffic(const OperationContext& operationContext, const TrafficEvent& event);

 private:
  static std::unique_ptr<EventLogger>& getStaticInstance();
};
} // namespace vrs
