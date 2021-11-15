// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include "TelemetryLogger.h"

#define DEFAULT_LOG_CHANNEL "TelemetryLogger"
#include <logging/Log.h>

#include <vrs/helpers/Strings.h>

using namespace std;

namespace vrs {

TelemetryLogger& TelemetryLogger::getInstance() {
  return *getStaticInstance().get();
}

std::unique_ptr<TelemetryLogger> TelemetryLogger::setLogger(
    std::unique_ptr<TelemetryLogger>&& telemetryLogger) {
  std::unique_ptr<TelemetryLogger> previousLogger;
  getStaticInstance().swap(telemetryLogger);
  return previousLogger;
}

void TelemetryLogger::logEvent(LogEvent&& event) {
  if (event.type == TelemetryLogger::kErrorType) {
    XR_LOGE(
        "{}, {}: {} {}",
        event.operationContext.operation,
        event.operationContext.sourceLocation,
        event.message,
        event.serverMessage);
  } else {
    XR_LOGW(
        "{}, {}: {} {}",
        event.operationContext.operation,
        event.operationContext.sourceLocation,
        event.message,
        event.serverMessage);
  }
}

void TelemetryLogger::logTraffic(
    const OperationContext& operationContext,
    const TrafficEvent& event) {
  XR_LOGI(
      "{} {} {}/{}, {}: When: {} Duration: {}/{} "
      "Offset: {} Transfer: {}/{} Retries: {} Errors: {} 429: {}",
      operationContext.operation,
      event.uploadNotDownload ? "upload" : "download",
      event.isSuccess ? "success" : "failure",
      event.httpStatus,
      operationContext.sourceLocation,
      event.transferStartTime,
      event.transferDurationMs,
      event.totalDurationMs,
      event.transferOffset,
      helpers::humanReadableFileSize(event.transferSize),
      helpers::humanReadableFileSize(event.transferRequestSize),
      event.retryCount,
      event.errorCount,
      event.error429Count);
}

std::unique_ptr<TelemetryLogger>& TelemetryLogger::getStaticInstance() {
  static std::unique_ptr<TelemetryLogger> sInstance = std::make_unique<TelemetryLogger>();
  return sInstance;
}

TelemetryLogger::~TelemetryLogger() {}

TrafficEvent& TrafficEvent::setUrl(const string& aServerName) {
  // discard prefixes such as http:// and https:// by finding "://"
  size_t start = aServerName.find("://");
  if (start == string::npos) {
    start = 0;
  } else {
    start += 3; // skip the 3 characters
  }
  // only keep what's before the first '/'
  size_t end = start;
  while (aServerName[end] != 0 && aServerName[end] != '/') {
    end++;
  }
  serverName.assign(aServerName.data(), start, end - start);
  return *this;
}

} // namespace vrs
