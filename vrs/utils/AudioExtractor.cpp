// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "AudioExtractor.h"

#include <array>
#include <iostream>

#define DEFAULT_LOG_CHANNEL "AudioExtractor"
#include <logging/Log.h>
#include <logging/Verify.h>

#include <vrs/RecordReaders.h>
#include <vrs/helpers/Endian.h>
#include <vrs/os/Utils.h>

using namespace std;
using namespace vrs;

namespace {

template <class T>
void writeHeader(void* p, const T t) {
  memcpy(p, &t, sizeof(T));
}

void startNewWavFile(
    ofstream& outFile,
    const AudioContentBlockSpec& audioBlock,
    const string& folderPath,
    StreamId id,
    uint32_t audioCounter,
    double timestamp) {
  XR_VERIFY(os::makeDirectories(folderPath) == 0);
  string path = fmt::format(
      "{}/{}-{:04}-{:.3f}.wav", folderPath, id.getNumericName(), audioCounter, timestamp);
  cout << "Writing " << path << endl;
  cout << "WAV file details: " << static_cast<int>(audioBlock.getChannelCount()) << " channel"
       << (audioBlock.getChannelCount() != 1 ? "s, " : ", ") << audioBlock.getSampleRate() << " "
       << audioBlock.getSampleFormatAsString() << " samples/s, "
       << static_cast<int>(audioBlock.getBitsPerSample()) << " bits per sample, "
       << static_cast<int>(audioBlock.getSampleBlockStride()) << " bytes sample block stride."
       << endl;

  outFile.open(path, ofstream::out | ofstream::binary);
  array<uint8_t, 44> fileHeader{};
  writeHeader(fileHeader.data() + 0, htobe32(0x52494646)); // 'RIFF' in big endian
  writeHeader(fileHeader.data() + 4, htole32(0)); // will come back and compute this later (36 +
                                                  // total PCM data size), write in little endian
  writeHeader(fileHeader.data() + 8, htobe32(0x57415645)); // 'WAVE' in big endian
  writeHeader(fileHeader.data() + 12, htobe32(0x666d7420)); // 'fmt' in big endian
  writeHeader(
      fileHeader.data() + 16,
      htole32(16)); // size of this sub chunk, always 16, write in little endian

  // See http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/WAVE.html
  // NOTE: Technically non-PCM formats should have a 'fact' chunk, which we are not writing out, but
  // it doesn't seem strictly necessary
  uint16_t format = 1; // 1 is for PCM (default)
  if (audioBlock.isIEEEFloat()) {
    format = 3;
  } else if (audioBlock.getSampleFormat() == AudioSampleFormat::A_LAW) {
    format = 6;
  } else if (audioBlock.getSampleFormat() == AudioSampleFormat::MU_LAW) {
    format = 7;
  }

  uint32_t bytesPerSample = (audioBlock.getBitsPerSample() + 7) / 8;
  writeHeader(fileHeader.data() + 20, htole16(format)); // audio format, write in little endian
  writeHeader(
      fileHeader.data() + 22,
      htole16(audioBlock.getChannelCount())); // channel count, write in little endian
  writeHeader(
      fileHeader.data() + 24,
      htole32(audioBlock.getSampleRate())); // sample rate, write in little endian
  writeHeader(
      fileHeader.data() + 28,
      htole32(
          audioBlock.getSampleRate() * audioBlock.getChannelCount() *
          bytesPerSample)); // byte rate, write in little endian
  writeHeader(
      fileHeader.data() + 32,
      htole16(
          audioBlock.getChannelCount() *
          bytesPerSample)); // block align (size of 1 sample), write in little endian
  writeHeader(
      fileHeader.data() + 34,
      htole16(audioBlock.getBitsPerSample())); // bits per sample, write in little endian
  writeHeader(fileHeader.data() + 36, htobe32(0x64617461)); // 'data' in big endian
  writeHeader(fileHeader.data() + 40, htole32(0)); // will come back and fill this in later (total
                                                   // PCM data size), write in little endian

  outFile.write((char*)fileHeader.data(), fileHeader.size());
}

void closeExistingWavFile(ofstream& outFile, uint32_t totalAudioDataSize) {
  // close existing file if there is one
  if (outFile.is_open()) {
    // seek to beginning, update chunk & file sizes in header before closing
    outFile.seekp(4);
    uint32_t fileSize = htole32(36 + totalAudioDataSize);
    outFile.write((char*)&fileSize, sizeof(fileSize));
    outFile.seekp(40);
    uint32_t dataSize = htole32(totalAudioDataSize);
    outFile.write((char*)&dataSize, sizeof(dataSize));
    outFile.close();
  }
}

uint32_t writeAudioBlock(
    ofstream& outFile,
    const AudioContentBlockSpec& audioBlock,
    const vector<uint8_t>& audio) {
  uint32_t bytesWritten = 0;
  uint32_t srcOffset = 0;
  uint32_t bytesPerSampleBlock =
      (audioBlock.getBitsPerSample() + 7) / 8 * audioBlock.getChannelCount();
  uint32_t srcStride = audioBlock.getSampleBlockStride();
  uint32_t totalSamples = audioBlock.getSampleCount();
  for (uint32_t i = 0; i < totalSamples; ++i) {
    if (srcOffset >= (uint32_t)audio.size()) {
      cout << "Malformed audio block encountered, read past end of audio block" << endl;
      break;
    }

    // round up # of bits per sample to nearest byte
    outFile.write((char*)audio.data() + srcOffset, bytesPerSampleBlock);
    bytesWritten += bytesPerSampleBlock;
    srcOffset += srcStride;
  }
  return bytesWritten;
}

} // namespace

