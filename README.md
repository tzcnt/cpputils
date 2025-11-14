![x64-linux-gcc](https://github.com/tzcnt/cpputils/actions/workflows/x64-linux-gcc.yml/badge.svg) ![x64-linux-clang](https://github.com/tzcnt/cpputils/actions/workflows/x64-linux-clang.yml/badge.svg) ![x64-windows-clang-cl](https://github.com/tzcnt/cpputils/actions/workflows/x64-windows-clang-cl.yml/badge.svg) ![arm64-macos-clang](https://github.com/tzcnt/cpputils/actions/workflows/arm64-macos-clang.yml/badge.svg)

![AddressSanitizer](https://github.com/tzcnt/cpputils/actions/workflows/x64-linux-clang-asan.yml/badge.svg) ![ThreadSanitizer](https://github.com/tzcnt/cpputils/actions/workflows/x64-linux-clang-tsan.yml/badge.svg) ![UndefinedBehaviorSanitizer](https://github.com/tzcnt/cpputils/actions/workflows/x64-linux-clang-ubsan.yml/badge.svg) [![codecov](https://codecov.io/gh/tzcnt/cpputils/graph/badge.svg?token=MFUXHHM5U3)](https://codecov.io/gh/tzcnt/cpputils)

# cpputils

High-performance utility classes and data structures.

These utilities are header-only. Each header in `/include` is a standalone utility class with no dependencies on the other headers.

There is a CMake project for the examples and tests, but it is not required to build this project in order to make use of the headers.
