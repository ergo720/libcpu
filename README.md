# Libcpu

[![CircleCI](https://circleci.com/gh/libcpu/libcpu.svg?style=svg)](https://circleci.com/gh/libcpu/libcpu)

<img src="https://raw.github.com/libcpu/libcpu/master/images/libcpu.png" alt="Libcpu logo" align="right" />

"libcpu" is an open source library that emulates several CPU architectures,
allowing itself to be used as the CPU core for different kinds of emulator
projects. It uses its own frontends for the different CPU types, and uses LLVM
for the backend. libcpu is supposed to be able to do user mode and system
emulation, and dynamic as well as static recompilation.

## Dependencies

CMake version 2.8 or higher is required.  
LLVM version 8.0.1 is required.  
Python version 2.6 or higher is required (version 3.7.x will not work and will lead to a build failure, other versions 3.x.x are untested).

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
