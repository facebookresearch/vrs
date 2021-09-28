// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include <gtest/gtest.h>

#include <vrs/os/Platform.h>

#include <vrs/ErrorCode.h>

using namespace vrs;

namespace {

struct ErrorCodeTest : testing::Test {};

} // namespace

TEST_F(ErrorCodeTest, testErrorCode) {
  EXPECT_EQ(ErrorCode::SUCCESS, 0);

#if IS_MAC_PLATFORM() || IS_IOS_PLATFORM()
  EXPECT_EQ(ErrorCode::FAILURE, 200000);
#elif IS_WINDOWS_PLATFORM()
  EXPECT_EQ(ErrorCode::FAILURE, 1 << 29);
#elif IS_LINUX_PLATFORM() || IS_ANDROID_PLATFORM()
  EXPECT_EQ(ErrorCode::FAILURE, 1000);
#endif

  const char* errorMessage = "test error message";

  int errorCode = domainErrorCode(ErrorDomain::Lz4DecompressionErrorDomain, 42, errorMessage);
  EXPECT_EQ(errorCode, errorDomainToErrorCodeStart(ErrorDomain::Lz4DecompressionErrorDomain) + 1);
  EXPECT_EQ(errorCodeToMessage(errorCode), "LZ4 Decompression error 42: test error message");

  errorCode = domainErrorCode(ErrorDomain::ZstdDecompressionErrorDomain, 42, errorMessage);
  EXPECT_EQ(errorCode, errorDomainToErrorCodeStart(ErrorDomain::ZstdDecompressionErrorDomain) + 1);
  EXPECT_EQ(errorCodeToMessage(errorCode), "ZSTD Decompression error 42: test error message");

  // Register domain errors to range limit.
  int domainError = 1000;
  for (int i = 2; i < kVRSErrorsDomainSize; i++) {
    domainError++;
    errorCode =
        domainErrorCode(ErrorDomain::Lz4DecompressionErrorDomain, domainError, errorMessage);
    EXPECT_EQ(errorCode, errorDomainToErrorCodeStart(ErrorDomain::Lz4DecompressionErrorDomain) + i);
  }

  // Try register one more
  errorCode =
      domainErrorCode(ErrorDomain::Lz4DecompressionErrorDomain, ++domainError, errorMessage);
  const int lastDomainError =
      errorDomainToErrorCodeStart(ErrorDomain::Lz4DecompressionErrorDomain) + kVRSErrorsDomainSize -
      1;
  const char* kTooManyLz4CompressionErrorMessage =
      "LZ4 Decompression error: <too many domain errors to track>";
  EXPECT_EQ(errorCode, lastDomainError);
  EXPECT_EQ(errorCodeToMessage(errorCode), kTooManyLz4CompressionErrorMessage);
  // Try to add yet another error to the domain, see it doesn't create a new error code
  int nextErrorCode =
      domainErrorCode(ErrorDomain::Lz4DecompressionErrorDomain, domainError + 1, errorMessage);
  EXPECT_EQ(nextErrorCode, errorCode);
  EXPECT_EQ(errorCodeToMessage(nextErrorCode), kTooManyLz4CompressionErrorMessage);
}

TEST_F(ErrorCodeTest, newDomainTest) {
  ErrorDomain gaia = newErrorDomain("Gaia");
  ErrorDomain gaia_2 = newErrorDomain("Gaia");
  ErrorDomain curl = newErrorDomain("Curl");
  ErrorDomain curl_2 = newErrorDomain("Curl");
  EXPECT_EQ(gaia, gaia_2);
  EXPECT_EQ(curl, curl_2);
  EXPECT_STREQ(errorCodeToMessage(errorDomainToErrorCodeStart(gaia)).c_str(), "Gaia");
  EXPECT_STREQ(errorCodeToMessage(errorDomainToErrorCodeStart(curl)).c_str(), "Curl");
  int gaiaError42 = domainErrorCode(gaia, 42, "explanation for 42");
  EXPECT_EQ(gaiaError42, errorDomainToErrorCodeStart(gaia) + 1);
  EXPECT_STREQ(errorCodeToMessage(gaiaError42).c_str(), "Gaia error 42: explanation for 42");
}
