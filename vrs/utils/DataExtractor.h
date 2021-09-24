// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <fstream>
#include <map>
#include <memory>

#include <vrs/RecordFileReader.h>
#include <vrs/utils/VideoRecordFormatStreamPlayer.h>

namespace vrs::utils {

class DataExtractor {
 public:
  /// One-stop export-all functionality based, with minimal control.
  /// Note: if there are existing files in the output folder, they will be overwritten
  /// if the file names collide, but existing files will remain.
  /// Clearing the output folder is the caller's responsibility.
  static int extractAll(const string& vrsFilePath, const string& outputFolder);

  /// Create a data extractor for a VRS file reader, with a target output folder
  /// The constructor doesn't actually extract anything.
  /// @param reader: a VRS file reader, expected to be open.
  /// @param outputFolder: absolute or relative path to a folder for the extracted data.
  DataExtractor(RecordFileReader& reader, const string& outputFolder);
  ~DataExtractor();

  /// Tell which streams should be extracted. By default, no stream will be extracted.
  /// @param id: A stream identifier, found in 'reader.getStreams()'.
  void extract(StreamId id);

  /// Start the extraction process, by creating the main files.
  /// However, no record is actually extracted, which is the caller's responsibility.
  /// If the whole file, should be extracted, simply call 'reader.readAllRecords()'.
  /// If a stream was registered using the 'extract(id)' call, it will be ignored.
  /// @return A status code, 0 representing success.
  int createOutput();

  /// Finalize the data extraction process, close the files, etc.
  /// @return A status code, 0 representing success.
  int completeOutput();

 protected:
  set<StreamId> getStreams() const;

  class DataExtractorStreamPlayer : public utils::VideoRecordFormatStreamPlayer {
   public:
    DataExtractorStreamPlayer(std::ofstream& output, const string& outputFolder);
    bool onDataLayoutRead(const CurrentRecord&, size_t blockIndex, DataLayout&) override;
    bool onImageRead(const CurrentRecord&, size_t blockIndex, const ContentBlock&) override;
    bool onCustomBlockRead(const CurrentRecord&, size_t blockIndex, const ContentBlock&) override;
    bool onUnsupportedBlock(const CurrentRecord&, size_t blockIndex, const ContentBlock&) override;

    int recordReadComplete(RecordFileReader&, const IndexRecord::RecordInfo&) override;

    bool writeImage(const CurrentRecord&, const ImageContentBlockSpec&, const vector<uint8_t>&);
    void writePngImage(const CurrentRecord&);

    void wroteImage(const string& filename);

   protected:
    std::ofstream& output_;
    const string outputFolder_;
    std::vector<string> blocks_;
    std::shared_ptr<PixelFrame> inputFrame_;
    std::shared_ptr<PixelFrame> processedFrame_;
    uint32_t imageCounter_ = 0;
  };

 private:
  RecordFileReader& reader_;
  const string& outputFolder_;
  std::ofstream output_;
  map<StreamId, unique_ptr<DataExtractorStreamPlayer>> extractors_;
};

} // namespace vrs::utils
