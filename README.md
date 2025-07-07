# SHMemora: Protective Key-Value Store on Distributed Shared Memory

**SHMemora** is a high-performance and **secure in-memory key-value store (IMKVS)** built for **multi-host systems using CXL-based Distributed Shared Memory (DSM)**. Unlike traditional IMKVS systems that suffer from high latency and security limitations, SHMemora introduces a **lightweight capability-based protection mechanism** to achieve strong isolation and fine-grained permissions with minimal overhead.

---

## Key Features

- **CXL Shared Memory**: Enables multiple hosts to directly load/store to a common shared memory pool via CXL 2.0/3.0.
- **Memory Protection**: Enforces memory access through protected paths only.
- **Unforgeable Capability Tokens**: Memory regions are protected by cryptographically authenticated tokens using HMAC. 
- **End-to-End Security Proofs and Penetration Tests**: Protection model verified both formally and experimentally.

---

## Project Structure

```
SHMemora/
├── include/                
│   ├── gshmp.h             # SHMemora shared memory API (token management, read/write, etc.)
│   ├── mhmpk.h             # Intel Memory Protection Keys (MPK) integration
│   ├── stats.h             # Performance monitoring and measurement
│   └── utils.h             # Utility functions and helpers

├── src/                   
│   ├── gshmp.cpp           # Core implementation of SHMemora's shared memory operations
│   ├── gshmp_upr.cpp       # Unprotected version of SHMemora
│   ├── stats.cpp           
│   └── utils.cpp           

├── test/                  
│   ├── kv.cpp              # Benchmark driver for IMKVS operations
```

## Setup & Compilation

### Prerequisites

- Intel GNR / AMD Turin processors
- Compatible CXL 2.0 or 3.0 memory device
- `numactl`, `libdaxctl`

### Compilation

```
bash

make
```

### Running benchmark

```
bash

./build/kv <nthreads> <value_size> <use_batch> <batch_kvs> <use_mac>
```

Example:

```
bash

./build/kv 32 256 1 5 0
```
- nthreads: Number of benchmark threads

- value_size: Size of the KV value in bytes

- use_batch: Enable batched tokens (1 for enabled, 0 for disabled)

- batch_kvs: Number of tokens per batch

- use_mac: Enable token HMAC verification (1 for enabled, 0 for disabled)

------

## Security Guarantee

SHMemora enforces strong memory isolation through a formally defined and tested security model, which:

- Prevents capability **token forgery** using HMAC authentication
- Detects and blocks **metadata tampering**
- Blocks **out-of-bounds memory access**



