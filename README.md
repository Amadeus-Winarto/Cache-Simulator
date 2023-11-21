# Cache Coherence Simulator

- [Cache Coherence Simulator](#cache-coherence-simulator)
  - [Introduction](#introduction)
  - [Usage](#usage)
  - [Protocols](#protocols)
    - [MESI](#mesi)
    - [Dragon](#dragon)
    - [MOESI](#moesi)
    - [MESIF](#mesif)
  - [Building](#building)
  - [Simulated Hardware Architecture](#simulated-hardware-architecture)
    - [Default](#default)
    - [Optimisation: Write Buffer](#optimisation-write-buffer)

## Introduction

We implemented a cache simulator for analyzing how different snooping-based coherence protocols such as MESI, MOESI, and Dragon, perform under various workloads. Given any program, we can use our simulator to compare the performance of various protocols, based on number of Bus Transactions, Memory Requests, Memory Write-Backs and Cache-to-Cache Transfers.

We simulate the cycle delays according to the following:

1. Chache to Main Memory --> 100 cycles
2. Cache to cache transfer --> 4*N + (P + 1) cycles, where N is the number of words per cache line and P is the number of Processors.
3. Bus updates --> 2 cycles
4. cache hit--> 0 cycles

The rationale for the additional (P+1) cycles delay for cache to cache transfer was to simulate the overhead for cache to cache communications.

## Usage

```bash
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
```

## Protocols

### MESI

The MESI protocol introduces 4 states to a cache line: M(odified), E(xclusive), S(hared) and I(nvalid). There are many different variations of MESI; our implementation tries to follow the protocol the [Illinois Protocol](https://dl.acm.org/doi/pdf/10.1145/800015.808204), which is an *extended* form of MESI as it allows clean-sharing i.e. a cache that has the requested line in the E or S state can respond to the request while inhibiting the main memory from responding.

The MESI protocol implemented in our simulator is thus as follows:

- On Read Miss:
  - A `BusRd` bus transaction is broadcasted to all caches and main memory
  - If any other cache has the line, the main memory is inhibited from responding to the request
  - A hardware daisy-chain is assumed i.e. only one cache can respond to the request. This enables *clean-sharing* of cache lines.
    - The cost of arbitration using a daisy-chain is assumed to be 1 cycle per cache/main-memory. Hence, in a 4-core setup, the arbitration cost is 5 cycles.
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

The Dragon Protocol has 4 states - Modified(M), Shared-Modified(Sm), Shared-Clean(Sc) and Exclusive(E). Modified and Exclusive states in the Dragon protocol are similar to that of the MESI protocols and its variants. The Dragon Protocol is an update based protocol adapted from the Xerox PARC Dragon processor.

Bus transactions

1. Bus Update
2. Bus Read

Processor transactions

1. Processor read
2. Processor write

The behaviour is as follows:

- On Read Miss:
  - A `BusRd` bus transaction is broadcasted to all caches and main memory
  - If any other cache has the line, the main memory is inhibited from responding to the request
  - If a cache line is shared, the cache line within the processor transitions to Sc and the responding cache line either transitions from M->Sm or E->Sc or it remain as Sc.
  - If a cache line is not shared, the cache line within the processor transitions to Exclusive.
- On Write Miss:
  - A `BusRd` bus transaction is broadcasted to all caches and main memory
  - If the cache line is shared, the cache line within the processor transitions to Sm. A `BusUpd` is then sent to all shared cache.
  - If the cache line is not shared, the cache line within the processor transitions to M.
- On Read Hit:
  - No bus transactions are generated. The cache line remains in the same state.
- On Write Hit:
  - If cache line is in M state, then no writes to main memory is performed. The cache line remains in the M state.
  - If cache line is in E state, then no writes to main memory is performed. The cache line transitions to M state.
  - If cache line is in Sm state, A `BusUpd` is then sent to all shared cache. The cache line remains in Sm.
  - If cache line is in Sc state, A `BusUpd` is then sent to all shared cache. The cache line transitons to Sm.
- On eviction, a cache line is written back to memory if it is in the M or Sm state.

### MOESI

The MOESI protocol we implement extends the Illinois protocol with *dirty-sharing*. An additional state O(wner) is added to the protocol. When a cache has the line in the M state, but the request is a read request, the cache transitions to the O state and responds to the request with the data *without* writing back to memory. Thus, writes to main memory only occur when a cache line is evicted from the M state.

The MOESI protocol implemented in our simulator is thus as follows:

- On Read Miss:
  - A `BusRd` bus transaction is broadcasted to all caches and main memory
  - If any other cache has the line, the main memory is inhibited from responding to the request
  - A hardware daisy-chain is assumed i.e. only one cache can respond to the request. This enables *clean-sharing* of cache lines.
    - The cost of arbitration using a daisy-chain is assumed to be 1 cycle per cache/main-memory. Hence, in a 4-core setup, the arbitration cost is 5 cycles.
  - If the responding cache is in the O state, it sends the data *without* writing back to memory and remains in the O state
  - If the responding cache is in the M state, it sends the data *without* writing back to memory and transitions to the O state
  - If the responding cache is in the E or S state, it sends the data *without* writing back to memory and transitions to the S state
  - For all other cache:
    - If it has the line in the M state, it transitions to the O state
    - If it has the line in the O state, it remains in the O state
    - If it has the line in the E or S state, it transitions to the S state
- On Write Miss:
  - A `BusRdX` bus transaction is broadcasted to all caches and main memory
  - Response is handled similarly to Read Miss, except instead of transitioning to S state, the responding caches transition to I state
- On Read Hit:
  - No bus transactions are generated. The cache line remains in the same state.
- On Write Hit:
  - If cache line is in M state, then no writes to main memory is performed. The cache line remains in the M state.
  - If cache line is in E state, then no writes to main memory is performed. The cache line transitions to M state.
  - If cache line is in S state, then an invalidation signal is asserted (no bus traffic is generated). The cache line transitions to M state.
- On eviction, a cache line is written back to memory only if it is in the M state or the O state.

### MESIF

The MESIF protocol we implement extends the Illinois protocol with *dirty-sharing*. An additional state F(Forward) is added to the protocol. When a cache line transitions is the Invalid state and the processor does a PrRd, it transitions to the Forward state. This allows clean sharing amongst the processors.

The MESIF protocol implemented in our simulator is thus as follows:

- On Read Miss:
  - A `BusRd` bus transaction is broadcasted to all caches and main memory
  - If any other cache has the line, the main memory is inhibited from responding to the request.
  - If the cache line is in the I state, it transitions to the F state if the cache line is shared.
  - If the cache line is in the S, M, E or F state, it remains in its own state.
  - For all other cache:
    - If it has the line in the M state, it transitions to the S state
    - If it has the line in the E, S or F state, it transitions to the S state
- On Write Miss:
  - A `BusRdX` bus transaction is broadcasted to all caches and main memory
  - Response is handled similarly to Read Miss, except instead of transitioning to S state, the responding caches transition to I state
- On Read Hit:
  - No bus transactions are generated. The cache line remains in the same state.
- On Write Hit:
  - If cache line is in M or E state, then no writes to main memory is performed. The cache line remains in the M state.
  - If cache line is in S or F state, then an invalidation signal is asserted (no bus traffic is generated). The cache line transitions to M state.
- On eviction, a cache line is written back to memory only if it is in the M state.

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

## Simulated Hardware Architecture

### Default

We use a system-wide bus to broadcast bus transactions to all caches and main memory. The bus is *atomic* i.e. only one bus transaction can be in flight at any given time. This is a simplification of the actual bus architecture. Writes and reads to main memory takes 100 cycles each. There is no write buffer for writes to main memory. Bus arbitration is done with a FIFO queue.

### Optimisation: Write Buffer

We implement a write buffer as an optional optimisation. With a write buffer, a main-memory "write" only sends the data to the write buffer. The write buffer is then drained in the background.

- If the write buffer is full, subsequent writes to the write buffer are stalled until the write buffer is sufficiently drained.
- If a cache line in the write buffer is requested before the write has completed, the main memory is inhibited from responding to the request. Instead, the write-buffer services the request. The write to main memory for that cache line is cancelled.
- If a cache line in the write buffer is requested after the write has completed, the main memory responds to the request.
- While the write-buffer writes to main-memory, the main-memory is able to respond to read requests.

The number of cycles for cache-to-write-buffer transfer is assumed to be the same as the number of cycles for cache-to-cache transfer to occur.

Note that with this optimisation, the system remains *sequentially consistent*. This is because even though the writes are not yet visible to the main memory, it is visible to all other caches via the write-buffer. Hence, no actual load-store reordering occurs.

To compile with the write buffer optimisation, pass the `USE_WRITE_BUFFER` flag to the compiler:

```bash
cmake .. -DUSE_WRITE_BUFFER=ON
```

The default write buffer size is `-1` i.e. infinite. To change the write buffer size, modify the capacity in `include/write_buffer.hpp`.
