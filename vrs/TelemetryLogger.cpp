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

#include "TelemetryLogger.h"

#define DEFAULT_LOG_CHANNEL "TelemetryLogger"
#include <logging/Log.h>

#include <vrs/helpers/Strings.h>
#include <vrs/os/Time.h>

using namespace std;

namespace vrs {

unique_ptr<TelemetryLogger> TelemetryLogger::setLogger(
    unique_ptr<TelemetryLogger>&& telemetryLogger) {
  unique_ptr<TelemetryLogger> previousLogger{std::move(getInstance())};
  getInstance() = std::move(telemetryLogger);
  return previousLogger;
}

void TelemetryLogger::logEvent(LogEvent&& event) {
  if (event.type == TelemetryLogger::kErrorType) {
    XR_LOGE(
        "{}, {}: {}, {}",
        event.operationContext.operation,
        event.operationContext.sourceLocation,
        event.message,
        event.serverReply);
  } else {
    XR_LOGW(
        "{}, {}: {}, {}",
        event.operationContext.operation,
        event.operationContext.sourceLocation,
        event.message,
        event.serverReply);
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

unique_ptr<TelemetryLogger>& TelemetryLogger::getInstance() {
  static unique_ptr<TelemetryLogger> sInstance{make_unique<TelemetryLogger>()};
  return sInstance;
}

TelemetryLogger::~TelemetryLogger() {}

TrafficEvent& TrafficEvent::setAttemptStartTime() {
  transferStartTime = os::getCurrentTimeSecSinceEpoch();
  return *this;
}

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
  serverName.assign(aServerName, start, end - start);
  return *this;
}

} // namespace vrs
