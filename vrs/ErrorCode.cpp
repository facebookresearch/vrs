/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <vrs/ErrorCode.h>

#include <map>
#include <mutex>
#include <string>

#define DEFAULT_LOG_CHANNEL "ErrorCode"
#include <logging/Log.h>

#include <vrs/os/Utils.h>

using namespace std;
using namespace vrs;

namespace {
const char* getSimpleVRSErrorName(int errorCode) {
  static map<int, const char*> sRegistry = {
      {SUCCESS, "Success"},
      {FAILURE, "Misc error"},

      {NOT_SUPPORTED, "Given method is not supported on your platform"},
      {NOT_IMPLEMENTED, "Given method is not implemented (yet) on your platform"},
      {VRSERROR_INTERNAL_ERROR, "Error occurred inside VRSError"},

      {NOT_A_VRS_FILE, "Not a VRS file"},
      {NO_FILE_OPEN, "No file open"},
      {FILE_ALREADY_OPEN, "File already open"},
      {FILE_NOT_FOUND, "File not found"},
      {INVALID_PARAMETER, "Invalid parameter"},
      {INVALID_REQUEST, "Invalid request"},
      {INVALID_RANGE, "Invalid range"},
      {INVALID_DISK_DATA, "Read error: invalid data"},
      {INVALID_FILE_SPEC, "Invalid file spec"},
      {INVALID_URI_FORMAT, "Invalid uri format"},
      {INVALID_URI_VALUE, "Invalid character in uri"},
      {READ_ERROR, "Read error: failed to read data"},
      {NOT_ENOUGH_DATA, "Read error: not enough data"},
      {TOO_MUCH_DATA, "Too much data"},
      {UNSUPPORTED_VRS_FILE, "Unsupported VRS file format version"},
      {UNSUPPORTED_DESCRIPTION_FORMAT_VERSION,
       "Read error: unsupported description format version"},
      {UNSUPPORTED_INDEX_FORMAT_VERSION, "Read error: unsupported index format version"},
      {INDEX_RECORD_ERROR, "Index record error"},
      {REINDEXING_ERROR, "Reindexing error"},
      {OPERATION_CANCELLED, "Operation cancelled"},
      {REQUESTED_FILE_HANDLER_UNAVAILABLE, "Requested FileHandler not available"},
      {REQUESTED_DELEGATOR_UNAVAILABLE, "Requested delegator not available"},
      {FILE_HANDLER_MISMATCH, "File handler mismatch"},
      {FILEPATH_PARSE_ERROR, "Could not parse filepath"},
      {MULTICHUNKS_NOT_SUPPORTED, "FileHandler can't handle multiple chunks"},

      {DISKFILE_NOT_OPEN, "DiskFile no file open"},
      {DISKFILE_FILE_NOT_FOUND, "DiskFile file not found"},
      {DISKFILE_INVALID_OFFSET, "DiskFile invalid offset"},
      {DISKFILE_NOT_ENOUGH_DATA, "DiskFile not enough data"},
      {DISKFILE_READ_ONLY, "DiskFile in read-only mode"},
      {DISKFILE_INVALID_STATE, "DiskFile invalid state"},
      {DISKFILE_PARTIAL_WRITE_ERROR, "DiskFile unexpected partial write"},
  };
  auto iter = sRegistry.find(errorCode);
  return iter != sRegistry.end() ? iter->second : nullptr;
}

int newDomainErrorCode(vrs::ErrorDomain errorDomain, int64_t errorCode) {
  static mutex sRangeIndexMapMutex;
  static map<int, map<int64_t, int>> sRangeIndexMap;
  unique_lock<mutex> lock(sRangeIndexMapMutex);
  map<int64_t, int>& indexMap = sRangeIndexMap[errorDomainToErrorCodeStart(errorDomain)];
  int& newErrorCode = indexMap[errorCode]; // create a code of value 0 if it didn't exist
  if (newErrorCode != 0) {
    return newErrorCode; // the error existed already
  }
  if (indexMap.size() >= vrs::kVRSErrorsDomainSize - 1) {
    // Too many errors for that domain
    return FAILURE;
  }
  newErrorCode = errorDomainToErrorCodeStart(errorDomain) + static_cast<int>(indexMap.size());
  return newErrorCode;
}

map<int, string> sDomainErrorRegistry;
mutex sDomainErrorRegistryMutex;

} // namespace

