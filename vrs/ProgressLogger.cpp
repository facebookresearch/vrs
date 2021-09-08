// Facebook Technologies, LLC Proprietary and Confidential.

#include "ProgressLogger.h"

#include <iomanip>
#include <sstream>

#define DEFAULT_LOG_CHANNEL "ProgressLogger"
#include <logging/Log.h>
#include <vrs/os/Time.h>

using namespace std;

namespace vrs {

ProgressLogger::ProgressLogger(bool detailedProgress, double updateDelay)
    : detailedProgress_{detailedProgress}, updateDelay_{updateDelay} {
  updateNextProgessTime();
  stepNumber_ = 0;
  stepCount_ = 1;
}

void ProgressLogger::setStepCount(int stepCount) {
  stepCount_ = stepCount;
}

ProgressLogger::~ProgressLogger() {}

void ProgressLogger::setDetailedProgress(bool detailedProgress) {
  detailedProgress_ = detailedProgress;
}

bool ProgressLogger::logNewStep(const string& stepName, size_t progress, size_t maxProgress) {
  if (++stepNumber_ > stepCount_) {
    stepCount_++;
  }
  return logProgress(stepName, progress, maxProgress, true);
}

bool ProgressLogger::shouldKeepGoing() {
  return true;
}

bool ProgressLogger::logProgress(
    const string& stepName,
    size_t progress,
    size_t maxProgress,
    bool newStep) {
  if ((newStep && detailedProgress_) || os::getTimestampSec() > nextProgressTime_) {
    if (maxProgress > 0 && maxProgress >= progress) {
      updateStep(progress, maxProgress);
      if (progress > 0) {
        logMessage(stepName + ' ' + to_string(progress * 100 / maxProgress) + "%...");
      } else {
        logMessage(stepName + "...");
      }
    } else {
      logMessage(stepName + "...");
    }
    updateNextProgessTime();
  }
  return shouldKeepGoing();
}

bool ProgressLogger::logStatus(const string& stepName, int status) {
  if (status != 0 || detailedProgress_ || os::getTimestampSec() > nextProgressTime_) {
    if (status == 0) {
      logMessage(stepName + " complete.");
    } else {
      logError(stepName + " failed!");
    }
    updateNextProgessTime();
  }
  return shouldKeepGoing();
}

bool ProgressLogger::logDuration(const string& operationName, double duration, int precision) {
  if (detailedProgress_) {
    std::stringstream ss;
    ss << operationName << " in " << fixed << setprecision(precision) << duration << "s.";
    logMessage(ss.str());
    updateNextProgessTime();
  }
  return shouldKeepGoing();
}

void ProgressLogger::updateNextProgessTime() {
  nextProgressTime_ = os::getTimestampSec() + updateDelay_;
}

void ProgressLogger::logMessage(const string& message) {
  XR_LOGI("{:.3f}: {}", os::getTimestampSec(), message);
}

void ProgressLogger::logError(const string& message) {
  XR_LOGE("{:.3f}: {}", os::getTimestampSec(), message);
}

void ProgressLogger::updateStep(size_t /*progress*/, size_t /*maxProgress*/) {}

SilentLogger::~SilentLogger() {}

} // namespace vrs
