// Facebook Technologies, LLC Proprietary and Confidential.

#define BOOST_USE_WINDOWS_H

#include "XprsEncoder.h"

#include <iostream>

#if USE_OCEAN
#include <ocean/base/Frame.h>
#include <ocean/cv/FrameConverter.h>
#endif

#define DEFAULT_LOG_CHANNEL "XprsEncoder"
#include <logging/Checks.h>
#include <logging/Log.h>
#include <logging/Verify.h>
#include <vrs/utils/converters/Raw10ToGrey10Converter.h>

#include "XprsManager.h"

using namespace std;

namespace {
template <class T>
T* addField(unique_ptr<vrs::ManualDataLayout>& dl, const char* name) {
  unique_ptr<T> uniqueField = make_unique<T>(name);
  T* fieldPointer = uniqueField.get();
  dl->add(move(uniqueField));
  return fieldPointer;
}
} // namespace

namespace vrs::vxprs {

using namespace DataLayoutConventions;
using xprs::XprsResult;

XprsEncoder::XprsEncoder(
    RecordFileReader& fileReader,
    RecordFileWriter& fileWriter,
    StreamId id,
    const utils::CopyOptions& copyOptions,
    const EncoderOptions& encoderOptions,
    BlockId imageSpecBlock,
    BlockId pixelBlock)
    : RecordFilterCopier(fileReader, fileWriter, id, copyOptions),
      encoderOptions_{encoderOptions},
      imageSpecBlock_{imageSpecBlock},
      pixelBlock_{pixelBlock},
      encodeThreadReady_{1},
      encodeJobReady_{0},
      copyComplete_{false},
      thread_{&XprsEncoder::encodeThread, this} {
  const uint32_t kInvalidFormatVersion = 0xffffffff;
  struct RecordDefinitionsCollector : public RecordFormatStreamPlayer {
    RecordDefinitionsCollector(XprsEncoder& encoder) : parent{encoder} {}
    unique_ptr<ManualDataLayout> imageSpecCustomDataLayout, keyFrameCustomDataLayout;
    RecordFormat recordFormat;
    vector<const DataLayout*> dataLayouts;
    uint32_t formatVersion = kInvalidFormatVersion;
    double timestamp = 0;
    void clear() {
      formatVersion = kInvalidFormatVersion;
      recordFormat.clear();
      dataLayouts.clear();
    }
    void addDataLayout(const DataLayout* dataLayout) {
      recordFormat + dataLayout->getContentBlock();
      dataLayouts.push_back(dataLayout);
    }
    void addContentBlock(const ContentBlock& contentBlock) {
      recordFormat + contentBlock;
      dataLayouts.push_back(nullptr);
    }
    bool onDataLayoutRead(const CurrentRecord& record, size_t idx, DataLayout& dl) override {
      BlockId thisBlock{record, idx};
      timestamp = record.timestamp;
      formatVersion = record.formatVersion;
      if (thisBlock == parent.imageSpecBlock_) {
        // Clone layout & add xprs codec specific action field
        imageSpecCustomDataLayout = make_unique<ManualDataLayout>(dl);
        parent.codecNamePiece_ =
            addField<DataPieceString>(imageSpecCustomDataLayout, kImageCodecName);
        if (thisBlock.isRightBefore(parent.pixelBlock_)) {
          parent.keyFrameIndexPiece_ = addField<DataPieceValue<ImageSpecType>>(
              imageSpecCustomDataLayout, kImageKeyFrameIndex);
          parent.keyFrameTimestampPiece_ =
              addField<DataPieceValue<double>>(imageSpecCustomDataLayout, kImageKeyFrameTimeStamp);
        }
        imageSpecCustomDataLayout->endLayout();
        addDataLayout(imageSpecCustomDataLayout.get());
      } else if (thisBlock.isRightBefore(parent.pixelBlock_)) {
        keyFrameCustomDataLayout = make_unique<ManualDataLayout>(dl);
        parent.keyFrameIndexPiece_ =
            addField<DataPieceValue<ImageSpecType>>(keyFrameCustomDataLayout, kImageKeyFrameIndex);
        parent.keyFrameTimestampPiece_ =
            addField<DataPieceValue<double>>(keyFrameCustomDataLayout, kImageKeyFrameTimeStamp);
        keyFrameCustomDataLayout->endLayout();
        addDataLayout(keyFrameCustomDataLayout.get());
      } else {
        addDataLayout(&dl);
      }
      return true;
    }
    bool onImageRead(const CurrentRecord& record, size_t index, const ContentBlock&) override {
      BlockId block{record, index};
      if (block == parent.pixelBlock_) {
        addContentBlock(ContentBlock(ImageFormat::VIDEO));
      } else {
        addContentBlock(getOfficialContentBlock(index));
      }
      return true;
    }
    bool onUnsupportedBlock(const CurrentRecord&, size_t index, const ContentBlock&) override {
      addContentBlock(getOfficialContentBlock(index));
      return true;
    }
    const ContentBlock& getOfficialContentBlock(size_t contentBlockIndex) {
      return getCurrentRecordFormatReader()->recordFormat.getContentBlock(contentBlockIndex);
    }
    XprsEncoder& parent;
  } collector(*this);
  fileReader.readFirstConfigurationRecord(id, &collector);
  writer_.addRecordFormat(
      Record::Type::CONFIGURATION,
      collector.formatVersion,
      collector.recordFormat,
      collector.dataLayouts);
  XR_LOGI("Configuration record format: {}", collector.recordFormat.asString());
  collector.clear();
  const IndexRecord::RecordInfo* dataRec =
      fileReader.getRecordByTime(id, Record::Type::DATA, collector.timestamp);
  if (dataRec) {
    fileReader.readRecord(*dataRec, &collector);
    startTime_ = dataRec->timestamp;
  }
  writer_.addRecordFormat(
      Record::Type::DATA, collector.formatVersion, collector.recordFormat, collector.dataLayouts);
  XR_LOGI("Data record format: {}", collector.recordFormat.asString());
  imageSpecCustomDataLayout_ = move(collector.imageSpecCustomDataLayout);
  keyFrameCustomDataLayout_ = move(collector.keyFrameCustomDataLayout);
}

XprsEncoder::~XprsEncoder() {
  encodeThreadReady_.wait();
  copyComplete_ = true;
  encodeJobReady_.post();
  if (thread_.joinable()) {
    thread_.join();
  }
}

void XprsEncoder::processRecord(const CurrentRecord& record, uint32_t readSize) {
  encodeThreadReady_.wait();
  sourceImage_.reset();
  chunks_.clear();
  finalChunks_.clear();
  recordTime_ = record.timestamp;
  recordType_ = record.recordType;
  formatVersion_ = record.formatVersion;
  RecordFilterCopier::processRecord(record, readSize);
}

void XprsEncoder::finishRecordProcessing(const CurrentRecord& record) {
  if (skipRecord_) {
    encodeThreadReady_.post();
  } else if (copyVerbatim_) {
    writer_.createRecord(record, verbatimRecordData_);
    encodeThreadReady_.post();
  } else if (sourceImage_) {
    encodeJobReady_.post();
  } else {
    // no image to process (config record?): let's just create the record
    utils::FilteredChunksSource chunkedSource(chunks_);
    writer_.createRecord(record, chunkedSource);
    encodeThreadReady_.post();
  }
}

void XprsEncoder::flush() {
  encodeThreadReady_.wait();
  encodeThreadReady_.post();
}

bool XprsEncoder::shouldCopyVerbatim(const CurrentRecord& record) {
  return record.recordType == Record::Type::STATE;
}

bool XprsEncoder::onDataLayoutRead(const CurrentRecord& record, size_t idx, DataLayout& dl) {
  BlockId thisBlock{record, idx};
  if (thisBlock == imageSpecBlock_) {
    ImageSpec& imageConfig = getExpectedLayout<ImageSpec>(dl, idx);
    ContentBlock imageBlock = imageConfig.getImageContentBlock(ImageFormat::RAW);
    const ImageContentBlockSpec& spec = imageBlock.image();
    if (!encoder_ || !matchEncoderConfig(spec)) {
      if (imageSpecToVideoCodec(
              spec, encoderOptions_, codecName_, codec_, encoderConfig_, &encoder_)) {
        XR_LOGI(
            "Encoding to {} implemented by {} codec using {}.",
            codecName_,
            codec_.implementationName,
            getPixelFormatName(encoderConfig_.encodeFmt));
        pixelFormat_ = spec.getPixelFormat();
      } else {
        codecName_.clear();
        XR_LOGW("Found no codec compatible with {}", toString(spec.getPixelFormat()));
      }
      keyFrameIndexValue_ = 0;
      keyFrameTimestampValue_ = 0;
    }
    imageSpecCustomDataLayout_->copyClonedDataPieceValues(dl);
    PixelFormat targetPixelFormat =
        (spec.getPixelFormat() == PixelFormat::RAW10) ? PixelFormat::GREY10 : spec.getPixelFormat();
    // encoding transformations
    DataPieceValue<ImageSpecType>* param;
    if ((param = imageSpecCustomDataLayout_->findDataPieceValue<ImageSpecType>(
             DataLayoutConventions::kImageStride)) != nullptr) {
      // When decoded, images will have their default stride value. Update to that value.
      ImageContentBlockSpec targetSpec{
          spec.getImageFormat(), targetPixelFormat, spec.getWidth(), spec.getHeight()};
      param->set(targetSpec.getStride());
    }
    if ((param = imageSpecCustomDataLayout_->findDataPieceValue<ImageSpecType>(
             DataLayoutConventions::kImagePixelFormat)) != nullptr) {
      param->set(static_cast<ImageSpecType>(targetPixelFormat));
    }
    codecNamePiece_->stage(codecName_);
    if (!thisBlock.isRightBefore(pixelBlock_)) {
      pushDataLayout(*imageSpecCustomDataLayout_);
    }
  } else if (thisBlock.isRightBefore(pixelBlock_)) {
    keyFrameCustomDataLayout_->copyClonedDataPieceValues(dl);
  } else {
    chunks_.emplace_back(make_unique<utils::ContentChunk>(dl));
  }
  return true;
}

bool XprsEncoder::onImageRead(
    const CurrentRecord& record,
    size_t blockIndex,
    const ContentBlock& cb) {
  if (record.timestamp < startTime_) {
    XR_LOGW(
        "Image at {} from {} is before a config record. Skipping...",
        record.timestamp,
        record.streamId.getName());
    skipRecord();
    return false;
  }
  BlockId thisBlock{record, blockIndex};
  const auto& imageSpec = cb.image();
  sourceImage_ = make_unique<utils::ContentBlockChunk>(cb, record);
  if (thisBlock != pixelBlock_ || !encoder_ ||
      !XR_VERIFY(cb.image().getImageFormat() == ImageFormat::RAW) ||
      !XR_VERIFY(matchEncoderConfig(imageSpec))) {
    skipRecord();
    return false;
  }
  finalChunks_.swap(chunks_); // Chunks before the datalayout & image can be moved to finalChunks_
  return true;
}

bool XprsEncoder::matchEncoderConfig(const ImageContentBlockSpec& spec) const {
  return encoderConfig_.width == spec.getWidth() && encoderConfig_.height == spec.getHeight() &&
      pixelFormat_ == spec.getPixelFormat();
}

void XprsEncoder::encodeThread() {
  while (!copyComplete_) {
    encodeJobReady_.wait();
    if (!copyComplete_) {
      unique_ptr<DataLayout>& customLayout =
          keyFrameCustomDataLayout_ ? keyFrameCustomDataLayout_ : imageSpecCustomDataLayout_;
      XprsResult result = XprsResult::ERR_GENERIC;
      xprs::Frame frame;
      uint8_t* readData = sourceImage_->getBuffer().data();
      const auto& imageSpec = sourceImage_->getContentBlock().image();
      if (imageSpec.getPixelFormat() == PixelFormat::RAW10) {
        ImageContentBlockSpec convertedSpec{
            PixelFormat::GREY10, imageSpec.getWidth(), imageSpec.getHeight()};
        convertedFrame_.resize(convertedSpec.getBlockSize());
        utils::convertRaw10ToGrey10(
            convertedFrame_.data(),
            readData,
            imageSpec.getWidth(),
            imageSpec.getHeight(),
            imageSpec.getStride());
        setupFrame(frame, convertedSpec, convertedFrame_.data());
      } else if (
          imageSpec.getPixelFormat() == PixelFormat::RGB8 &&
          encoderConfig_.encodeFmt == xprs::PixelFormat::GBRP) {
        const uint32_t width = imageSpec.getWidth();
        const uint32_t height = imageSpec.getHeight();
        const uint32_t planeSize = width * height;
        convertedFrame_.resize(planeSize * 3);
        // ffmpeg needs de-interleaved planes
        uint8_t* rPlane = convertedFrame_.data();
        uint8_t* gPlane = rPlane + planeSize;
        uint8_t* bPlane = gPlane + planeSize;
        frame.planes[0] = gPlane;
        frame.stride[0] = width;
        frame.planes[1] = bPlane;
        frame.stride[1] = width;
        frame.planes[2] = rPlane;
        frame.stride[2] = width;
        uint32_t imageStride = imageSpec.getStride();
        for (uint32_t h = 0; h < height; h++) {
          const uint8_t* src = readData;
          for (uint32_t w = 0; w < width; w++) {
            *rPlane++ = *src++;
            *gPlane++ = *src++;
            *bPlane++ = *src++;
          }
          readData += imageStride;
        }
        frame.numPlanes = 3;
      } else if (
          imageSpec.getPixelFormat() == PixelFormat::RGB8 &&
          encoderConfig_.encodeFmt == xprs::PixelFormat::YUV444P) {
#if USE_OCEAN
        const uint32_t width = imageSpec.getWidth();
        const uint32_t height = imageSpec.getHeight();
        convertedFrame_.resize(width * height * 3); // safe to hardcode
        using namespace Ocean;
        // Try to create an Ocean-style source frame
        const FrameType sourceFrameType(
            width, height, FrameType::FORMAT_RGB24, FrameType::ORIGIN_UPPER_LEFT);
        Frame sourceFrame(sourceFrameType, readData, Frame::CM_USE_KEEP_LAYOUT);
        // Create an Ocean-style target frame
        const FrameType targetFrameType(
            width, height, FrameType::FORMAT_YUV24, FrameType::ORIGIN_UPPER_LEFT);
        Frame targetFrame(targetFrameType, convertedFrame_.data(), Frame::CM_USE_KEEP_LAYOUT);
        XR_VERIFY(CV::FrameConverter::convert(
            sourceFrame, targetFrameType, targetFrame, CV::FrameConverter::CP_ALWAYS_COPY));
        XR_VERIFY(!targetFrame.isPlaneOwner()); // Beware of Ocean's backstabbing behaviors!
        // ffmpeg de-interleaved planes. Copy pixel data back in the original buffer.
        const uint32_t planeSize = width * height;
        uint8_t* yPlane = readData;
        uint8_t* uPlane = yPlane + planeSize;
        uint8_t* vPlane = uPlane + planeSize;
        const uint8_t* src = convertedFrame_.data();
        frame.planes[0] = yPlane;
        frame.stride[0] = width;
        frame.planes[1] = uPlane;
        frame.stride[1] = width;
        frame.planes[2] = vPlane;
        frame.stride[2] = width;
        for (uint32_t count = 0; count < planeSize; count++) {
          *yPlane++ = *src++;
          *uPlane++ = *src++;
          *vPlane++ = *src++;
        }
        frame.numPlanes = 3;
#else
        XR_LOGW("Ocean RGB8 conversion not available...");
        setupFrame(frame, imageSpec, readData);
#endif
      } else {
        setupFrame(frame, imageSpec, readData);
      }
      frame.ptsMs = static_cast<xprs::TimeStamp>((recordTime_ - startTime_) * 1000); // ms
      frame.fmt = encoderConfig_.encodeFmt;
      frame.width = encoderConfig_.width;
      frame.height = encoderConfig_.height;
      frame.keyFrame = false;
      XR_CHECK(xprs::getNumPlanes(frame.fmt) == frame.numPlanes);

      xprs::EncoderOutput encodedFrame;
      result = encoder_->encodeFrame(encodedFrame, frame);
      XR_CHECK(result == XprsResult::OK);
      if (encodedFrame.isKey) {
        keyFrameIndexValue_ = 0;
        keyFrameTimestampValue_ = recordTime_;
      } else {
        keyFrameIndexValue_++;
      }
      keyFrameIndexPiece_->set(keyFrameIndexValue_);
      keyFrameTimestampPiece_->set(keyFrameTimestampValue_);
      customLayout->collectVariableDataAndUpdateIndex();
      finalChunks_.emplace_back(make_unique<vrs::utils::ContentChunk>(*customLayout));
      vector<uint8_t> buffer(
          encodedFrame.buffer.data, encodedFrame.buffer.data + encodedFrame.buffer.size);
      finalChunks_.emplace_back(
          make_unique<utils::ContentBlockChunk>(ImageFormat::VIDEO, move(buffer)));
      // don't forget the content block chunks that came after the image (if any)
      for (auto& chunk : chunks_) {
        finalChunks_.emplace_back(move(chunk));
      }
      chunks_.clear();
      utils::FilteredChunksSource chunkedSource(finalChunks_);
      writer_.createRecord(recordTime_, recordType_, formatVersion_, chunkedSource);
    }
    encodeThreadReady_.post();
  }
}

void XprsEncoder::setupFrame(
    xprs::Frame& frame,
    const ImageContentBlockSpec& imageSpec,
    uint8_t* pixelBuffer) {
  const uint32_t planeCount = imageSpec.getPlaneCount();
  for (uint32_t p = 0; p < planeCount; p++) {
    frame.planes[p] = pixelBuffer;
    frame.stride[p] = imageSpec.getPlaneStride(p);
    pixelBuffer += frame.stride[p] * imageSpec.getPlaneHeight(p);
  }
  frame.numPlanes = planeCount;
}

static vector<xprs::PixelFormat> vrsToXprsPixelFormats(PixelFormat vrsPixelFormat) {
  switch (vrsPixelFormat) {
    case PixelFormat::GREY8:
      return {xprs::PixelFormat::GRAY8};
    case PixelFormat::GREY10:
      return {xprs::PixelFormat::GRAY10LE};
    case PixelFormat::GREY12:
      return {xprs::PixelFormat::GRAY12LE};
    case PixelFormat::RGB8:
      return {
        xprs::PixelFormat::GBRP,
#if USE_OCEAN
            xprs::PixelFormat::YUV444P, // Using Ocean to convert
#endif
      };
    case PixelFormat::RAW10:
      return {xprs::PixelFormat::GRAY10LE}; // because we convert
    case PixelFormat::YUV_I420_SPLIT:
      return {xprs::PixelFormat::YUV420P};
    default:
      return {};
  }
  return {};
}

static bool contains(const xprs::PixelFormatList& formats, xprs::PixelFormat pixelFormat) {
  return find(formats.begin(), formats.end(), pixelFormat) != formats.end();
}

bool imageSpecToVideoCodec(
    const ImageContentBlockSpec& spec,
    const EncoderOptions& encoderOptions,
    string& outCodecName,
    xprs::VideoCodec& outVideoCodec,
    xprs::EncoderConfig& outEncoderConfig,
    unique_ptr<xprs::IVideoEncoder>* inOutEncoder) {
  outEncoderConfig.width = spec.getWidth();
  outEncoderConfig.height = spec.getHeight();
  outEncoderConfig.keyDistance = encoderOptions.keyframeDistance;
  outEncoderConfig.quality = encoderOptions.quality;
  outEncoderConfig.preset = encoderOptions.preset;
  // let's find an appropriate codec. This list is sorted in preferrence order.
  vector<xprs::VideoCodecFormat> codecFormats = {
      xprs::VideoCodecFormat::H264, xprs::VideoCodecFormat::H265, xprs::VideoCodecFormat::VP9};
  for (auto format : codecFormats) {
    xprs::CodecList codecList;
    if (!XR_VERIFY(xprs::enumEncodersByFormat(codecList, format) == XprsResult::OK) ||
        !XR_VERIFY(xprs::getNameFromVideoCodecFormat(outCodecName, format) == XprsResult::OK)) {
      continue;
    }
    for (const auto& codec : codecList) {
      if (!encoderOptions.codecNameSearchStr.empty() &&
          outCodecName.find(encoderOptions.codecNameSearchStr) == string::npos) {
        continue;
      }
      // Look for a xprs::PixelFormat to use, such that:
      // - the codec supports it
      // - we have a conversion between the vrs::PixelFormat and the xprs::PixelFormat
      // - the encoder options allow for that xprs::PixelFormat
      xprs::PixelFormatList codecPixelFormats;
      if (!XR_VERIFY(
              xprs::enumPixelFormats(codecPixelFormats, codec.implementationName) ==
              XprsResult::OK) ||
          codecPixelFormats.empty()) {
        continue;
      }
      for (xprs::PixelFormat pixelFormat : vrsToXprsPixelFormats(spec.getPixelFormat())) {
        if (!contains(codecPixelFormats, pixelFormat) ||
            (!encoderOptions.pixelFormats.empty() &&
             !contains(encoderOptions.pixelFormats, pixelFormat))) {
          continue;
        }
        outEncoderConfig.encodeFmt = pixelFormat;

        outVideoCodec = codec;
        if (inOutEncoder == nullptr) {
          // we found a candidate, but we're not asked to instantiate the encoder, we're done!
          return true;
        }
        // try to instantiate the encoder, which could fail despite the validation...
        inOutEncoder->reset(xprs::createEncoder(codec));
        xprs::IVideoEncoder* encoder = inOutEncoder->get();
        if (encoder != nullptr && encoder->init(outEncoderConfig, "") == XprsResult::OK) {
          return true;
        }
        inOutEncoder->reset();
      }
    }
  }
  return false;
}

} // namespace vrs::vxprs
