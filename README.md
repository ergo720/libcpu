# Libcpu

[![Build Status](https://secure.travis-ci.org/libcpu/libcpu.png?branch=master)](http://travis-ci.org/libcpu/libcpu)

<img src="https://raw.github.com/libcpu/libcpu/master/images/libcpu.png" alt="Libcpu logo" align="right" />

"libcpu" is an open source library that emulates several CPU architectures,
allowing itself to be used as the CPU core for different kinds of emulator
projects. It uses its own frontends for the different CPU types, and uses LLVM
for the backend. libcpu is supposed to be able to do user mode and system
emulation, and dynamic as well as static recompilation.

## Dependencies

CMake version 2.8 or higher is required.
LLVM version 3.3 is required (pre-built binaries will not work and will lead to a build failure).
Python version 2.6 or higher is required (version 3.7.x will not work and will lead to a build failure, other versions 3.x.x are untested).

**Note:** To build successfully LLVM 3.3, you need to apply this fix here https://github.com/llvm-mirror/llvm/commit/9f61e485e6c4a6763695ab399ff61567271836df to the source code.
This is caused by this known bug: https://bugs.llvm.org/show_bug.cgi?id=16625

## Building

**On Ubuntu:**

```
sudo apt-get install flex bison libreadline-dev
```

**On Fedora:**

```
sudo yum install flex bison readline-devel
```

To build libcpu:

```
make
```

**On Windows:**

```
mkdir build && cd build
cmake .. -G "Visual Studio 16 2019" -A Win32
```

Then build the generated solution file with Visual Studio 2019.
Visual Studio 2017 is also known to work.

## Testing

To run the x86 front-end tests:

```
./test/scripts/8086.sh
```

## License

Copyright (c) 2009-2010, the libcpu developers

Libcpu is distributed under the 2-clause BSD license.
