// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

#pragma once

#include <map>

#include <vrs/RecordFormatStreamPlayer.h>

#include <vrs/utils/VideoFrameHandler.h>

namespace vrs::utils {

/// Helper class to handle video codec compressed frames.
/// Note that this class can handle anything else RecordFormatStreamPlayer can handle, just as well.
class VideoRecordFormatStreamPlayer : public RecordFormatStreamPlayer {
 public:
  /// Method to handle image/video data received in the onImageRead() callback.
  /// @param outBuffer: an allocated buffer where to write the decoded image data.
  /// The buffer's size must be cb.image().getRawImageSize()
  /// @param record: record being read context.
  /// @param cb: image content block description.
  /// @return 0 if the image was properly decoded and the data written out in the buffer.
  /// Otherwise, a non-0 status code indicating the error.
  int tryToDecodeFrame(void* outBuffer, const CurrentRecord& record, const ContentBlock& cb);

  /// PixelFrame version of the API above.
  int tryToDecodeFrame(PixelFrame& outFrame, const CurrentRecord& record, const ContentBlock& cb);

  /// Tell if the last image played from a stream couldn't be decoded because of missing frames.
  /// @param streamId: the id of the stream to check, required because a stream player may be
  /// attached to multiple streams at once.
  /// @return True if
  bool isMissingFrames(StreamId streamId) const;

  /// Same functionality as above, but only valid when attached to a single stream.
  bool isMissingFrames() const;

  /// When reading a frame out of sequence, the frame might not be decodable without reading
  /// the previous frames in the group, maybe all the way to the last keyframe before this frame.
  /// It's only after trying to read a frame that the required keyframe can be located, and
  /// only then can this API then be used to read the required missing frames, if necessary.
  /// This API was designed to be called from the
  /// VideoRecordFormatStreamPlayer::recordReadComplete() callback, because reading a file or a
  /// record can not be done recursively, so it's only after a read operation is complete that
  /// further read operations can be made.
  /// @param filereader: the file reader to read records from.
  /// @param record: the record that needs to be properly read.
  /// @param exactFrame: if true, all the frames up to that frame might be read.
  /// Designed to be used in VideoRecordFormatStreamPlayer::recordReadComplete().
  /// If false, only up to one frame will be read, probably the keyframe.
  /// Use this API when scrubbing in a UI, as the result is unspecified.
  int readMissingFrames(
      RecordFileReader& fileReader,
      const IndexRecord::RecordInfo& recordInfo,
      bool exactFrame = true);

  /// Tell if the read operation is being performed to read frames before the actual target frame.
  bool whileReadingMissingFrames() const {
    return whileReadingMissingFrames_;
  }

  /// Reset videoFrameHandler's internal state to force reading from the key frame.
  /// @param streamId: StreamID for the handler you want to reset, if you don't specify anything
  /// reset all the handlers.
  void resetVideoFrameHandler(const StreamId& streamId = {});

/// Reference implementation to systematically & transparently read missing I-frame & P-frames.
/// Note that callbacks when reading previous frames will be happen, as if the record was really
/// the one we need to decode. whileReadingMissingFrames() will tell the context during callbacks.
#if 0
  int recordReadComplete(RecordFileReader& fileReader, const IndexRecord::RecordInfo& recordInfo)
      override {
    return readMissingFrames(fileReader, recordInfo, true);
  }
#endif

 private:
  std::map<StreamId, VideoFrameHandler> handlers_;
  bool whileReadingMissingFrames_ = false;
};

} // namespace vrs::utils
