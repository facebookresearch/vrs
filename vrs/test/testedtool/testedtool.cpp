// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include <cstdlib>

// super trivial app to test return code & output streams
int main(int argc, char** argv) {
  if (argc < 2) {
    return -1;
  }
  return atoi(argv[1]);
}
