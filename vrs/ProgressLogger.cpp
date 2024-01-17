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

#include "ProgressLogger.h"

#include <iomanip>
#include <sstream>

#define DEFAULT_LOG_CHANNEL "ProgressLogger"
#include <logging/Log.h>

#include <vrs/helpers/Strings.h>
#include <vrs/os/Time.h>

using namespace std;

namespace vrs {

ProgressLogger::ProgressLogger(bool detailedProgress, double updateDelay)
    : detailedProgress_{detailedProgress}, updateDelay_{updateDelay} {
  nextProgressTime_ = 0;
  stepNumber_ = 0;
  stepCount_ = 1;
}

void ProgressLogger::setStepCount(int stepCount) {
  stepCount_ = stepCount;
}

ProgressLogger::~ProgressLogger() = default;

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
    updateNextProgressTime();
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
    updateNextProgressTime();
  }
  return shouldKeepGoing();
}

bool ProgressLogger::logDuration(const string& operationName, double duration, int precision) {
  if (detailedProgress_) {
    stringstream ss;
    ss << operationName << " in " << fixed << setprecision(precision) << duration << "s.";
    logMessage(ss.str());
    updateNextProgressTime();
  }
  return shouldKeepGoing();
}

void ProgressLogger::updateNextProgressTime() {
  nextProgressTime_ = os::getTimestampSec() + updateDelay_;
}

void ProgressLogger::logMessage(const string& message) {
  XR_LOGI("{}: {}", helpers::humanReadableDateTime(os::getCurrentTimeSecSinceEpoch()), message);
}

void ProgressLogger::logError(const string& message) {
  XR_LOGE("{}: {}", helpers::humanReadableDateTime(os::getCurrentTimeSecSinceEpoch()), message);
}

void ProgressLogger::updateStep(size_t /*progress*/, size_t /*maxProgress*/) {}

SilentLogger::~SilentLogger() = default;

} // namespace vrs
