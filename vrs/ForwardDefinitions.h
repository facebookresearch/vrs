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

#pragma once

#include <cstdint>

// Small set of forward declarations to reduce header inclusions

namespace vrs {

class StreamId;
enum class RecordableTypeId : uint16_t;

class RecordFileWriter;
class RecordFileReader;
class MultiRecordFileReader;
class Recordable;
struct StreamTags;
class DataReference;
enum class CompressionType : uint8_t;
enum class CompressionPreset;
enum class ThreadRole;
namespace IndexRecord {
struct RecordInfo;
} // namespace IndexRecord

class RecordFormat;
class DataLayout;
class ContentBlock;
struct CurrentRecord;
enum class ContentType : uint8_t;
enum class ImageFormat : uint8_t;
enum class PixelFormat : uint8_t;
enum class AudioFormat : uint8_t;
enum class AudioSampleFormat : uint8_t;

struct FileSpec;
class FileDelegator;
class FileHandler;
class WriteFileHandler;

class DiskFileChunk;
template <class FileChunk>
class DiskFileT;
using DiskFile = DiskFileT<DiskFileChunk>;

} // namespace vrs
