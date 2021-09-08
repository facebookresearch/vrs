// Facebook Technologies, LLC Proprietary and Confidential.

#define DEFAULT_LOG_CHANNEL "FileHandlerFactory"
#include <logging/Checks.h>
#include <logging/Log.h>

#include <vrs/DiskFile.h>
#include <vrs/ErrorCode.h>
#include <vrs/FileHandlerFactory.h>

namespace vrs {

FileHandlerFactory& FileHandlerFactory::getInstance() {
  static FileHandlerFactory instance;
  return instance;
}

FileHandlerFactory::FileHandlerFactory() {
  registerFileHandler(std::make_unique<DiskFile>());
}

int FileHandlerFactory::delegateOpen(
    const std::string& path,
    std::unique_ptr<FileHandler>& outNewDelegate) {
  FileSpec fileSpec;
  int status = fileSpec.fromPathJsonUri(path);
  return status == 0 ? delegateOpen(fileSpec, outNewDelegate) : status;
}

int FileHandlerFactory::delegateOpen(
    const FileSpec& fileSpec,
    std::unique_ptr<FileHandler>& outNewDelegate) {
  if (!fileSpec.fileHandlerName.empty() &&
      (!outNewDelegate || outNewDelegate->getFileHandlerName() != fileSpec.fileHandlerName)) {
    std::unique_ptr<FileHandler> newHandler = getFileHandler(fileSpec.fileHandlerName);
    if (!newHandler) {
      XR_LOGW(
          "No FileHandler '{}' available to open '{}'",
          fileSpec.fileHandlerName,
          fileSpec.toJson());
      outNewDelegate.reset();
      return REQUESTED_FILE_HANDLER_UNAVAILABLE;
    }
    outNewDelegate = move(newHandler);
  }
  // default to a disk file
  if (!outNewDelegate) {
    outNewDelegate = std::make_unique<DiskFile>();
  }
  // Now delegate opening the file to the file handler, which might delegate further...
  std::unique_ptr<FileHandler> newDelegate;
  int status = outNewDelegate->delegateOpenSpec(fileSpec, newDelegate);
  if (newDelegate) {
    outNewDelegate.swap(newDelegate);
  }
  return status;
}

void FileHandlerFactory::registerFileHandler(std::unique_ptr<FileHandler>&& fileHandler) {
  XR_DEV_CHECK_FALSE(fileHandler->getFileHandlerName().empty());
  fileHandlerMap_[fileHandler->getFileHandlerName()] = std::move(fileHandler);
}

void FileHandlerFactory::unregisterFileHandler(const std::string& fileHandlerName) {
  fileHandlerMap_.erase(fileHandlerName);
}

std::unique_ptr<FileHandler> FileHandlerFactory::getFileHandler(const std::string& name) {
  XR_DEV_CHECK_FALSE(name.empty());
  auto handler = fileHandlerMap_.find(name);
  if (handler != fileHandlerMap_.end()) {
    return handler->second->makeNew();
  }
  return {};
}

} // namespace vrs
