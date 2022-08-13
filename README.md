# sDDF
seL4 Device Driver Framework

The sDDF aims to provide interfaces and protocols for writing and porting device drivers to run as seL4 user level programs. It currently supports a network device running on iMX8 hardware, reaching near wire speed. 
It has been built on top of [seL4 Core Platform](https://github.com/BreakawayConsulting/sel4cp) and requires [this pull request](https://github.com/BreakawayConsulting/sel4cp/pull/11). The seL4 Core Platform binaries can be built separately and handed to the echo_server makefile. 

## Building the sDDF

    $ cd echo_server
    $ make BUILD_DIR=<path/to/build> SEL4CP_SDK=<path/to/core/platform/sdk> SEL4CP_BOARD=imx8mm SEL4CP_CONFIG=(release/debug)
    
## Benchmarking

In order to run the benchmarks, set `SEL4CP_CONFIG=benchmark`. The system has been designed to interact with [ipbench](https://sourceforge.net/projects/ipbench/) to take measurements. 
    
## Supported Boards

### iMX8MM-EVK

