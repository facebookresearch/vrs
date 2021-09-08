// Facebook Technologies, LLC Proprietary and Confidential.

#include "CopyRecords.h"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>

#define DEFAULT_LOG_CHANNEL "CopyRecords"
#include <logging/Log.h>

#include <vrs/os/Time.h>
#include <vrs/os/Utils.h>

#include "CopyHelpers.h"

using namespace std;
using namespace vrs;

namespace vrs::utils {

void copyMergeDoc() {
  cout << "VRStool --copy combined.vrs <source.vrs>+" << endl;
  cout << "VRStool --merge combined.vrs <source.vrs>+" << endl;
  cout << endl;
  cout << "Combine multiple VRS files into a single VRS file" << endl;
  cout << endl;
  cout << "File tags will be merged. If a value is declared in multiple file, the value in the"
       << endl;
  cout << "earlier file will be kept." << endl;
  cout << endl;
  cout << "The 'copy' option will add the streams side-by-side, even if their RecordableTypeId"
       << endl;
  cout << "is identical." << endl;
  cout << endl;
  cout << "The 'merge' option will merge streams with the same RecordableTypeId," << endl;
  cout << "in their respective order in each source file. So for each RecordableTypeId:" << endl;
  cout << " - the first streams of type RecordableTypeId of each file will be merged together,"
       << endl;
  cout << " - the second streams of type RecordableTypeId of each file will be merged together,"
       << endl;
  cout << " - etc." << endl;
  cout << "Stream tags will also be merged, using the same logic as for file tags." << endl;
  cout << endl;
  cout << "If the files don't have streams with matching RecordableTypeId, both copy and merge"
       << endl;
  cout << "operations produce the same output." << endl;
  cout << endl;
  cout << "Important: it's the RecordableTypeId that's matched, not the StreamId." << endl;
  cout
      << "So if you stream-merge two files, each with a single stream of the same RecordableTypeId,"
      << endl;
  cout << "even if the StreamId instance id don't match, the streams will be merged together."
       << endl;
}

std::unique_ptr<StreamPlayer> makeCopier(
    RecordFileReader& fileReader,
    RecordFileWriter& fileWriter,
    StreamId streamId,
    const CopyOptions& copyOptions) {
  return make_unique<Copier>(fileReader, fileWriter, streamId, copyOptions);
}

int copyRecords(
    FilteredVRSFileReader& filteredReader,
    const string& pathToCopy,
    const CopyOptions& copyOptions,
    std::unique_ptr<vrs::UploadMetadata>&& uploadMetadata,
    MakeStreamFilterFunction makeStreamFilter) {
  // make sure we don't end-up with a stale gaia lookup cache record
  if (uploadMetadata && uploadMetadata->getUpdateId() != 0) {
    if (uploadMetadata->getUpdateId() == filteredReader.getGaiaId()) {
      filteredReader.clearGaiaSourceCachedLookup();
    } else {
      GaiaClient::makeInstance()->clearCachedLookup(uploadMetadata->getUpdateId());
    }
  }
  ThrottledWriter throttledWriter(copyOptions);
  RecordFileWriter& writer = throttledWriter.getWriter();
  writer.addTags(filteredReader.reader.getTags());
  vector<unique_ptr<StreamPlayer>> filters;
  filters.reserve(filteredReader.filter.streams.size());
  size_t maxRecordCount = 0;
  for (auto id : filteredReader.filter.streams) {
    filters.emplace_back(makeStreamFilter(filteredReader.reader, writer, id, copyOptions));
    maxRecordCount += filteredReader.reader.getIndex(id).size();
  }
  double startTimestamp, endTimestamp;
  filteredReader.getConstrainedTimeRange(startTimestamp, endTimestamp);
  filteredReader.tagOverrides.overrideTags(writer);
  if (!uploadMetadata) {
    writer.preallocateIndex(filteredReader.buildIndex());
  }
  ThrottledFileHelper fileHelper(throttledWriter);
  int copyResult = fileHelper.createFile(pathToCopy, uploadMetadata);
  if (copyResult == 0) {
    // Init tracker propgress early, to be sure we track the background thread queue size
    filteredReader.preRollConfigAndState(); // make sure to copy most recent config & state records
    throttledWriter.initTimeRange(startTimestamp, endTimestamp);
    filteredReader.iterate(&throttledWriter);
    for (auto& filter : filters) {
      filter->flush();
    }
    copyResult = fileHelper.closeFile();
    if (writer.getBackgroundThreadQueueByteSize() != 0) {
      XR_LOGE("Unexpected count of bytes left in queue after copy!");
    }
  }
  copyOptions.outGaiaId = fileHelper.getGaiaId();
  return copyResult;
}

// given a list of existing tags and a list of new tags, create a list of tags to insert
static void mergeTags(
    const map<string, string>& writtenTags,
    const map<string, string>& newTags,
    map<string, string>& outTags,
    const string& source,
    bool isVrsPrivate) {
  for (const auto& newTag : newTags) {
    auto writtenTag = writtenTags.find(newTag.first);
    // add tags, but don't overwrite an existing value (warn instead)
    if (writtenTag != writtenTags.end()) {
      if (newTag.second != writtenTag->second) {
        // if that tag is already set to a different value
        if (isVrsPrivate) {
          // don't merge private VRS tags, but warn...
          cerr << "Warning! The tag '" << newTag.first << "' was already set, ignoring value '"
               << newTag.second << "' from " << source << endl;
        } else {
          // store value using a new name, to perserve (some) context
          cerr << "Warning! The tag '" << newTag.first
               << "' was already set. Dup found in: " << source << endl;
          string newName = newTag.first + "_merged";
          int count = 1;
          // find a name that's not in use anywhere.
          // Because of collisions, we even need to check newTags & outTags...
          while (writtenTags.find(newName) != writtenTags.end() ||
                 newTags.find(newName) != newTags.end() || outTags.find(newName) != outTags.end()) {
            newName = newTag.first + "_merged-" + to_string(count++);
          }
          outTags[newName] = newTag.second;
        }
      } else {
        // the value is identical: do nothing.
      }
    } else {
      outTags[newTag.first] = newTag.second;
    }
  }
}

namespace {
class NoDuplicateCopier : public Copier {
 public:
  NoDuplicateCopier(
      vrs::RecordFileReader& fileReader,
      vrs::RecordFileWriter& fileWriter,
      vrs::StreamId id,
      const CopyOptions& copyOptions)
      : Copier(fileReader, fileWriter, id, copyOptions) {
    lastRecordTimestamps_.fill(nan(""));
  }

