# What is in this folder?

The `vrs/os` module provides a number of operating system abstractions. It
exists in part because `std::filesystem` is in its infancy. While
`std::filesystem` has made great progress recently, it still has teething
issues, and worse, it is still not available on every system we need VRS to
support.

For open source targets, `vrs/os` mostly relies on boost, which provides the
most robust and complete implementation.

Even after a robust version of `std::filesystem` is broadly available on common
platforms and we can stop depending on boost, we will continue to rely on
`vrs/os` to help build support for VRS in more challenging environments/emerging
operating systems, where support will be slow to come, if it ever does, even for
boost.
