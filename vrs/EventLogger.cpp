// Facebook Technologies, LLC Proprietary and Confidential.

#include "EventLogger.h"

#define DEFAULT_LOG_CHANNEL "EventLogger"
#include <logging/Log.h>

#include <vrs/RecordFileInfo.h>

using namespace std;

namespace vrs {

EventLogger& EventLogger::getInstance() {
  return *getStaticInstance().get();
}

std::unique_ptr<EventLogger> EventLogger::setLogger(std::unique_ptr<EventLogger>&& eventLogger) {
  std::unique_ptr<EventLogger> previousLogger;
  getStaticInstance().swap(eventLogger);
  return previousLogger;
}

void EventLogger::logEvent(LogEvent&& event) {
  if (event.type == EventLogger::kErrorType) {
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

void EventLogger::logTraffic(const OperationContext& operationContext, const TrafficEvent& event) {
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
      RecordFileInfo::humanReadableFileSize(event.transferSize),
      RecordFileInfo::humanReadableFileSize(event.transferRequestSize),
      event.retryCount,
      event.errorCount,
      event.error429Count);
}

std::unique_ptr<EventLogger>& EventLogger::getStaticInstance() {
  static std::unique_ptr<EventLogger> sInstance = std::make_unique<EventLogger>();
  return sInstance;
}

EventLogger::~EventLogger() {}

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
