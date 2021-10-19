// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <limits>
#include <memory>

#include <vrs/ForwardDefinitions.h>
#include <vrs/StreamPlayer.h>
#include <vrs/utils/DecoderFactory.h>

namespace vrs::utils {

constexpr uint32_t kInvalidFrameIndex = std::numeric_limits<uint32_t>::max();

/// Helper class to handler decoding video codec frames.
/// Designed to be used in a RecordFormatPlayaable object.
/// VideoRecordFormatStreamPlayer uses this class, and might be the only class that needs to.
class VideoFrameHandler {
 public:
  /// Attempt to decode a video codec encoded frame of a video stream with i-frames, and p-frames.
  /// i-frames can be decoded in any order. p-frames can only be decoded in the correct sequence.
  /// This callback is designed to implement RecordFormatStreamPlayer::onImageRead() for video
  /// images.
  /// @param outBuffer: a preallocated buffer which size must be that of the raw image.
  /// @param reader: data source to read the encoded data frame from.
  /// @param contentBlock: image content block describing the encoded frame, including the codec.
  /// @return 0 if the frame was successfuly decoded, which can happen only if:
  /// - the codec specified in contentBlock is available and was intantiated successfuly.
  /// - the frame is either an i-frame, or the next p-frame after the last decoded frame.
  /// - no error unexpected error happened on the way...
  /// When 0 is returned, the frame is ready to be used, and the next content block can be read.
  /// The decoder's internal state is moved forward, so the next p-frame (if any) can be decoded.
  /// When a non-0 status is returned:
  /// - don't use outFrame, because its content is undefined
  /// - isMissingFrames() will tell if frames need to be decoded before this frame can be.
  int tryToDecodeFrame(void* outBuffer, RecordReader* reader, const ContentBlock& contentBlock);

  /// PixelFrame variant of the call above, for convenience.
  /// @param outFrame: a frame to be set to decoded frame. Its format will be changed if necessary.
  /// @param reader: data source to read the encoded data frame from.
  /// @param contentBlock: image content block describing the encoded frame, including the codec.
  /// @return A decode status code, as above.
  int tryToDecodeFrame(
      PixelFrame& outFrame,
      RecordReader* reader,
      const ContentBlock& contentBlock);

  /// After an attempt to decode a frame was made, tell if frames must be read to build up.
  /// @return False if there is no need to decode previous frames.
  bool isMissingFrames() const {
    return isVideo_ && !videoGoodState_;
  }
  /// When reading a frame out of sequence, the frame might not be decodable without reading
  /// the previous frames in the group, maybe all the way to the last keyframe before this frame.
  /// It's only after trying to read a frame that the required keyframe can be located, and
  /// only then can this API then be used to read the required missing frames, if necessary.
  /// This API was designed to be called from the RecordFormatStreamPlayer::recordReadComplete()
  /// callback, because reading a file or a record can not be done recursively, so it's only after
  /// a read operation is complete that further read operations can be made.
  /// @param filereader: the file reader to read records from.
  /// @param record: the record that needs to be properly read.
  /// @param exactFrame: if true, all the frames up to that frame might be read.
  /// If false, only up to one frame will be read, probably the keyframe.
  /// Use this API when scrubbing in a UI, as the result is unspecified.
  int readMissingFrames(
      RecordFileReader& fileReader,
      const IndexRecord::RecordInfo& record,
      bool exactFrame);
  /// After a failed decoding attemp, which can be detected by calling isMissingFrames(),
  /// this method will tell where the needed key frame is in this stream.
  double getRequestedKeyFrameTimestamp() const {
    return requestedKeyFrameTimestamp_;
  }
  /// After a failed decoding attemp, which can be detected by calling isMissingFrames(),
  /// this method will tell the frame index of that last frame.
  uint32_t getRequestedKeyFrameIndex() const {
    return requestedKeyFrameIndex_;
  }
  /// After a failed decoding attemp, which can be detected by calling isMissingFrames(),
  /// this method will tell how many frames past the key frame timestamp may be skipped.
  uint32_t getFramesToSkip() const;

  void reset();

 private:
  std::unique_ptr<DecoderI> decoder_;

  double decodedKeyFrameTimestamp_{};
  uint32_t decodedKeyFrameIndex_{kInvalidFrameIndex};
  double requestedKeyFrameTimestamp_{};
  uint32_t requestedKeyFrameIndex_{kInvalidFrameIndex};
  bool videoGoodState_{false};
  bool isVideo_{false};
};

} // namespace vrs::utils