namespace vrs::utils {

AudioExtractor::AudioExtractor(const string& folderPath, StreamId id, uint32_t& counter)
    : folderPath_{folderPath},
      id_{id},
      cumulativeOutputAudioFileCount_{counter},
      currentAudioContentBlockSpec_{AudioFormat::UNDEFINED} {}

AudioExtractor::~AudioExtractor() {
  // Need to write header size before closing file
  closeExistingWavFile(currentWavFile_, totalAudioDataSize_);
}

bool AudioExtractor::onAudioRead(
    const CurrentRecord& record,
    size_t,
    const ContentBlock& audioBlock) {
  audio_.resize(audioBlock.getBlockSize());
  int readStatus = record.reader->read(audio_);
  if (readStatus != 0) {
    XR_LOGW(
        "{} - {} record @ {}: Failed read audio data ({}).",
        record.streamId.getNumericName(),
        toString(record.recordType),
        record.timestamp,
        errorCodeToMessage(readStatus));
    return false;
  }

  const AudioContentBlockSpec& audioBlockSpec = audioBlock.audio();
  if (audioBlockSpec.getAudioFormat() != AudioFormat::PCM) {
    cout << "Skipping non-PCM audio block\n";
    return true;
  }

  if (!currentAudioContentBlockSpec_.isCompatibleWith(audioBlockSpec)) {
    closeExistingWavFile(currentWavFile_, totalAudioDataSize_);
    totalAudioDataSize_ = 0;
    startNewWavFile(
        currentWavFile_,
        audioBlockSpec,
        folderPath_,
        id_,
        streamOutputAudioFileCount_,
        record.timestamp);
    currentAudioContentBlockSpec_ = audioBlockSpec;
    cumulativeOutputAudioFileCount_++;
    streamOutputAudioFileCount_++;
    segmentStartTimestamp_ = record.timestamp;
    segmentSamplesCount_ = 0;
  }

  totalAudioDataSize_ += writeAudioBlock(currentWavFile_, audioBlockSpec, audio_);

  // Time/sample counting validation
  if (segmentSamplesCount_ > 0) {
    const double kMaxJitter = 0.01;
    double actualTime = record.timestamp - segmentStartTimestamp_;
    double expectedTime =
        static_cast<double>(segmentSamplesCount_) / currentAudioContentBlockSpec_.getSampleRate();
    if (actualTime - expectedTime > kMaxJitter) {
      XR_LOGW("Audio block at {:.3f} {:.3f} ms late.", actualTime, actualTime - expectedTime);
    } else if (expectedTime - actualTime > kMaxJitter) {
      XR_LOGW(
          "Audio block at {:.3f} {:.3f} ms, {:.2f}% early.",
          expectedTime,
          expectedTime - actualTime,
          (1. - actualTime / expectedTime) * 100);
    }
  } else {
    segmentStartTimestamp_ = record.timestamp;
  }
  segmentSamplesCount_ += audioBlock.audio().getSampleCount();

  return true; // read next blocks, if any
}

bool AudioExtractor::onUnsupportedBlock(
    const CurrentRecord& record,
    size_t,
    const ContentBlock& cb) {
  // the audio was not decoded ... not sure why?
  if (cb.getContentType() == ContentType::AUDIO) {
    XR_LOGW("Audio block skipped for {}, content: {}", record.streamId.getName(), cb.asString());
  }
  return false;
}

} // namespace vrs::utils
