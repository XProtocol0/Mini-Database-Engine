# Mini-Database-Engine

A small relational database engine written from scratch in C++20. When complete it will have page storage, a buffer pool, B+ tree indexes, latching, transactions, a lock manager with deadlock detection, WAL with ARIES-style recovery, MVCC, and a client/server layer.

**Status:** Phase 0 (engineering setup) is done. See [docs/architecture.md](docs/architecture.md) for the layer design and [design_decisions.md](design_decisions.md) for the decision log.

## Requirements

- CMake ≥ 3.24
- A C++20 compiler with `<format>`: Clang ≥ 17 or Apple Clang ≥ 15. GCC ≥ 13 should also work but is not tested in CI.
- Network access on first configure (GoogleTest is fetched automatically)

## Build and test

Each command configures, builds and runs all tests in its own `build/<preset>/` directory:

```sh
cmake --workflow --preset debug     # Debug + AddressSanitizer + UBSan
cmake --workflow --preset tsan      # ThreadSanitizer
cmake --workflow --preset release   # optimized, no sanitizers
```

After the first run, to rebuild and re-test:

```sh
cmake --build --preset debug && ctest --preset debug
ctest --preset debug -R Failpoint   # run a subset of tests
```

## Benchmarks

```sh
cmake --workflow --preset release
./build/release/benchmarks/micro_bench
```

## Formatting

```sh
pip install clang-format==23.1.3
find include src tests benchmarks \( -name '*.h' -o -name '*.cpp' \) | xargs clang-format -i
```

## Repository layout

```
include/db/<module>/   public headers      src/<module>/   implementation
tests/                 GoogleTest suites   benchmarks/     benchmark executables
tools/                 debug tools         docs/           design notes
```
