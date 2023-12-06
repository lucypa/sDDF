# sDDF
seL4 Device Driver Framework

The sDDF aims to provide interfaces and protocols for writing and
porting device drivers to run as seL4 user level programs. It
currently supports a network device running on iMX8 hardware, reaching
near wire speed.  It has been built on top of [seL4 Core
Platform](https://github.com/BreakawayConsulting/sel4cp) and requires
[this pull
request](https://github.com/BreakawayConsulting/sel4cp/pull/11). The
seL4 Core Platform binaries can be built separately and handed to the
echo_server makefile.

## Building the sDDF

Note that while any ARM GCC toolchain should work, all testing and
benchmarking so far has been done with the ARM GCC toolchain version 10.2-2020.11.

If you wish to use the default toolchain you can download it from here:
https://developer.arm.com/-/media/Files/downloads/gnu-a/10.2-2020.11/binrel/gcc-arm-10.2-2020.11-x86_64-aarch64-none-elf.tar.xz?revision=79f65c42-1a1b-43f2-acb7-a795c8427085&hash=61BBFB526E785D234C5D8718D9BA8E61.

Otherwise, you can change the Makefile to accept another toolchain or pass the prefix
to the Makefile using the argument `TOOLCHAIN=<PREFIX>`.

```
    $ cd echo_server
    $ make BUILD_DIR=<path/to/build> \
        SEL4CP_SDK=<path/to/core/platform/sdk> \
        SEL4CP_BOARD=imx8mm_evk SEL4CP_CONFIG=(benchmark/release/debug)
```

## Benchmarking

In order to run the benchmarks, set `SEL4CP_CONFIG=benchmark`. The
system has been designed to interact with
[ipbench](https://sourceforge.net/projects/ipbench/) to take
measurements.

Checks to make before benchmarking:
* Turn off all debug prints.
* Turn off all sDDF related asserts (pass `NO_ASSERT` in Makefile).
* Run with LWIP asserts turned off as well (`LWIP_NOASSERT`).
* Make sure compiler optimisations are enabled.

## Supported Boards

### iMX8MM-EVK

## Tx Copy

This branch is configured for a single client, client0 and the ARP component. The client has an additional copy
component on the TX path as well to reduce the trust placed on the client. 
Note that this system was only tested with one client, but uses the same code for a multiple client set up, hence there are two small hacks inside the multiplexers to avoid the unused second client. 
