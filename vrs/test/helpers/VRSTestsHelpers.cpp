// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include "VRSTestsHelpers.h"

#include <gtest/gtest.h>
#include <array>

#define DEFAULT_LOG_CHANNEL "VRSTestHelpers"
#include <logging/Log.h>

#include <test_helpers/GTestMacros.h>
#include <vrs/os/Time.h>
#include <vrs/os/Utils.h>

#include <vrs/FileCache.h>
#include <vrs/StreamPlayer.h>

using namespace vrs;
using namespace vrs::test;

namespace {

// Detect if there was any attempt to got back in the file
class ForwardDiskFile : public DiskFile {
 public:
  int skipForward(int64_t offset) override {
    if (offset < 0) {
      jumpbackCount_++;
    }
    return DiskFile::skipForward(offset);
  }
  int setPos(int64_t offset) override {
    if (getPos() > offset) {
      jumpbackCount_++;
    }
    return DiskFile::setPos(offset);
  }

  int close() override {
    jumpbackCount_ = 0;
    return DiskFile::close();
  }

  size_t getJumpbackCount() const {
    return jumpbackCount_;
  }

 private:
  size_t jumpbackCount_ = 0;
};

class BlankStreamPlayer : public StreamPlayer {
 public:
  virtual bool processRecordHeader(const CurrentRecord& record, DataReference& outDataReference) {
    buffer_.resize(record.recordSize);
    outDataReference.useVector(buffer_);
    return true;
  }
  virtual void processRecord(const CurrentRecord&, uint32_t) {}
  vector<char> buffer_;
};

struct FrameMetadata : public AutoDataLayout {
  DataPieceValue<uint32_t> cameraIndex{"camera_index"};
  DataPieceValue<uint32_t> frameNumber{"frame_number"};
  DataPieceString str{"some_string"};

  AutoDataLayoutEnd end;
};

class DawnCamera : public Recordable {
 public:
  DawnCamera(uint32_t index, const FileConfig& fileConfig)
      : Recordable(RecordableTypeId::SampleDeviceRecordableClass, kCameraFlavor[index]),
        cameraIndex_(index),
        fileConfig_{fileConfig} {
    setCompression(CompressionPreset::Default);
    addRecordFormat(Record::Type::DATA, 1, frameData_.getContentBlock(), {&frameData_});
    if (index == 0) {
      getRecordManager().setRecordBufferOverAllocationMins(100, 0);
    } else if (index == 1) {
      getRecordManager().setRecordBufferOverAllocationMins(0, 2);
    } else if (index == 2) {
      getRecordManager().setRecordBufferOverAllocationMins(100, 2);
    } else if (index == 3) {
      getRecordManager().setRecordBufferOverAllocationMins(1000, 10);
    }
  }

  const Record* createStateRecord() override {
    // "old" timestamp to force testing out of order records system!
    return createRecord(-1, Record::Type::STATE, kStateVersion);
  }
  void addStateRecord(deque<IndexRecord::DiskRecordInfo>& index) {
    index.push_back({-1, 0, this->getStreamId(), Record::Type::STATE});
  }

  const Record* createConfigurationRecord() override {
    // "old" timestamp to force testing out of order records system!
    return createRecord(-2, Record::Type::CONFIGURATION, kConfigurationVersion);
  }
  void addConfigurationRecord(deque<IndexRecord::DiskRecordInfo>& index) {
    index.push_back({-2, 0, this->getStreamId(), Record::Type::CONFIGURATION});
  }

  const Record* createFrame(uint32_t frameNumber) {
    uint32_t frameSize = getSizeOfFrame(frameNumber);
    std::vector<uint8_t> buffer(frameSize);
    for (uint32_t n = 0; n < frameSize; ++n) {
      buffer[n] = static_cast<uint8_t>(frameNumber ^ (7 * n) ^ (11 * (frameNumber + n)));
    }
    frameData_.cameraIndex.set(cameraIndex_);
    frameData_.frameNumber.set(frameNumber);
    frameData_.str.stage(std::to_string(frameNumber));
    return createRecord(
        getFrameTime(frameNumber),
        Record::Type::DATA,
        kDataVersion,
        DataSource(frameData_, buffer));
  }
  void addFrame(deque<IndexRecord::DiskRecordInfo>& index, uint32_t frameNumber) {
    index.push_back(
        {getFrameTime(frameNumber),
         getSizeOfFrame(frameNumber),
         this->getStreamId(),
         Record::Type::DATA});
  }

