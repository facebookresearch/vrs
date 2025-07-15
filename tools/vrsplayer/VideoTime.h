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

#include <qobject.h>

#include <vrs/os/Time.h>

typedef void PaStream; // NOLINT(modernize-use-using)

namespace vrsp {

class PlayerUI;

class VideoTime {
 public:
  /// Get a time measurement unrelated to playback start/pause/stop
  /// The time source might be a system clock (default),
  /// or an audio stream clock, as appropriate.
  static double getRawTime();
  /// Set audio stream from which the raw time should be retrieved.
  static void setTimeAudioStreamSource(PaStream* audioStream);

  static void setPlayerUI(PlayerUI* ui) {
    sPlayerUI = ui;
  }

  static void setPlaybackSpeed(double speed) {
    sPlaybackSpeed = speed;
  }

  static double getPlaybackSpeed() {
    return sPlaybackSpeed;
  }

  void start() {
    if (!playing_) {
      resetValidation();
      setTime(pausedtime_);
      playing_ = true;
    }
  }
  void pause() {
    if (playing_) {
      pausedtime_ = getRawTime() - offset_;
      playing_ = false;
    }
  }
  void setTime(double time) {
    pausedtime_ = time;
    offset_ = getRawTime() - time;
  }
  double getTime() const;

 private:
  bool playing_ = false;
  double pausedtime_{};
  double offset_{};

  static void resetValidation();
  static void validateTime();

  static bool sValidatedTime;
  static double sFirstTimeAudioTime;
  static double sFirstClassicTime;

  static PaStream* sAudioStream;
  static double sPlaybackSpeed;

  static PlayerUI* sPlayerUI; // for error reporting
};

} // namespace vrsp
