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

## dev-multicore

This branch is configured for a multicore set up. Currently the eth.system file is
set up with the following core allocations:

* Core 0: ethernet driver, transmit multiplexer
* Core 1: receive multiplexer, ARP, copy component, client, timer driver. 
* Core 2: - 
* Core 3: -

Each core has it's own benchmarking PD and idle thread. The benchmarking PD makes 
benchmark utilisation system calls to gather cycle counts for each PD. The kernel stores
this information locally on the core the PD is running on, so the benchmarking PD for that core has to be set up with the PD ids for that particular core. If you change the core 
allocation in the .system file, you should also check the benchmark_.c threads have the correct allocation or you will get garbage data.

This branch also contains the split driver design, which is currently not configured but can
be easily set up by uncommenting the entry in the system file. You also need to check the multiplexer communicates with the correct driver by changing the `#define DRIVER_SEND` to
the appropriate channel id for the eth2 PD. Note that my thesis research did not find this design to be feasible. 