  uint32_t getIndex() const {
    return cameraIndex_;
  }

  uint32_t getSizeOfFrame(uint32_t) const {
    return kFrameWidth * kFrameHeight;
  }

  double getFrameTime(size_t frameNumber) {
    return static_cast<double>(frameNumber) / fileConfig_.frameRate;
  }

  uint32_t cameraIndex_;
  const FileConfig& fileConfig_;
  FrameMetadata frameData_;
};

struct ThreadParam {
  RecordFileWriter& fileWriter;
  DawnCamera& camera;
  double startTime;
  std::atomic<int>& myCounter;
  std::atomic<int>& limitCounter;
  bool& fatalError;
  const FileConfig& fileConfig;
  bool realtime;
};

unique_ptr<deque<IndexRecord::DiskRecordInfo>> createPreliminaryIndex(
    std::array<std::unique_ptr<DawnCamera>, kCameraCount>& cameras,
    CreateParams& p) {
  auto preliminaryIndex = std::make_unique<deque<IndexRecord::DiskRecordInfo>>();
  deque<IndexRecord::DiskRecordInfo>& index = *preliminaryIndex;
  for (auto& camera : cameras) {
    camera->addStateRecord(index);
    camera->addConfigurationRecord(index);
  }
  for (uint32_t frame = 0; frame < p.fileConfig.frameCount && index.size() < p.preallocateIndexSize;
       ++frame) {
    for (auto& camera : cameras) {
      camera->addFrame(index, frame);
    }
  }
  if (index.size() > p.preallocateIndexSize) {
    index.resize(p.preallocateIndexSize);
  }
  return preliminaryIndex;
}

void createRecordsThreadTask(ThreadParam* param) {
  param->camera.setRecordableIsActive(true);
  for (uint32_t frame = 0; frame < param->fileConfig.frameCount && !param->fatalError; ++frame) {
    if (param->realtime) {
      double wallTime = os::getTimestampSec() - param->startTime;
      double frameTime = param->camera.getFrameTime(frame);
      if (wallTime < frameTime) {
        const double sleepDuration = frameTime - wallTime;
        std::this_thread::sleep_for(std::chrono::duration<double>(sleepDuration));
      }
    }
    param->camera.createFrame(frame);
    if (param->camera.getIndex() == 0 && ((frame + 1) % kFrameSaveFrequency) == 0 &&
        !param->fatalError) {
      int error =
          param->fileWriter.writeRecordsAsync(param->camera.getFrameTime(frame) - kPrerollTime);
      if (error != 0) {
        param->fatalError = true;
        EXPECT_EQ(error, 0);
        break;
      }
    }
    // each thread has its own counter, and checks that it is not too far ahead of another thread,
    // which could lead to records being written out of order and fail the test
    param->myCounter.operator++();
    while (param->myCounter.load() > param->limitCounter.load() + 2 && !param->fatalError) {
      std::this_thread::yield();
    }
  }
  double wallTime = os::getTimestampSec() - param->startTime;
  XR_LOGD(
      "Thread {} walltime: {} vs {}\n",
      param->camera.getIndex(),
      wallTime,
      param->camera.getFrameTime(param->fileConfig.frameCount));
}

} // namespace

