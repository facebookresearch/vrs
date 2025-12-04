# What is in this folder?

The `vrs/os` module provides a number of operating system abstractions. It exists to provide consistent filesystem and OS-level operations across different platforms.

For open source targets, `vrs/os` relies on C++17's `std::filesystem`, which provides robust and standardized filesystem operations.

The `vrs/os` module will continue to evolve to support VRS in challenging environments and emerging operating systems, where platform-specific adaptations may be needed.
