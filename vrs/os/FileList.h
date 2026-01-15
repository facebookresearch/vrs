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

#include <string>
#include <vector>

namespace vrs {
namespace os {

using std::string;
using std::vector;

/// Get all the files in folder, and maybe its folder too.
/// @param path: file system path to list, that must be reachable.
/// @param inOutFiles: on exit, the list of files at that location is appended.
/// @param outFolders: on exit, if provided, set to the list of folders at that location.
/// @return A status code, 0 meaning success.
int getFilesAndFolders(
    const string& path,
    vector<string>& inOutFiles,
    vector<string>* outFolders = nullptr);

/// Get a list of files at a location, possibly recursing into subfolders.
/// @param path: file system path to list, that must be reachable.
/// May point to a single file, or to a folder.
/// If pointing to a folder, the folder's contents are listed, recursively, up to the depth
/// specified.
/// @param inOutFiles: on exit, the files at that location are appended to the list.
/// Attention! The list is not cleared on entry, to allow successive and recursive calls.
/// @param maxRecursiveDepth: max level of recursion, for when path points to a folder.
/// If maxRecursiveDepth is 0, only the folder's immediate content is listed.
/// If maxRecursiveDepth is 1, its immediate subfolders are also listed, etc.
/// Each time a folder is listed, its immediate files are added first, then the content of each
/// folder recursively, always sorted alphabetically.
/// @return A status code, 0 meaning success.
int getFileList(const string& path, vector<string>& inOutFiles, int maxRecursiveDepth = 0);

} // namespace os
} // namespace vrs
