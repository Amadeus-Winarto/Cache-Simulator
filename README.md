# Cache Coherence Simulator

## Introduction

We implemented a cache simulator for analyzing how different snooping-based coherence protocols such as MESI and Dragonfly perform under various workloads. Given any program, we can use our simulator to compare the performance of various protocols, based on number of Bus Transactions, Memory Requests, Memory Write-Backs and Cache-to-Cache Transfers.

## Usage

Usage: Cache Simulator [-h] [--cache_size VAR] [--associativity VAR] [--block_size VAR] protocol input_file

Positional arguments:
  protocol              Cache coherence protocol to use. One of: [MESI, Dragon]
  input_file            Input benchmark name. Must be in the current directory

Optional arguments:
  -h, --help            shows help message and exits
  -v, --version         prints version information and exits
  --cache_size          Cache size (bytes) [default: 4096]
  --associativity       Associativity of the cache [default: 2]
  --block_size          Block size (bytes) [default: 32]

## Protocols

### MESI

The MESI protocol introduces 4 states to a cache line: M(odified), E(xclusive), S(hared) and I(nvalid). There are many different variations of MESI; our implementation tries to follow the protocol the [Illinois Protocol](https://dl.acm.org/doi/pdf/10.1145/800015.808204), which is an *extended* form of MESI as it allows clean-sharing i.e. a cache that has the requested line in the E or S state can respond to the request while inhibiting the main memory from responding.

The MESI protocol implemented in our simulator is thus as follows:

- On Read Miss:
  - A `BusRd` bus transaction is broadcasted to all caches and main memory
  - If any other cache has the line, the main memory is inhibited from responding to the request
  - An implicit priority network is assumed i.e. only one cache can respond to the request
  - All caches that have the line transitions to the S state
  - If the responding cache has the line in the M state, it transitions to the S state *while* writing back to memory
- On Write Miss:
  - A `BusRdX` bus transaction is broadcasted to all caches and main memory
  - Response is handled similarly to Read Miss, except instead of transitioning to S state, the responding caches transition to I state
- On Read Hit:
  - No bus transactions are generated. The cache line remains in the same state.
- On Write Hit:
  - If cache line is in M state, then no writes to main memory is performed. The cache line remains in the M state.
  - If cache line is in E state, then no writes to main memory is performed. The cache line transitions to M state.
  - If cache line is in S state, then an invalidation signal is asserted (no bus traffic is generated). The cache line transitions to M state.
- On eviction, a cache line is written back to memory only if it is in the M state.

### Dragon

TODO

### MOESI

TODO

## Building

```bash
git clone git@github.com:Amadeus-Winarto/Cache-Simulator.git
cd Cache-Simulator

# Build
mkdir build
cd build
cmake ..
make
```

This codebase uses C++20 features, so you will need a compiler that supports it. We have tested this codebase with GCC 11.4.0, GCC 12.3.0, and Clang 15.0.7 on Ubuntu 22.04. There are known issues with MSVC due to its incomplete support for C++20.

This codebase also uses CMake>3.20 as we use the `FetchContent` module to download the [argparse](https://github.com/p-ranav/argparse) library, which is used for parsing command line arguments.
