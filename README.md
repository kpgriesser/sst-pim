# PIM SST

This project provides a set of SST components that provide methods to model Processor In Memory (PIM) applications combined with the REV RISCV CPU.

The PIM provides a user extensible finite state machine (FSM) that allows sequencing access to/from a region of DRAM and to/from a local SRAM.

## Supported Platforms

Tested on the following platforms
- MacOS Darwin Kernel Version 23.6.0 arm64
- Redhat 4.18.0-477.10.1.el8_8.x86_64 #1 SMP x86_64
- Ubuntu 5.15.0-122-generic #132-Ubuntu SMP  x86_64

## Getting Started

Checkout and build the 'fastprint' branch of the REV CPU.

```
export PIM_REV_HOME=<path to REV>
git clone git@github.com:kpgriesser/sst-pim.git
cd sst-pim
mkdir build
cd build
cmake ../
make -j -s
make install
ctest
```

## REV Application Examples

```
cd test/pim
make clean
make
cat rev-output/checkdram/run.revlog
cat rev-output/userfunc/run.revlog
ls rev-test-src
```

These tests demonstrate both software and hardware algorithms:
- checkdram.cpp: memcpy (dram to dram) function in hardware.
- userfunc.cpp: scalar-vector multiply function.

The tests can be modified at compile time by modifying these lines of the source code:
```
// Select one and only one
//#define DO_LOOP 1
//#define DO_MEMCPY 1
#define DO_PIM 1
```

The finite state machines (FSMs) are in sstcomp/PIMBackend:
- tclpim_functions.*: built-in functions
- userpim_functions.*: user provided functions

## Appx (Application Driver) Examples

The application driver replaces the REV CPU with application code compiled on that host and loaded as a Miranda subcomponent.
This provides an efficient methods to test the PIM independently of the REV CPU. 

```
cd test/pim
export APPX=1
make clean
make
cat appx-output/AppxTest/run.log
```

