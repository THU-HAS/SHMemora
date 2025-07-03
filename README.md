# SHMemora: Protective Key-Value Store on Distributed Shared Memory

**SHMemora** is a high-performance and **secure in-memory key-value store (IMKVS)** built for **multi-host systems using CXL-based Distributed Shared Memory (DSM)**. Unlike traditional KV store systems that suffer from high latency and security limitations, SHMemora introduces a **lightweight protection mechanism** and **capability-based access control** to achieve strong isolation and fine-grained permissions with minimal overhead.

---

## Key Features

- **CXL Shared Memory**: Enables multiple hosts to directly load/store to a common shared memory pool via CXL 2.0.
- **Memory Protection**: Enforces memory access through protected paths only.
- **Unforgeable Capability Tokens**: Memory regions are protected by cryptographically authenticated tokens using HMAC. 
- **Partitioned Memory Layout**: Data segments are partitioned by host to improve scalability.
- **End-to-End Security Proofs and Penetration Tests**: Protection model verified both formally and experimentally.

---

## Project Structure

```
SHMemora/
├── include/                
│   ├── gshmp.h             # SHMemora shared memory API (token management, read/write, etc.)
│   ├── mhmpk.h             # Intel MPK-based memory protection interface definitions
│   ├── stats.h             # Declarations for performance statistics functions
│   └── utils.h             # Utility function definitions

├── src/                   
│   ├── gshmp.cpp           
│   ├── gshmp_upr.cpp       
│   ├── stats.cpp           
│   └── utils.cpp           

├── test/                  
│   ├── kv.cpp              # KV insert/get/update/delete benchmark
│   ├── generator.hpp       # Random key/value generator
```

## Setup & Compilation

### Requirements

- Linux OS (Ubuntu 22.04+ recommended)
- CXL 2.0 device
- `g++`, `make`, `numactl`, `libdaxctl`

### Compilation

```
bash

make
```

### Running benchmark

You can run the KV benchmark with different parameters:

```
bash

./build/kv <nthreads> <value_size> <use_batch> <batch_kvs> <use_mac>
```

Example:

```
bash

./build/kv 32 256 1 5 0
```



## Internals

- Memory regions are accessed and verified using `shkey_t`, which consists of `token_id` and `HMAC`.
- A hash table validates access permissions before any memory read/write.
- Uses `clflush` + `mfence` to ensure persistence and avoid cache inconsistencies when necessary.
- Lock-free operations such as `CAS` improve performance under contention.

------

## Security Guarantee

SHMemora protection model:

- Prevents **token forgery** via HMAC
- Detects and stops **metadata tampering**
- Blocks **out-of-bounds memory access**
- Enforced via **formal proofs** and **penetration test cases**



