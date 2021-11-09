// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include <program_options/ProgramOptions.h>

#define DEFAULT_LOG_CHANNEL "FolderToVRS"
#include <logging/Log.h>
#include <logging/Verify.h>

#include <vrs/DataLayout.h>
#include <vrs/DataLayoutConventions.h>
#include <vrs/DataPieces.h>
#include <vrs/DiskFile.h>
#include <vrs/RecordFileWriter.h>
#include <vrs/gaia/GaiaClient.h>
#include <vrs/gaia/GaiaUploader.h>
#include <vrs/gaia/archive/FileList.h>
#include <vrs/helpers/FileMacros.h>
#include <vrs/helpers/Strings.h>
#include <vrs/os/Utils.h>
#include <vrs/utils/PixelFrame.h>
#include <vrs/utils/RecordFileInfo.h>

/// This utility create a VRS from a folder ful of png files, sorted alphabetically.
/// The records will contain the pixel data in raw format, if PixelFrame was able to decode the
/// image. Configuration records contain the image dimensions and pixel format. If the png files
/// don't have the same dimensions & pixel format, then configuration records will be inserted
/// before each data record, as needed.

namespace po = boost::program_options;
using namespace std;
using namespace vrs;
using namespace vrs::DataLayoutConventions;

class ConfigDataLayout : public AutoDataLayout {
 public:
  DataPieceValue<ImageSpecType> width{kImageWidth};
  DataPieceValue<ImageSpecType> height{kImageHeight};
  DataPieceValue<ImageSpecType> stride{kImageStride};
  DataPieceEnum<PixelFormat, ImageSpecType> pixelFormat{kImagePixelFormat};

  AutoDataLayoutEnd end;
};

class DataDataLayout : public AutoDataLayout {
 public:
  DataPieceString fileName{"file_name"};

  AutoDataLayoutEnd end;
};

class ImageStream : public Recordable {
  static const uint32_t kConfigurationRecordFormatVersion = 1;
  static const uint32_t kDataRecordFormatVersion = 1;

 public:
  ImageStream(uint32_t fps, CompressionPreset preset)
      : Recordable(RecordableTypeId::ImageStream, "test/folder_to_vrs") {
    // Tell how the records of this stream should be compressed (or not)
    setCompression(preset);
    // Extremly important: Define the format of this stream's records
    addRecordFormat(
        Record::Type::CONFIGURATION,
        kConfigurationRecordFormatVersion,
        // the following describe config records' format: a single datalayout content block
        config_.getContentBlock(),
        {&config_});
    addRecordFormat(
        Record::Type::DATA,
        kDataRecordFormatVersion,
        // the following describe data records' format: a datalayout content block + pixel data
        data_.getContentBlock() + ContentBlock(ImageFormat::RAW),
        {&data_});
    timeIncrement_ = 1. / fps;
  }
  const Record* createConfigurationRecord() override {
    return nullptr;
  }
  void createConfigurationRecord(const utils::PixelFrame& pixels) {
    const ImageContentBlockSpec& spec = pixels.getSpec();
    if (config_.width.get() == spec.getWidth() && config_.height.get() == spec.getHeight() &&
        config_.stride.get() == spec.getStride() &&
        config_.pixelFormat.get() == spec.getPixelFormat()) {
      return;
    }
    config_.width.set(spec.getWidth());
    config_.height.set(spec.getHeight());
    config_.stride.set(spec.getStride());
    config_.pixelFormat.set(spec.getPixelFormat());

    // create a record using that data
    createRecord(
        getTime(),
        Record::Type::CONFIGURATION,
        kConfigurationRecordFormatVersion,
        DataSource(config_));
  }
  const Record* createStateRecord() override {
    // Best practice is to always create a record when asked, with a reasonable timestamp,
    // even if the record is empty.
    return createRecord(getTime(), Record::Type::STATE, 0);
  }
  // When an image is captured, create a record for it
  int createDataRecord(const string& filePath) {
    DiskFile file;
    IF_ERROR_LOG_AND_RETURN(file.open(filePath));
    std::vector<uint8_t> buffer(file.getTotalSize());
    IF_ERROR_LOG_AND_RETURN(file.read(buffer.data(), buffer.size()));
    if (!XR_VERIFY(pixels_.readPngFrame(buffer))) {
      return -1;
    }
    data_.fileName.stage(os::getFilename(filePath));
    return createDataRecord(pixels_);
  }
  // When an image is captured, create a record for it
  int createDataRecord(const utils::PixelFrame& pixels) {
    // won't do anything if the spec don't change
    createConfigurationRecord(pixels);
    // create a record using that (fake) data
    createRecord(
        getTime(true),
        Record::Type::DATA,
        kDataRecordFormatVersion,
        DataSource(data_, {pixels.rdata(), pixels.size()}));
    return 0;
  }
  double getTime(bool increment = false) {
    double t = time_;
    if (increment) {
      time_ += timeIncrement_;
    }
    return t;
  }

