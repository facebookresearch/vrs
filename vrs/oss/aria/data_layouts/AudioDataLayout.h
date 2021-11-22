// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <cstdint>

#include <vrs/DataLayout.h>
#include <vrs/DataLayoutConventions.h>
#include <vrs/DataPieces.h>

// Note: The VRS stream type for audio data is
// vrs::RecordableTypeId::StereoAudioRecordableClass.

namespace aria {

struct AudioConfigurationLayout : public vrs::AutoDataLayout {
  static constexpr uint32_t kVersion = 2;

  vrs::DataPieceValue<std::uint32_t> streamId{"stream_id"};

  // Number of channels in the audio stream.
  vrs::DataPieceValue<std::uint8_t> numChannels{vrs::DataLayoutConventions::kAudioChannelCount};

  // Number of samples per second. Typical values: 44100Hz.
  vrs::DataPieceValue<std::uint32_t> sampleRate{vrs::DataLayoutConventions::kAudioSampleRate};

  // Format of each subsample, deciding the bits and type per subsample.
  // Convertible from vrs::AudioSampleFormat.
  vrs::DataPieceValue<std::uint8_t> sampleFormat{vrs::DataLayoutConventions::kAudioSampleFormat};

  vrs::AutoDataLayoutEnd endLayout;
};

struct AudioDataLayout : public vrs::AutoDataLayout {
  static constexpr uint32_t kVersion = 2;

  // A list of timestamps of each sample in the block, following the same order they are stored in
  // the ContentBlock.
  vrs::DataPieceVector<std::int64_t> captureTimestampsNs{"capture_timestamps_ns"};

  // Set 1 for muted, 0 otherwise.
  vrs::DataPieceValue<std::uint8_t> audioMuted{"audio_muted"};

  vrs::AutoDataLayoutEnd endLayout;
};

} // namespace aria
