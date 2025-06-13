#include <iostream>
#include <string>
#include <unordered_set>

#define DEFAULT_LOG_CHANNEL "SampleVRSFilterCopy"
#include <logging/Log.h>
#include <vrs/RecordFileReader.h>
#include <vrs/RecordFileWriter.h>

using namespace std;
using namespace vrs;

/// Helper: Parse RecordableTypeId from string
optional<RecordableTypeId> parseType(const string& typeStr) {
  auto id = RecordableTypeId::fromString(typeStr);
  return id == RecordableTypeId::Undefined ? nullopt : optional{id};
}

/// Sample app to copy a VRS file, excluding specific stream types if provided.
int main(int argc, char* argv[]) {
  if (argc < 3) {
    XR_LOGE("Usage: {} <input.vrs> <output.vrs> [excludedType1 excludedType2 ...]", argv[0]);
    return 1;
  }

  const string inputFile = argv[1];
  const string outputFile = argv[2];
  unordered_set<RecordableTypeId> excludedTypes;

  // Parse optional excluded types
  for (int i = 3; i < argc; ++i) {
    auto maybeType = parseType(argv[i]);
    if (maybeType) {
      excludedTypes.insert(*maybeType);
    } else {
      XR_LOGW("Unknown stream type: {}", argv[i]);
    }
  }

  RecordFileReader reader;
  int status = reader.openFile(inputFile);
  if (status != 0) {
    XR_LOGE("Failed to open input VRS file '{}': {}", inputFile, errorCodeToMessage(status));
    return status;
  }

  RecordFileWriter writer;

  for (const auto& streamEntry : reader.getStreams()) {
    const auto& streamId = streamEntry.first;
    const auto typeId = streamId.getTypeId();

    if (excludedTypes.count(typeId)) {
      XR_LOGI("Excluding stream: {}", typeId.toString());
      continue;
    }

    int copyStatus = writer.copyStream(reader, streamId);
    if (copyStatus != 0) {
      XR_LOGW("Failed to copy stream {}: {}", streamId.getName(), errorCodeToMessage(copyStatus));
    }
  }

  status = writer.writeFile(outputFile);
  if (status != 0) {
    XR_LOGE("Failed to write output file '{}': {}", outputFile, errorCodeToMessage(status));
    return status;
  }

  XR_LOGI("Successfully copied VRS file to '{}'", outputFile);
  return 0;
}

