// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
  ErrorDomain jpeg = newErrorDomain("Jpeg");
  ErrorDomain jpeg_2 = newErrorDomain("Jpeg");
  ErrorDomain curl = newErrorDomain("Curl");
  ErrorDomain curl_2 = newErrorDomain("Curl");
  EXPECT_EQ(jpeg, jpeg_2);
  EXPECT_EQ(curl, curl_2);
  EXPECT_STREQ(errorCodeToMessage(errorDomainToErrorCodeStart(jpeg)).c_str(), "Jpeg");
  EXPECT_STREQ(errorCodeToMessage(errorDomainToErrorCodeStart(curl)).c_str(), "Curl");
  int jpegError42 = domainErrorCode(jpeg, 42, "explanation for 42");
  EXPECT_EQ(jpegError42, errorDomainToErrorCodeStart(jpeg) + 1);
  EXPECT_STREQ(errorCodeToMessage(jpegError42).c_str(), "Jpeg error 42: explanation for 42");
}
