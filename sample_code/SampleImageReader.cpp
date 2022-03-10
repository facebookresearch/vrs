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

#include <cassert>

#include <vrs/RecordFileReader.h>
#include <vrs/RecordFormatStreamPlayer.h>

using namespace vrs;

namespace vrs_sample_code {

// This sample code shows how to read images of a VRS file that follows RecordFormat & DataLayout
// conventions. The code compiles, but it is not actually functional, it just demonstrates basic
// principles.

/// Sample class to read images from a VRS file stream.
class ImagePlayer : public RecordFormatStreamPlayer {
  /// Callback that will receive the images
  bool onImageRead(const CurrentRecord& record, size_t /*idx*/, const ContentBlock& cb) override {
    // the image data was not read yet: allocate your own buffer & read!
    size_t frameByteCount = cb.getBlockSize();
    assert(frameByteCount != 0); // Should not happen, but you want to know if it does!
    assert(frameByteCount != ContentBlock::kSizeUnknown); // Should not happen either...

    /// find more about the image format:
    //    const ImageContentBlockSpec& spec = block.image();
    //    uint32_t width = spec.getWidth();
    //    uint32_t height = spec.getHeight();
    //    PixelFormat pixelFormat = spec.getPixelFormat();
    //    size_t bytesPerPixel = spec.getBytesPerPixel();
    //    uint32_t lineStrideBytes = spec.getStride();

    vector<uint8_t> frameBytes(frameByteCount);
    // Synchronously read the image data, all at once, or line-by-line, byte-by-byte, as you like...
    if (record.reader->read(frameBytes) == 0) {
      /// do your thing with the image...
    }
    return true; // read next blocks, if any
  }
};

/// Sample basic code to demonstrate how to read a VRS file.
struct SampleImageReader {
  /// This function is the entry point for your reader
  void imageReader(const string& vrsFilePath) {
    RecordFileReader reader;
    if (reader.openFile(vrsFilePath) == 0) {
      vector<unique_ptr<StreamPlayer>> streamPlayers;
      // Map the devices referenced in the file to stream player objects
      // Just ignore the device(s) you do not care for
      const set<StreamId>& streamIds = reader.getStreams();
      for (auto id : streamIds) {
        if (id.getTypeId() == RecordableTypeId::SampleDevice) {
          streamPlayers.emplace_back(new ImagePlayer());
          reader.setStreamPlayer(id, streamPlayers.back().get());
        }
      }
      // We're ready: read all the records in order, and send them to the stream players registered
      reader.readAllRecords();
    }
  }
};

} // namespace vrs_sample_code
