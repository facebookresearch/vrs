# What is in this folder?

The modules in `vrs/oss` are replacements for libraries that exist at Meta but we aren't open sourcing at this point.

- `vrs/oss/logging` provides a logging facility, that merely prints to `stdout`. When integrating VRS in your own code, you will probably want to redirect that logging output to your own logging infrastructure.

The other modules in `vrs/oss` may look a bizarre, because they are minimized clones of the original, sometimes with a single definition. They are only used for testing.

- `vrs/oss/portability` provides a single definition used for some tests only, and that weren't promoted into `vrs/os`.
- `vrs/oss/test_data` provides some sample files used for unit tests purposes only.
- `vrs/oss/test_helpers` and `vrs/oss/TestDataDir` provides helper functionality for unit tests.