  bool processRecordHeader(const CurrentRecord& record, vrs::DataReference& outDataRef) override {
    double& lastTimestamp = lastRecordTimestamps_[static_cast<size_t>(record.recordType)];
    if (lastTimestamp == record.timestamp) {
      return false;
    }
    lastTimestamp = record.timestamp;
    return Copier::processRecordHeader(record, outDataRef);
  }

 protected:
  array<double, static_cast<size_t>(vrs::Record::Type::COUNT)> lastRecordTimestamps_;
};
} // namespace

// Combine multiple VRS files into a single VRS file
int mergeRecords(
    FilteredVRSFileReader& firstRecordFilter,
    list<FilteredVRSFileReader>& moreRecordFilters,
    const string& pathToCopy,
    const CopyOptions& copyOptions,
    std::unique_ptr<vrs::UploadMetadata>&& uploadMetadata) {
  // setup the record file writer and hook-up the readers to record copiers, and copy/merge tags
  ThrottledWriter throttledWriter(copyOptions);
  RecordFileWriter& recordFileWriter = throttledWriter.getWriter();
  list<NoDuplicateCopier> copiers; // to delete copiers on exit
  // track copiers by recordable type id in sequence/instance order in the output file
  map<RecordableTypeId, vector<NoDuplicateCopier*>> copiersMap;
  // copy the tags & create the copiers for the first source file
  recordFileWriter.addTags(firstRecordFilter.reader.getTags());
  for (auto id : firstRecordFilter.filter.streams) {
    copiers.emplace_back(firstRecordFilter.reader, recordFileWriter, id, copyOptions);
    copiersMap[id.getTypeId()].push_back(&copiers.back());
  }
  // calculate the overall timerange, so we can resolve time constraints on the overall file
  double startTimestamp, endTimestamp;
  firstRecordFilter.getTimeRange(startTimestamp, endTimestamp);
  for (auto& filter : moreRecordFilters) {
    filter.expandTimeRange(startTimestamp, endTimestamp);
    // add the global tags
    map<string, string> tags;
    mergeTags(recordFileWriter.getTags(), filter.reader.getTags(), tags, filter.path, false);
    recordFileWriter.addTags(tags);

    // track how many stream of each type we've seen in the current reader
    map<RecordableTypeId, size_t> recordableIndex;
    // For each stream, see if we merge it into an existing stream, or create a new one
    for (auto id : filter.filter.streams) {
      if (copyOptions.mergeStreams) {
        size_t index = recordableIndex[id.getTypeId()]++; // get index & increment for next one
        vector<NoDuplicateCopier*>& existingCopiers = copiersMap[id.getTypeId()];
        if (index < existingCopiers.size()) {
          // merge this stream: re-use the existing copier
          filter.reader.setStreamPlayer(id, existingCopiers[index]);
          Writer& writer = existingCopiers[index]->getWriter();
          // merge new user & vrs tags into the existing stream tags
          const StreamTags& writtenTags = writer.getRecordableTags();
          const StreamTags& newTags = filter.reader.getTags(id);
          StreamTags streamTags;
          string tagSource = id.getName() + " of " + filter.path;
          mergeTags(writtenTags.user, newTags.user, streamTags.user, tagSource, false);
          mergeTags(writtenTags.vrs, newTags.vrs, streamTags.vrs, tagSource, true);
          writer.addTags(streamTags);
        } else {
          copiers.emplace_back(filter.reader, recordFileWriter, id, copyOptions);
          copiersMap[id.getTypeId()].push_back(&copiers.back());
        }
      } else {
        copiers.emplace_back(filter.reader, recordFileWriter, id, copyOptions);
      }
    }
  }

  firstRecordFilter.tagOverrides.overrideTags(throttledWriter.getWriter());

  // create a time-sorted list of all the records (pre-flight only: no actual read)
  deque<SourceRecord> records;
  auto recordCollector = [&records](
                             RecordFileReader& reader, const IndexRecord::RecordInfo& record) {
    records.push_back(SourceRecord{&reader, &record});
    return true;
  };
  firstRecordFilter.filter.resolveTimeConstraints(startTimestamp, endTimestamp);
  firstRecordFilter.preRollConfigAndState(recordCollector);
  firstRecordFilter.iterate(recordCollector);
  for (auto& filter : moreRecordFilters) {
    filter.filter.resolveTimeConstraints(startTimestamp, endTimestamp);
    filter.preRollConfigAndState(recordCollector);
    filter.iterate(recordCollector);
  }
  sort(records.begin(), records.end());

  // Build preliminary index
  auto preliminaryIndex = make_unique<deque<IndexRecord::DiskRecordInfo>>();
  deque<IndexRecord::DiskRecordInfo>& index = *preliminaryIndex;
  int64_t offset = 0;
  for (auto& r : records) {
    index.push_back(
        {r.record->timestamp,
         static_cast<uint32_t>(r.record->fileOffset - offset),
         r.record->streamId,
         r.record->recordType});
    offset = r.record->fileOffset;
  }
  if (!uploadMetadata) {
    recordFileWriter.preallocateIndex(move(preliminaryIndex));
  }

  ThrottledFileHelper fileHelper(throttledWriter);
  int mergeResult = fileHelper.createFile(pathToCopy, uploadMetadata);
  if (mergeResult == 0) {
    // read all the records in order
    if (!records.empty()) {
      throttledWriter.initTimeRange(
          records.front().record->timestamp, records.back().record->timestamp);
      for (auto& recordSource : records) {
        recordSource.reader->readRecord(*recordSource.record);
        throttledWriter.onRecordDecoded(recordSource.record->timestamp);
      }
    }
    mergeResult = fileHelper.closeFile();
  }
  copyOptions.outGaiaId = fileHelper.getGaiaId();
  return mergeResult;
}

int downloadRecords(
    FilteredVRSFileReader& downloadFilteredReader,
    const string& downloadFolder,
    const CopyOptions& copyOptions) {
  const string uri = downloadFilteredReader.getPathOrUri();
  string downloadFileName = downloadFilteredReader.getFileName();
  if (downloadFileName.empty()) {
    if (copyOptions.jsonOutput) {
      printJsonResult(FAILURE, "Can't get filename for " + uri);
    } else {
      cerr << "Failed to obtain original filename for " << uri << endl;
    }
    return FAILURE;
  }
  const string copyPath = os::pathJoin(downloadFolder, downloadFileName);
  double startTime = os::getTimestampSec();
  int status = copyRecords(downloadFilteredReader, copyPath, copyOptions);
  if (copyOptions.jsonOutput) {
    printJsonResult(
        status,
        errorCodeToMessage(status),
        {{kLocalPathResult, copyPath}},
        downloadFilteredReader.getGaiaId());
  } else if (status != 0) {
    cerr << "Failed to download " << uri << "..." << endl;
  } else {
    double duration = os::getTimestampSec() - startTime;
    duration = static_cast<double>(static_cast<int64_t>(duration * 100)) / 100;
    cout << "Successfully downloaded " << uri << " to " << copyPath << " in " << fixed
         << setprecision(1) << duration << "s." << endl;
  }
  return status;
}

int updateRecords(
    GaiaId updateId,
    FilteredVRSFileReader& updateFilteredReader,
    const CopyOptions& copyOptions) {
  cout << "Uploading new optimized VRS file version to Manifold..." << endl;
  unique_ptr<UploadMetadata> uploadMetadata = make_unique<UploadMetadata>();
  uploadMetadata->setUploadType(UploadType::Update);
  uploadMetadata->setUpdateId(updateId);
  string tempPath = os::getTempFolder() + to_string(updateId) + ".vrs";
  int statusCode = copyRecords(updateFilteredReader, tempPath, copyOptions, move(uploadMetadata));
  if (copyOptions.jsonOutput) {
    printJsonResult(statusCode, errorCodeToMessage(statusCode), {}, updateId);
  } else if (statusCode != 0) {
    cerr << "Failed to update " << updateId << ": " << errorCodeToMessage(statusCode) << endl;
  } else {
    cout << "Update of gaia:" << updateId << " complete." << endl;
  }
  return statusCode;
}

} // namespace vrs::utils
