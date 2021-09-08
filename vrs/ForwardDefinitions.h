// Facebook Technologies, LLC Proprietary and Confidential.

#pragma once

#include <cstdint>

/// Small set of forward declarations to reduce header inclusions

namespace vrs {

class StreamId;
using RecordableId [[deprecated("Please use vrs::StreamId instead of vrs::RecordableId.")]] =
    vrs::StreamId;
enum class RecordableTypeId : uint16_t;

class RecordFileWriter;
class RecordFileReader;
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
enum class ContentType : uint8_t;
enum class ImageFormat : uint8_t;
enum class PixelFormat : uint8_t;
enum class AudioFormat : uint8_t;
enum class AudioSampleFormat : uint8_t;

struct FileSpec;
class FileHandler;
class DiskFile;

} // namespace vrs
