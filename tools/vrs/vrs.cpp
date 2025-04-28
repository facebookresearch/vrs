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

#include "VrsCommand.h"

#ifdef INCLUDE_XPRS
#include <vrs/utils/DecoderFactory.h>
#include <vrs/utils/xprs_decoder/XprsDecoder.h>
#endif

#include <vrs/os/Utils.h>

using namespace std;
using namespace vrscli;

int main(int argc, char** argv) {
// Initialize with XPRS if included
#ifdef INCLUDE_XPRS
  vrs::utils::DecoderFactory::get().registerDecoderMaker(vrs::vxprs::xprsDecoderMaker);
#endif

  const string& appName = vrs::os::getFilename(argv[0]);
  if (argc == 1) {
    printHelp(appName);
    printSamples(appName);
    return EXIT_FAILURE;
  }
  VrsCommand vrsCommand;
  if (!vrsCommand.parseCommand(appName, argv[1])) {
    printHelp(appName);
    printSamples(appName);
    return EXIT_FAILURE;
  }
  int statusCode = EXIT_SUCCESS;
  int argn = 1;
  while (++argn < argc && statusCode == EXIT_SUCCESS) {
    string arg = argv[argn];
    if (!vrsCommand.parseArgument(appName, argn, argc, argv, statusCode) &&
        !vrsCommand.processUnrecognizedArgument(appName, arg)) {
      statusCode = EXIT_FAILURE;
    }
  }
  if (argc == 1 || vrsCommand.showHelp) {
    printHelp(appName);
    printSamples(appName);
  } else if (statusCode == EXIT_SUCCESS) {
    statusCode = vrsCommand.openVrsFile() ? vrsCommand.runCommands() : EXIT_FAILURE;
  }

  return statusCode;
}