namespace vrs {
namespace test {

void deleteChunkedFile(const std::string& path) {
  FileSpec spec;
  if (RecordFileReader::vrsFilePathToFileSpec(path, spec) == 0) {
    for (const auto& chunk : spec.chunks) {
      os::remove(chunk);
    }
  }
}

void deleteChunkedFile(DiskFile& file) {
  auto chunks = file.getFileChunks();
  file.close();
  for (const auto& chunk : chunks) {
    os::remove(chunk.first);
  }
}

void RecordFileWriterTester::skipFinalizeIndexRecord(RecordFileWriter& writer) {
  writer.skipFinalizeIndexRecords_ = true;
}

int threadedCreateRecords(CreateParams& p) {
  Recordable::resetNewInstanceIds();
  std::array<std::unique_ptr<DawnCamera>, kCameraCount> cameras;
  RecordFileWriter fileWriter;
  fileWriter.setTag("fileTag1", "fileValue1");
  fileWriter.setTag("fileTag2", "fileValue2");
  std::array<std::atomic<int>, kCameraCount> counters;
  std::vector<ThreadParam> threadParams;
  bool fatalError = false;
  for (uint32_t cameraIndex = 0; cameraIndex < kCameraCount; cameraIndex++) {
    cameras[cameraIndex] = std::make_unique<DawnCamera>(cameraIndex, kLongFileConfig);
    fileWriter.addRecordable(cameras[cameraIndex].get());
    counters[cameraIndex] = 0;
    threadParams.push_back(ThreadParam{
        fileWriter,
        *cameras[cameraIndex],
        os::getTimestampSec(),
        counters[cameraIndex],
        counters[(cameraIndex + 1) % kCameraCount],
        fatalError,
        p.fileConfig,
        (p.testOptions & TestOptions::REALTIME) != 0});
  }
  fileWriter.setCompressionThreadPoolSize(p.fileWriterThreadCount);
  if (p.preallocateIndexSize > 0) {
    fileWriter.preallocateIndex(createPreliminaryIndex(cameras, p));
  }
  if (p.customCreateFileFunction) {
    RETURN_ON_FAILURE(p.customCreateFileFunction(p, fileWriter));
  } else if (p.testOptions & TestOptions::SPLIT_HEADER) {
    RETURN_ON_FAILURE(fileWriter.createChunkedFile(p.path, p.maxChunkSizeMB, move(p.chunkHandler)));
  } else {
    fileWriter.setMaxChunkSizeMB(p.maxChunkSizeMB);
    RETURN_ON_FAILURE(fileWriter.createFileAsync(p.path));
  }
  std::vector<std::thread> threads;
  threads.reserve(kCameraCount);
  for (uint32_t threadIndex = 0; threadIndex < kCameraCount; threadIndex++) {
    threads.push_back(std::thread{&createRecordsThreadTask, &threadParams[threadIndex]});
  }
  for (uint32_t threadIndex = 0; threadIndex < kCameraCount; threadIndex++) {
    XR_LOGD("Joining thread #{}", threadIndex);
    threads[threadIndex].join();
  }
  if (p.testOptions & TestOptions::SKIP_FINALIZE_INDEX) {
    RecordFileWriterTester::skipFinalizeIndexRecord(fileWriter);
  }
  XR_LOGD("Closing file");
  EXPECT_EQ(fileWriter.closeFileAsync(), 0);
  threads.clear();
  XR_LOGD("Waiting for file closed");
  RETURN_ON_FAILURE(fileWriter.waitForFileClosed());
  return 0;
}

int singleThreadCreateRecords(CreateParams& p) {
  Recordable::resetNewInstanceIds();
  std::array<std::unique_ptr<DawnCamera>, kCameraCount> cameras;
  RecordFileWriter fileWriter;
  fileWriter.setTag("fileTag1", "fileValue1");
  fileWriter.setTag("fileTag2", "fileValue2");
  for (uint32_t cameraIndex = 0; cameraIndex < kCameraCount; cameraIndex++) {
    cameras[cameraIndex] = std::make_unique<DawnCamera>(cameraIndex, kLongFileConfig);
    DawnCamera* camera = cameras[cameraIndex].get();
    fileWriter.addRecordable(camera);
    camera->setRecordableIsActive(true);
  }
  if (p.preallocateIndexSize > 0) {
    fileWriter.preallocateIndex(createPreliminaryIndex(cameras, p));
  }
  if (p.customCreateFileFunction) {
    RETURN_ON_FAILURE(p.customCreateFileFunction(p, fileWriter));
  } else if (p.testOptions & TestOptions::SPLIT_HEADER) {
    size_t kMB = 1024 * 1024;
    RETURN_ON_FAILURE(
        fileWriter.createChunkedFile(p.path, p.maxChunkSizeMB * kMB, move(p.chunkHandler)));
  } else {
    // When creating records synchronously, config & state records are not automatically inserted.
    for (auto& camera : cameras) {
      camera->createConfigurationRecord();
      camera->createStateRecord();
    }
  }
  if (p.testOptions & TestOptions::SKIP_FINALIZE_INDEX) {
    RecordFileWriterTester::skipFinalizeIndexRecord(fileWriter);
  }
  // Create all the records in this thread
  for (uint32_t frame = 0; frame < p.fileConfig.frameCount; ++frame) {
    for (auto& camera : cameras) {
      camera->createFrame(frame);
    }
  }
  if (p.customCreateFileFunction || (p.testOptions & TestOptions::SPLIT_HEADER)) {
    EXPECT_EQ(fileWriter.closeFileAsync(), 0);
    return fileWriter.waitForFileClosed();
  } else {
    fileWriter.setMaxChunkSizeMB(p.maxChunkSizeMB);
    return fileWriter.writeToFile(p.path);
  }
}

void checkRecordCountAndIndex(const CheckParams& p) {
  FileCache::disableFileCache();
  RecordFileReader reader;
  ForwardDiskFile* diskFile = new ForwardDiskFile();
  reader.setFileHandler(std::unique_ptr<FileHandler>(diskFile));
  int openFileStatus = reader.openFile(p.filePath);
  EXPECT_EQ(openFileStatus, 0);
  if (openFileStatus != 0) {
    return;
  }
  EXPECT_EQ(reader.getIndex().size() + p.truncatedUserRecords, p.fileConfig.totalRecordCount);
  EXPECT_EQ(reader.hasIndex(), p.hasIndex);
  // Make sure we have stream players attached to every stream, to read every record...
  vector<unique_ptr<BlankStreamPlayer>> streamPlayers;

  EXPECT_EQ(reader.getStreams().size(), kCameraCount);
  StreamId topLeftCamera = reader.getStreamForFlavor(
      RecordableTypeId::SampleDeviceRecordableClass, kTopLeftCameraFlavor);
  EXPECT_TRUE(topLeftCamera.isValid());
  EXPECT_TRUE(
      reader
          .getStreamForFlavor(RecordableTypeId::SampleDeviceRecordableClass, kTopRightCameraFlavor)
          .isValid());
  EXPECT_TRUE(reader
                  .getStreamForFlavor(
                      RecordableTypeId::SampleDeviceRecordableClass, kBottomLeftCameraFlavor)
                  .isValid());
  EXPECT_TRUE(reader
                  .getStreamForFlavor(
                      RecordableTypeId::SampleDeviceRecordableClass, kBottomRightCameraFlavor)
                  .isValid());
  EXPECT_EQ(reader.getStreams(RecordableTypeId::SampleDeviceRecordableClass).size(), 4);
  vector<StreamId> ids =
      reader.getStreams(RecordableTypeId::SampleDeviceRecordableClass, kTopLeftCameraFlavor);
  EXPECT_EQ(ids.size(), 1);
  EXPECT_EQ(ids[0], topLeftCamera);
  uint32_t index = 0;
  for (auto id : reader.getStreams()) {
    streamPlayers.emplace_back(new BlankStreamPlayer());
    reader.setStreamPlayer(id, streamPlayers.back().get());
    EXPECT_EQ(reader.getFlavor(id), kCameraFlavor[index++]);
  }

  reader.readAllRecords();
  EXPECT_EQ(diskFile->getJumpbackCount(), p.jumpbackCount);

  // if the file has no valid index, rebuild one & check again
  if (!p.hasIndex) {
    openFileStatus = reader.openFile(p.filePath, true);
    EXPECT_EQ(openFileStatus, 0);
    if (openFileStatus != 0) {
      return;
    }
    EXPECT_EQ(reader.hasIndex(), true);
    reader.readAllRecords();
    EXPECT_EQ(diskFile->getJumpbackCount(), p.jumpbackCountAfterFixingIndex);
  }
}

} // namespace test
} // namespace vrs
