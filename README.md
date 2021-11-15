# VRS Read-Me

VRS is a file format optimized to record & playback streams of sensor data, such as images, audio samples, and any other discrete sensors (IMU, temperature, etc), stored in per-device streams of time-stamped records. VRS was first created to record images and sensor data from early prototypes of the Quest device, to develop the device’s positional tracking system now known as [Insight](https://ai.facebook.com/blog/powered-by-ai-oculus-insight/).

## Getting Started

VRS is a C++ library with external open source libraries: [boost](https://github.com/boostorg/boost), [cereal](https://github.com/USCiLab/cereal), [fmt](https://github.com/fmtlib/fmt), [lz4](https://github.com/lz4/lz4), [zstd](https://github.com/facebook/zstd), [xxhash](https://github.com/Cyan4973/xxHash), and [googletest](https://github.com/google/googletest) for unit tests. To build & run VRS, you’ll need a C++17 compiler, such as a recent enough version of clang or Visual Studio. The simplest way to build VRS is to use cmake and install the libraries on your system using some package system, such as Brew on MacOS, or apt on Ubuntu. VRS can be built and used on many other platforms, Windows, Android, iOS and other many flavors of Linux, but we currently only provide instructions for MacOS and Ubuntu.

### Install tools & libraries

* MacOS
    * install Brew, following the instruction on [Brew’s web site](https://brew.sh/).
    * install tools & libraries:
        `brew install cmake ninja ccache boost fmt cereal lz4 zstd xxhash googletest`
* Linux
    * install tools & libraries:
        `sudo apt-get install cmake ninja-build ccache libboost-all-dev libfmt-dev
        libcereal-dev liblz4-dev libzstd-dev libxxhash-dev libgtest-dev`

### Build & run

Run cmake:
`cmake -S path_to_vrs_folder -B path_to_build_folder '-GCodeBlocks - Ninja' -DCMAKE_BUILD_TYPE:String=Release`

Build everything & run tests:
`cd path_to_build_folder
ninja all
ctest -j8`

### Windows

We don’t have equivalent instructions for Windows. [vcpkg](https://vcpkg.io/en/index.html) looks like a promising package manager for Windows, but the cmake build system needs more massaging to work. Contributions welcome! :-)

## Samples

* [The sample code](./sample_code) demonstrates the basics to record and read a VRS file, then how to work with `RecordFormat` and `DataLayout` definitions. The code is extensively documented, functional, compiles but isn’t meant to be run.
    * [SampleRecordAndPlay.cpp](./sample_code/SampleRecordAndPlay.cpp)
        Demonstrates different ways to create VRS files, and how to read them, but the format of the records is deliberately trivial, as it is not the focus of this code.
    * [SampleImageReader.cpp](./sample_code/SampleImageReader.cpp)
        Demonstrates how to read records containing images.
    * [SampleRecordFormatDataLayout.cpp](./sample_code/SampleRecordFormatDataLayout.cpp)
        Demonstrates how to use `RecordFormat` and `DataLayout` in more details.
* [The sample apps](./sample_apps) are fully functional apps demonstrate how to create, then read, a VRS file with 3 types of streams.
    * a metadata stream
    * a stream with metadata and uncompressed images
    * a stream with audio images

## License

VRS is released under the [Apache 2.0 license](LICENSE).