 private:
  // DataLayout objects aren't super cheap to create, so we reuse the same instances every time
  utils::PixelFrame pixels_;
  ConfigDataLayout config_;
  DataDataLayout data_;
  double time_ = 0;
  double timeIncrement_;
};

int main(int argc, char** argv) {
  ::arvr::logging::setGlobalLogLevel(arvr::logging::Level::Info);
  AutoGaiaClientInit gaiaInit;

  string sourceFolder, destinationFile;
  string compressionPreset;
  string gaiaProject;
  vector<string> tags;
  string gaiaDescription;
  uint32_t fps;

  arvr::ProgramOptions options(arvr::ProgramOptions::OverrideConfigFlags::Allow, {});
  // clang-format off
  options.addOptions()
      ("source,s",
        po::value<std::string>(&sourceFolder)->required(),
        "Source folder.")
      ("destination,d",
        po::value<std::string>(&destinationFile)->required(),
        "Destination file.")
      ("project",
        po::value<std::string>(&gaiaProject),
        "Optional Gaia project name to upload to.")
      ("tag",
        boost::program_options::value<std::vector<string>>(&tags)->multitoken()->zero_tokens(),
        "Gaia tags on upload.")
      ("description",
        po::value<std::string>(&gaiaDescription),
        "Optional Gaia description.")
      ("compression",
        po::value<std::string>(&compressionPreset)->default_value("zmedium"),
        "Compression preset: [none|fast|tight|zfast|zlight|zmedium|ztight|zmax].")
      ("fps",
        po::value<uint32_t>(&fps)->default_value(25),
        "Number of frames per seconds in the target VRS file.")
      ;
  // clang-format on
  if (!options.parse(argc, argv, arvr::ProgramOptions::UnrecognizedOption::Error)) {
    return -1;
  }
  if (sourceFolder.empty() || !os::isDir(sourceFolder)) {
    cerr << "Usage error: The source path must be a folder with images in it." << endl;
    return -1;
  }
  if (destinationFile.empty() ||
      (os::pathExists(destinationFile) && !os::isFile(destinationFile))) {
    cerr << "Usage error: The destination path must be a file or a new file." << endl;
    return -1;
  }
  vector<string> files;
  IF_ERROR_LOG_AND_RETURN(getFileList(sourceFolder, files, 0));
  if (files.empty()) {
    cerr << "Usage error: Found no files in source folder." << endl;
    return -1;
  }
  CompressionPreset preset = toEnum<CompressionPreset>(compressionPreset);
  if (preset == CompressionPreset::Undefined) {
    cerr << "Usage error: invalid compression preset." << endl;
    return -1;
  }

  ImageStream images(fps, preset);
  RecordFileWriter outputFile;
  outputFile.addRecordable(&images);
  outputFile.setCompressionThreadPoolSize();
  outputFile.trackBackgroundThreadQueueByteSize();
  unique_ptr<GaiaUploader> uploader;
  UploadId uploadId{};
  if (!gaiaProject.empty()) {
    uploader = make_unique<GaiaUploader>();
    auto uploadMetadata = make_unique<UploadMetadata>();
    uploadMetadata->setProject(gaiaProject);
    uploadMetadata->setFileName(os::getFilename(destinationFile));
    uploadMetadata->setTags(tags);
    uploadMetadata->setDescription(gaiaDescription);
    IF_ERROR_LOG_AND_RETURN(
        uploader->stream(move(uploadMetadata), outputFile, destinationFile, uploadId));
  } else {
    IF_ERROR_LOG_AND_RETURN(outputFile.createFileAsync(destinationFile));
  }
  const size_t kMaxBackGroundSize = 2 * 1000 * 1000 * 1000;
  for (const auto& path : files) {
    if (helpers::endsWith(path, ".png")) {
      cout << "Adding " << path << endl;
      images.createDataRecord(path);
      outputFile.writeRecordsAsync(images.getTime());
      while (outputFile.getBackgroundThreadQueueByteSize() > kMaxBackGroundSize) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    }
  }
  IF_ERROR_LOG_AND_RETURN(outputFile.waitForFileClosed());
  if (uploader) {
    cout << "File creation complete, finishing upload..." << endl;
    GaiaId gaiaId;
    int uploadStatus = uploader->finishUpload(uploadId, gaiaId);
    if (uploadStatus != 0) {
      cerr << "Upload failed: " << errorCodeToMessage(uploadStatus) << endl;
    } else {
      cout << "Upload complete! New Gaia object: " << gaiaIdToUri(gaiaId) << endl;
    }
  } else {
    vrs::RecordFileInfo::printOverview(cout, destinationFile);
  }

  return 0;
}
