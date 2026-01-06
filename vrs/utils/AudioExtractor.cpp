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

#include <vrs/utils/AudioExtractor.h>

#include <array>
#include <iostream>
#include <utility>

#define DEFAULT_LOG_CHANNEL "AudioExtractor"
#include <logging/Log.h>
#include <logging/Verify.h>

#include <vrs/RecordReaders.h>
#include <vrs/helpers/Endian.h>
#include <vrs/helpers/FileMacros.h>
#include <vrs/helpers/Throttler.h>
#include <vrs/os/Utils.h>

using namespace std;
using namespace vrs;

namespace {

utils::Throttler& getThrottler() {
  static utils::Throttler sThrottler;
  return sThrottler;
}

template <class T>
inline void writeHeader(void* p, const T t) {
  memcpy(p, &t, sizeof(T));
}
} // namespace

constexpr uint32_t kWavHeaderSize = 44;

namespace vrs::utils {

int AudioExtractor::createWavFile(
    const string& wavFilePath,
    const AudioContentBlockSpec& audioBlock,
    DiskFile& outFile) {
  IF_ERROR_RETURN(outFile.create(wavFilePath));

  array<uint8_t, kWavHeaderSize> fileHeader{};
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

  uint32_t bytesPerSample = audioBlock.getBytesPerSample();
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

  return outFile.write((char*)fileHeader.data(), fileHeader.size());
}

int AudioExtractor::writeWavAudioData(
    DiskFile& inFile,
    const AudioContentBlockSpec& audioBlock,
    const vector<uint8_t>& audio) {
  uint32_t srcOffset = 0;
  uint32_t bytesPerSampleBlock = audioBlock.getBytesPerSample() * audioBlock.getChannelCount();
  uint32_t srcStride = audioBlock.getSampleFrameStride();
  uint32_t totalSamples = audioBlock.getSampleCount();
  for (uint32_t i = 0; i < totalSamples; ++i) {
    if (srcOffset >= (uint32_t)audio.size()) {
      cout << "Malformed audio block encountered, read past end of audio block\n";
      break;
    }

    // round up # of bits per sample to nearest byte
    IF_ERROR_RETURN(inFile.write((char*)audio.data() + srcOffset, bytesPerSampleBlock));
    srcOffset += srcStride;
  }
  return 0;
}

int AudioExtractor::closeWavFile(DiskFile& inFile) {
  if (!inFile.isOpened()) {
    return 0;
  }
  uint32_t totalAudioDataSize = inFile.getPos() - kWavHeaderSize;
  // seek to beginning, update chunk & file sizes in header before closing
  IF_ERROR_RETURN(inFile.setPos(4));
  uint32_t fileSize = htole32(36 + totalAudioDataSize);
  IF_ERROR_RETURN(inFile.write((char*)&fileSize, sizeof(fileSize)));
  IF_ERROR_RETURN(inFile.setPos(40));
  uint32_t dataSize = htole32(totalAudioDataSize);
  IF_ERROR_RETURN(inFile.write((char*)&dataSize, sizeof(dataSize)));
  return inFile.close();
}

AudioExtractor::AudioExtractor(string folderPath, StreamId id, uint32_t& counter)
    : folderPath_{std::move(folderPath)},
      id_{id},
      cumulativeOutputAudioFileCount_{counter},
      currentAudioContentBlockSpec_{AudioFormat::UNDEFINED} {}

AudioExtractor::~AudioExtractor() {
  // Need to write header size before closing file
  closeWavFile(currentWavFile_);
}

bool AudioExtractor::onAudioRead(const CurrentRecord& record, size_t, const ContentBlock& cb) {
  utils::AudioBlock audioBlock;
  if (!audioBlock.readBlock(record.reader, cb)) {
    THROTTLED_LOGW(
        record.fileReader,
        "{} - {} record @ {}: Failed read audio data.",
        record.streamId.getNumericName(),
        toString(record.recordType),
        record.timestamp);
    return false;
  }
  if (!audioBlock.decompressAudio(decompressor_)) {
    THROTTLED_LOGW(
        record.fileReader,
        "{} - {} record @ {}: Failed decode audio data.",
        record.streamId.getNumericName(),
        toString(record.recordType),
        record.timestamp);
  }

  const AudioContentBlockSpec& audioBlockSpec = audioBlock.getSpec();
  if (audioBlockSpec.getAudioFormat() != AudioFormat::PCM) {
    cout << "Skipping non-PCM audio block\n";
    return true;
  }

  vector<uint8_t>& audioBytes = audioBlock.getBuffer();
  const int64_t kMaxWavFileSize = 1LL << 32; // WAV can't handle files larger than 4 GiB.
  if (!currentAudioContentBlockSpec_.isCompatibleWith(audioBlockSpec) ||
      currentWavFile_.getPos() + audioBytes.size() >= kMaxWavFileSize) {
    closeWavFile(currentWavFile_);

    VERIFY_SUCCESS(os::makeDirectories(folderPath_));
    string path = fmt::format(
        "{}/{}-{:04}-{:.3f}.wav",
        folderPath_,
        id_.getNumericName(),
        streamOutputAudioFileCount_,
        record.timestamp);
    cout << "Writing " << path << "\n";
    cout << "WAV file details: " << static_cast<int>(audioBlockSpec.getChannelCount()) << " channel"
         << (audioBlockSpec.getChannelCount() != 1 ? "s, " : ", ") << audioBlockSpec.getSampleRate()
         << " " << audioBlockSpec.getSampleFormatAsString() << " samples/s, "
         << static_cast<int>(audioBlockSpec.getBitsPerSample()) << " bits per sample, "
         << static_cast<int>(audioBlockSpec.getSampleFrameStride())
         << " bytes sample frame stride.\n";
    VERIFY_SUCCESS(createWavFile(path, audioBlockSpec, currentWavFile_));

    currentAudioContentBlockSpec_ = audioBlockSpec;
    cumulativeOutputAudioFileCount_++;
    streamOutputAudioFileCount_++;
    segmentStartTimestamp_ = record.timestamp;
    segmentSamplesCount_ = 0;
  }

  VERIFY_SUCCESS(writeWavAudioData(currentWavFile_, audioBlockSpec, audioBytes));

  // Time/sample counting validation
  if (segmentSamplesCount_ > 0) {
    const double kMaxJitter = 0.01;
    double actualTime = record.timestamp - segmentStartTimestamp_;
    double expectedTime =
        static_cast<double>(segmentSamplesCount_) / currentAudioContentBlockSpec_.getSampleRate();
    if (actualTime - expectedTime > kMaxJitter) {
      THROTTLED_LOGW(
          record.fileReader,
          "Audio block at {:.3f} {:.3f} ms late.",
          actualTime,
          actualTime - expectedTime);
    } else if (expectedTime - actualTime > kMaxJitter) {
      THROTTLED_LOGW(
          record.fileReader,
          "Audio block at {:.3f} {:.3f} ms, {:.2f}% early.",
          expectedTime,
          expectedTime - actualTime,
          (1. - actualTime / expectedTime) * 100);
    }
  } else {
    segmentStartTimestamp_ = record.timestamp;
  }
  segmentSamplesCount_ += audioBlock.getSampleCount();

  return true; // read next blocks, if any
}

bool AudioExtractor::onUnsupportedBlock(
    const CurrentRecord& record,
    size_t,
    const ContentBlock& cb) {
  // the audio was not decoded ... not sure why?
  if (cb.getContentType() == ContentType::AUDIO) {
    THROTTLED_LOGW(
        record.fileReader,
        "Audio block skipped for {}, content: {}",
        record.streamId.getName(),
        cb.asString());
  }
  return false;
}

} // namespace vrs::utils