namespace vrs {
string errorCodeToMessage(int errorCode) {
  if (errorCode < 0 || (errorCode > 0 && errorCode < kPlatformUserErrorsStart)) {
    return os::fileErrorToString(errorCode);
  }
  const char* errorName = getSimpleVRSErrorName(errorCode);
  if (errorName != nullptr) {
    return errorName;
  }
  {
    unique_lock<mutex> lock(sDomainErrorRegistryMutex);
    auto iter = sDomainErrorRegistry.find(errorCode);
    if (iter != sDomainErrorRegistry.end()) {
      return iter->second;
    }
  }
  return fmt::format("<Unknown error code '{}'>", errorCode);
}

string errorCodeToMessageWithCode(int errorCode) {
  return errorCodeToMessage(errorCode) + " (#" + to_string(errorCode) + ")";
}

ErrorDomain newErrorDomain(const string& domainName) {
  static mutex sCustomDomainMapMutex;
  static map<string, ErrorDomain> sCustomDomainMap;
  unique_lock<mutex> lock(sCustomDomainMapMutex);
  ErrorDomain& errorDomain = sCustomDomainMap[domainName];
  if (static_cast<int>(errorDomain) == 0) {
    errorDomain = static_cast<ErrorDomain>(
        static_cast<int>(ErrorDomain::CustomDomains) +
        static_cast<int>(sCustomDomainMap.size() - 1));
    unique_lock<mutex> domain_lock(sDomainErrorRegistryMutex);
    sDomainErrorRegistry[errorDomainToErrorCodeStart(errorDomain)] = domainName;
  }
  return errorDomain;
}

int domainErrorCode(ErrorDomain errorDomain, int64_t errorCode, const char* errorMessage) {
  unique_lock<mutex> lock(sDomainErrorRegistryMutex);
  static bool sInternalDomainsRegistered = false;
  if (!sInternalDomainsRegistered) {
    sInternalDomainsRegistered = true;
    sDomainErrorRegistry[errorDomainToErrorCodeStart(ErrorDomain::Lz4DecompressionErrorDomain)] =
        "LZ4 Decompression";
    sDomainErrorRegistry[errorDomainToErrorCodeStart(ErrorDomain::ZstdDecompressionErrorDomain)] =
        "ZSTD Decompression";
    sDomainErrorRegistry[errorDomainToErrorCodeStart(ErrorDomain::ZstdCompressionErrorDomain)] =
        "ZSTD Compression";
    sDomainErrorRegistry[errorDomainToErrorCodeStart(ErrorDomain::FbVrsErrorDomain)] = "fbVRS";
  }
  int newErrorCode = newDomainErrorCode(errorDomain, errorCode);
  // check if there are already too many errors registered for that domain.
  if (newErrorCode == FAILURE) {
    newErrorCode = errorDomainToErrorCodeStart(errorDomain) + kVRSErrorsDomainSize - 1;
    string& lastErrorMessage = sDomainErrorRegistry[newErrorCode];
    if (lastErrorMessage.empty()) {
      lastErrorMessage = sDomainErrorRegistry[errorDomainToErrorCodeStart(errorDomain)] +
          " error: <too many domain errors to track>";
    }
    return newErrorCode;
  }
  // example: "LZ4 decompression error 25: invalid data".
  // Always update the text, in case it changes, to return the last one!
  sDomainErrorRegistry[newErrorCode] =
      sDomainErrorRegistry[errorDomainToErrorCodeStart(errorDomain)] + " error " +
      to_string(errorCode) + ": " + errorMessage;

  return newErrorCode;
}

} // namespace vrs
