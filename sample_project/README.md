# What is in this folder?

The code in this folder is a standalone CMake project that uses the VRS library
and headers installed on your system, as opposed to using VRS directly from
source. This example shows how to import and link this library in a project of
your own. The compiled app will create a VRS file with a bunch of arbitrary
records.

To build this project do the following:

1. Follow the
   [installation instructions](https://github.com/facebookresearch/vrs/blob/main/README.md#instructions-macos-and-ubuntu-and-container)
   to build and install the VRS library
2. Copy this directory and its contents to somewhere you want to develop
3. `cd` into your newly copied `sample_project` directory and run
   `mkdir build && cd build`
4. Run: `cmake .. && make`
5. The sample app should successfully build, and you can run it with
   `./sample_recording_app`

For in-depth information and examples on how to properly use the VRS library
please see files in the `sample_app` and `sample_code` directories.
