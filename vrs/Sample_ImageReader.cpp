// Facebook Technologies, LLC Proprietary and Confidential.

#include <cassert>

#include <vrs/RecordFileReader.h>
#include <vrs/RecordFormatStreamPlayer.h>

using namespace vrs;

/// This sample code shows how to read images of a VRS file that follows RecordFormat & DataLayout
/// conventions.

// class to consume images of a VRS file
class ImagePlayer : public RecordFormatStreamPlayer {
  /// this is where you'll receive the images
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

    std::vector<uint8_t> frameBytes(frameByteCount);
    // Synchronously read the image data, all at once, or line-by-line, byte-by-byte, as you like...
    if (record.reader->read(frameBytes) == 0) {
      /// do your thing with the image...
    }
    return true; // read next blocks, if any
  }
};

/// This class is only here to make the compiler happy.
struct SampleImageReader {
  /// This function is the entry point for your reader
  void imageReader(const string& vrsFilePath) {
    RecordFileReader reader;
    if (reader.openFile(vrsFilePath) == 0) {
      std::vector<std::unique_ptr<StreamPlayer>> streamPlayers;
      // Map the devices referenced in the file to stream player objects
      // Just ignore the device(s) you do not care for
      const std::set<StreamId>& streamIds = reader.getStreams();
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
