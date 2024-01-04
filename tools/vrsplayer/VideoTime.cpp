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

#include "VideoTime.h"

#include <sstream>

#include <qmetaobject.h>

#include <portaudio.h>

#define DEFAULT_LOG_CHANNEL "VideoTime"
#include <logging/Log.h>

#include <vrs/os/Time.h>

#include "PlayerUI.h"

namespace vrsp {

using namespace std;

PaStream* VideoTime::sAudioStream = nullptr;
double VideoTime::sPlaybackSpeed = 1;
bool VideoTime::sValidatedTime = false;
double VideoTime::sFirstTimeAudioTime = 0;
double VideoTime::sFirstClassicTime = 0;
PlayerUI* VideoTime::sPlayerUI = nullptr;

#define PA_TIME (Pa_GetStreamTime(sAudioStream))

static string getAudioDeviceName() {
  PaDeviceIndex defaultOutpuDeviceIndex = Pa_GetDefaultOutputDevice();
  const PaDeviceInfo* info = Pa_GetDeviceInfo(defaultOutpuDeviceIndex);
  return info ? info->name : "<unknown audio device>";
}

void VideoTime::setTimeAudioStreamSource(PaStream* audioStream) {
  sAudioStream = audioStream;
  resetValidation();
}

double VideoTime::getTime() const {
  if (sAudioStream && playing_ && !sValidatedTime) {
    validateTime();
  }
  return playing_ ? getRawTime() - offset_ : pausedtime_;
}

void VideoTime::resetValidation() {
  sValidatedTime = false;
  sFirstClassicTime = vrs::os::getTimestampSec();
  sFirstTimeAudioTime = PA_TIME;
}

void VideoTime::validateTime() {
  double gap = vrs::os::getTimestampSec() - sFirstClassicTime;
  if (gap > 1) {
    double audiogap = PA_TIME - sFirstTimeAudioTime;
    double ratio = audiogap / gap;
    QString title;
    stringstream msg;
    if (ratio <= 0) {
      // dead audio time
      title = "Audio Device Not Working";
      msg << "The clock provided by the audio device named '" << getAudioDeviceName()
          << "' doesn't appear to be working at all.";
      sAudioStream = nullptr;
      XR_LOGE("{}", msg.str());
    } else if (ratio < 0.95) {
      // audio time too slow
      title = "Slow Audio Device";
      msg << "The clock provided by the audio device named '" << getAudioDeviceName()
          << "' doesn't appear to be going fast enough, "
          << "since it's going only at about " << int(ratio * 100)
          << "% of your system's clock speed.";
      sAudioStream = nullptr;
      XR_LOGE("{}", msg.str());
    } else if (ratio < 1.05) {
      // audio time is about right
      sValidatedTime = true;
    } else {
      // audio time too fast
      title = "Fast Audio Device";
      msg << "The clock provided by the audio device named '" << getAudioDeviceName()
          << "' appears to be too fast, "
          << "since it's going at about " << int(ratio * 100) << "% of your system's clock speed.";
      sAudioStream = nullptr;
      XR_LOGE("{}", msg.str());
    }
    if (!sValidatedTime && sPlayerUI) {
      msg << "\n"
          << "\n"
          << "VRSplayer will try using the system's clock instead.\n"
          << "\n"
          << "You could try to select a different default audio device using "
          << "your system's preferences/control panel.";
      QMetaObject::invokeMethod(
          sPlayerUI,
          "reportError",
          Qt::AutoConnection,
          Q_ARG(QString, title),
          Q_ARG(QString, QString::fromStdString(msg.str())));
    }
  }
}

double VideoTime::getRawTime() {
  return ((sAudioStream != nullptr) ? PA_TIME : vrs::os::getTimestampSec()) * sPlaybackSpeed;
}

} // namespace vrsp
