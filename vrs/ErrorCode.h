// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <map>
#include <string>

#include <fmt/format.h>

#include <vrs/os/Platform.h>

namespace vrs {

#if IS_MAC_PLATFORM() || IS_IOS_PLATFORM()
// http://www.cs.cmu.edu/afs/cs/user/lenzo/html/mac_errors.html
// Largest error number is 100102 kPOSIXErrorEOPNOTSUPP
const int kPlatformUserErrorsStart = 200000;
#elif IS_WINDOWS_PLATFORM()
const int kPlatformUserErrorsStart = 1 << 29; // bit 29 is set for user errors
#elif IS_LINUX_PLATFORM() || IS_ANDROID_PLATFORM()
const int kPlatformUserErrorsStart = 1000; // Errorno is 131
#elif IS_XROS_PLATFORM()
const int kPlatformUserErrorsStart = 1000; // Use same as linux
#else
#error "unknown platform"
#endif

const int kSimpleVRSErrorsSize = 1000;
const int kVRSErrorsDomainSize = 100;
const int kDomainErrorsStart = kPlatformUserErrorsStart + kSimpleVRSErrorsSize;

/// Enum for regular VRS errors.
enum ErrorCode : int {
  SUCCESS = 0,

  FAILURE = kPlatformUserErrorsStart,
  NOT_SUPPORTED,
  NOT_IMPLEMENTED,
  VRSERROR_INTERNAL_ERROR,

  NOT_A_VRS_FILE,
  NO_FILE_OPEN,
  FILE_ALREADY_OPEN,
  FILE_NOT_FOUND,
  INVALID_PARAMETER,
  INVALID_REQUEST,
  INVALID_RANGE,
  INVALID_DISK_DATA,
  INVALID_FILE_SPEC,
  INVALID_URI_FORMAT,
  INVALID_URI_VALUE,
  READ_ERROR,
  NOT_ENOUGH_DATA,
  TOO_MUCH_DATA,
  UNSUPPORTED_VRS_FILE,
  UNSUPPORTED_DESCRIPTION_FORMAT_VERSION,
  UNSUPPORTED_INDEX_FORMAT_VERSION,
  INDEX_RECORD_ERROR,
  REINDEXING_ERROR,
  OPERATION_CANCELLED,
  REQUESTED_FILE_HANDLER_UNAVAILABLE,
  FILE_HANDLER_MISMATCH,
  FILEPATH_PARSE_ERROR,
  MULTICHUNKS_NOT_SUPPORTED,

  DISKFILE_NOT_OPEN,
  DISKFILE_FILE_NOT_FOUND,
  DISKFILE_INVALID_OFFSET,
  DISKFILE_NOT_ENOUGH_DATA,
  DISKFILE_READ_ONLY,
  DISKFILE_INVALID_STATE,
  DISKFILE_PARTIAL_WRITE_ERROR,
};

/// Errors can come from VRS, or a helper library like LZ4, ZSTD, or a file handler's sub-system.
/// There is no telling if these error codes will collide with the OS', VRS' or each other.
/// Error domains create a safe mechanism to report any of these errors as an int,
/// which can then be converted back to a human readable string using vrs::errorCodeToMessage(code).
/// The caveat is that the numeric values themselves may vary from run-to-run.
/// Error domains can be created dynamically, with the limitation that 99 distinct custom errors per
/// domain can be tracked during a single run. In practice, 99 distinct errors per domain should be
/// plenty, and it's safe to generate more errors, but the error messages beyond 99 will not be
/// tracked individually.
enum class ErrorDomain : int {
  Lz4DecompressionErrorDomain,
  ZstdCompressionErrorDomain,
  ZstdDecompressionErrorDomain,
  FbVrsErrorDomain, // TODO(yinoue): convert fbcode to use a custom domain, then delete this.

  // keep last, as we will add to this enum at runtime using newErrorDomain()
  CustomDomains
};

/// Conversion of a error domain to an int. For internal & test purposes only.
constexpr int errorDomainToErrorCodeStart(ErrorDomain errorDomain) {
  return kDomainErrorsStart + static_cast<int>(errorDomain) * kVRSErrorsDomainSize;
}

/// Convert an int error code into a human readable string for logging.
/// This API should work with any int error code returned by any VRS API.
/// @param errorCode: an error code returned by any VRS API.
/// @return A string that describes the error.
std::string errorCodeToMessage(int errorCode);

/// Convert an int error code into a human readable string for logging.
/// This API should work with any int error code returned by any VRS API.
/// This version includes the error code's numeric value.
/// @param errorCode: an error code returned by any VRS API.
/// @return A string that describes the error.
std::string errorCodeToMessageWithCode(int errorCode);

/// Create a new error domain, based on a name that's supposed to be unique, such as "CURL"
/// @param domainName: some unique domain name.
/// @return A new error domain enum value.
ErrorDomain newErrorDomain(const std::string& domainName);

/// Create an int error code for a specific error domain and error code within that domain.
/// @param errorDomain: the error domain of the error code.
/// @param errorCode: an error code within that domain, which can be any value.
/// @param errorMessage: an error description for the errorCode.
/// @return A int that can be safely returned by VRS to represent the domain error.
/// The errorMessage is saved, so that future calls to errorCodeToMessage() will return that
/// error message for that int error code.
int domainErrorCode(ErrorDomain errorDomain, int64_t errorCode, const char* errorMessage);

/// Helper function so that any integer error type (including enums) can be used for domain errors,
/// as this template helper will simply cast that error code to an int64.
template <class T>
int domainErrorCode(ErrorDomain errorDomain, T errorCode, const char* errorMessage) {
  return domainErrorCode(errorDomain, static_cast<int64_t>(errorCode), errorMessage);
}

/// Helper class to define your own error domain.
/// - create an enum class for your errors, that you pass to your template.
/// - provide a map enum -> to text, to explain each enum.
/// You can then call newErrorCode() to get an int error code that you can return.

template <class EC>
const std::map<EC, const char*>& getErrorCodeRegistry();

template <class EC>
ErrorDomain getErrorDomain();

template <class EC>
int domainError(EC errorCode) {
  const std::map<EC, const char*>& registry = getErrorCodeRegistry<EC>();
  auto iter = registry.find(errorCode);
  if (iter != registry.end()) {
    return domainErrorCode(getErrorDomain<EC>(), errorCode, iter->second);
  }
  std::string msg = fmt::format("<Unknown error code '{}'>", static_cast<int>(errorCode));
  return domainErrorCode(getErrorDomain<EC>(), errorCode, msg.c_str());
}

} // namespace vrs
