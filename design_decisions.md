# Design Decisions

Every design choice in the engine is recorded here: the options considered with their pros and cons, the option chosen, and why. Entries are never deleted. When a decision changes, the old entry is marked **Superseded by D-xxx** and a new entry is added.

## Index

| ID | Phase | Decision | Status |
|---|---|---|---|
| [D-001](#d-001-build-system-and-build-presets) | 0 | Build system and build presets | Accepted |
| [D-002](#d-002-test-framework) | 0 | Test framework | Accepted |
| [D-003](#d-003-third-party-dependency-acquisition) | 0 | Third-party dependency acquisition | Accepted |
| [D-004](#d-004-sanitizer-build-matrix) | 0 | Sanitizer build matrix | Accepted |
| [D-005](#d-005-ci-provider-and-matrix) | 0 | CI provider and matrix | Accepted |
| [D-006](#d-006-code-style-and-formatting) | 0 | Code style and formatting | Accepted |
| [D-007](#d-007-library-layout-and-module-boundaries) | 0 | Library layout and module boundaries | Accepted |
| [D-008](#d-008-typed-identifiers) | 0 | Typed identifiers | Accepted |
| [D-009](#d-009-metrics-registry) | 0 | Metrics registry | Accepted |
| [D-010](#d-010-failpoint-framework) | 0 | Failpoint framework | Accepted |
| [D-011](#d-011-logging) | 0 | Logging | Accepted |
| [D-012](#d-012-benchmark-harness) | 0 | Benchmark harness | Accepted |

---

## D-001: Build system and build presets

**Subtask / phase:** Phase 0 – Repository setup

**Context:** The project needs a one-command build and a one-command test run, plus several build configurations (sanitizers, release) that must not be mixed up.

**Options considered**

| Option | Pros | Cons |
|---|---|---|
| CMake + `CMakePresets.json` workflow presets | Industry standard for C++; IDE support (VS Code, CLion); `cmake --workflow --preset X` runs configure, build and test in one command; each preset gets its own build tree | CMake language is verbose |
| CMake with hand-written scripts | Flexible | Configuration knowledge lives in shell scripts; easy to drift |
| Bazel | Hermetic, fast incremental builds | Heavy setup for a single-repo project; weaker IDE integration on macOS |
| Meson | Clean syntax | Smaller ecosystem; fewer contributors know it |

**Decision:** CMake (≥ 3.24) with configure, build, test and workflow presets: `debug`, `tsan` and `release`. Each builds into `build/<preset>/`. The default generator (Makefiles) is used so the build does not depend on Ninja being installed.

**Rationale:** Presets give exactly the "one command" property the plan requires, and they keep sanitizer trees separate by construction. Warnings (`-Wall -Wextra -Wpedantic`) and sanitizer flags are attached through an interface target so they apply to our code only. `-Werror` is on in all presets but is an option, so a newer compiler's new warning cannot block an ad-hoc local build.

---

## D-002: Test framework

**Subtask / phase:** Phase 0 – Repository setup

**Context:** Unit, integration, concurrency and crash tests are needed from Phase 1 onward. Crash tests need to check that a child process exits in a specific way.

**Options considered**

| Option | Pros | Cons |
|---|---|---|
| GoogleTest | Death tests (`EXPECT_EXIT`) for crash/failpoint tests; fixtures; value-parameterized and typed tests for differential testing; CTest integration via `gtest_discover_tests`; most widely known | Heavier compile; macro-heavy |
| Catch2 v3 | Expressive `REQUIRE`; BDD sections; good generators | No built-in death tests, so crash tests would need hand-rolled fork/exec helpers |
| doctest | Very fast compiles; header-only | Smaller feature set; no death tests |

**Decision:** GoogleTest 1.17.0.

**Rationale:** Death tests matter most for this project. Failpoint crash tests (Phase 0) and the crash/recovery suite (Phase 11) need to check exit codes of a crashed child, and GoogleTest supports this natively. CTest discovery also gives one CTest entry per test, so a single failing case is visible in CI.

---

## D-003: Third-party dependency acquisition

**Subtask / phase:** Phase 0 – Repository setup

**Context:** GoogleTest (and possibly later tools) must be available on a fresh clone and on CI without manual setup.

**Options considered**

| Option | Pros | Cons |
|---|---|---|
| CMake `FetchContent` from a release URL with pinned SHA-256 | Fresh clone builds with no extra steps; exact version pinned and verified; built with the same flags as our code | Needs network on first configure |
| System package (`find_package`) | No download | Version varies by machine; Homebrew/apt builds are not sanitizer-instrumented, which breaks TSan |
| Vendored copy / git submodule | Works offline | Bloats repo or adds submodule friction |

**Decision:** `FetchContent` with a pinned release archive and SHA-256 hash.

**Rationale:** Sanitizer builds need dependencies compiled with the same instrumentation. A prebuilt system GoogleTest would give TSan false positives. FetchContent builds it in-tree with our flags, and the hash makes the download reproducible.

---

## D-004: Sanitizer build matrix

**Subtask / phase:** Phase 0 – Repository setup

**Context:** Memory errors and data races are the most common and most costly bugs in a storage engine. ASan and TSan cannot be combined in one binary.

**Options considered**

| Option | Pros | Cons |
|---|---|---|
| Two sanitizer builds: ASan+UBSan (`debug`) and TSan (`tsan`) | Covers memory errors, UB and data races; ASan and UBSan combine cleanly | Two build trees; CI time doubles |
| ASan+UBSan only | Simpler | Misses data races, the main hazard from Phase 3 onward |
| Valgrind / Helgrind | No recompilation | 20–50× slowdown; weaker race detection than TSan |

**Decision:** Two sanitizer presets. `debug` is Debug + `-fsanitize=address,undefined` with UB made fatal (`-fno-sanitize-recover`). `tsan` is RelWithDebInfo + `-fsanitize=thread`. Sanitizer flags are applied globally, so GoogleTest is instrumented too.

**Rationale:** Concurrency is central to this engine, so TSan has to be a first-class build. TSan uses RelWithDebInfo because its overhead on top of `-O0` makes stress tests impractically slow, while debug info keeps race reports readable.

---

## D-005: CI provider and matrix

**Subtask / phase:** Phase 0 – Repository setup

**Context:** The repository is hosted on GitHub. Both sanitizer builds must run on every change.

**Options considered**

| Option | Pros | Cons |
|---|---|---|
| GitHub Actions | Native to the host; free for public repos; Linux and macOS runners | YAML-based; macOS minutes cost more for private repos |
| GitLab CI / CircleCI | Mature | Requires mirroring or a separate service |
| No CI, local scripts | Zero setup | Nothing enforces that both sanitizers pass |

**Decision:** GitHub Actions with a matrix of {ubuntu-24.04 (clang), macos-15} × {debug, tsan}, plus a separate clang-format check.

**Rationale:** Development happens on macOS, but fsync and file-I/O semantics differ on Linux, so both platforms are tested from the start. On Linux, `vm.mmap_rnd_bits` is lowered for the TSan job because recent kernels' ASLR entropy crashes TSan binaries at startup.

---

## D-006: Code style and formatting

**Subtask / phase:** Phase 0 – Repository setup

**Context:** A consistent style keeps diffs about behavior rather than whitespace. The action plan already uses names such as `ReadPage`, `FetchPage` and `BufferPoolManager`.

**Options considered**

| Option | Pros | Cons |
|---|---|---|
| Google style (via clang-format), 100 columns | Matches the plan's `PascalCase` method names; widely documented naming rules (`kConstant`, `member_`) | 80 columns is cramped for templates, so the column limit is raised |
| LLVM style | Also well known | `camelCase` methods conflict with the plan's naming |
| No enforced formatter | Nothing to set up | Style drift and noisy diffs |

**Decision:** clang-format with `BasedOnStyle: Google` and `ColumnLimit: 100`, pinned to clang-format 23.1.3 (installable with `pip install clang-format==23.1.3`). Naming: types and methods `PascalCase`, variables `snake_case`, members `snake_case_`, constants `kPascalCase`, namespace `db`.

**Rationale:** The style matches the class and method names already in the plan. The formatter is pinned because clang-format output changes between major versions, and an unpinned CI would fail on a version difference rather than a real style problem.

---

## D-007: Library layout and module boundaries

**Subtask / phase:** Phase 0 – Repository setup

**Context:** The plan requires visible module boundaries (common, storage, buffer, catalog, index, transaction, concurrency, execution, planner, recovery, server) with dependencies flowing downward only.

**Options considered**

| Option | Pros | Cons |
|---|---|---|
| One static library, modules as directories (`include/db/<module>`, `src/<module>`) | Simple build; no link-order issues while module interfaces are still changing | The dependency rule is not enforced by the build |
| One CMake library target per module | The build enforces the dependency rule: an upward include fails to link | Many targets to maintain; cross-module changes early on need CMake edits each time |

**Decision:** A single `minidb` library for now, with modules separated by directory and the allowed dependency direction documented in `docs/architecture.md`.

**Rationale:** Most modules are empty until their phase begins, so per-module targets would be boilerplate today. Revisit (as a superseding entry) once several modules exist, around Phase 5. That is when an upward dependency first becomes tempting.

---

## D-008: Typed identifiers

**Subtask / phase:** Phase 0 – Common utilities

**Context:** PageId, FrameId, TableId, IndexId, TxnId and LSN are all integers. Mixing them up (e.g. passing a frame index where a page number is expected) is a classic buffer-pool bug that compiles silently.

**Options considered**

| Option | Pros | Cons |
|---|---|---|
| `using PageId = uint32_t` aliases | Zero friction | No type safety at all |
| Hand-written struct per ID | Type-safe | Duplicated boilerplate for comparison, hashing and printing |
| One `StrongId<Tag, Rep>` template | Type-safe; one implementation; zero runtime cost | Slightly more verbose call sites (`PageId(3)`, `.value()`) |

**Decision:** `StrongId<Tag, Rep>` with an explicit constructor, defaulted `<=>`, `std::hash` and `operator<<`. A default-constructed ID is *invalid* (the max value of `Rep`). Widths: PageId, FrameId, TableId and IndexId are 32-bit; TxnId and LSN are 64-bit.

**Rationale:** Mixing up IDs is a compile error, not a corrupted page. Using the max value as the invalid sentinel keeps 0 usable as a real ID (page 0, frame 0). 32-bit page IDs allow 2³² pages (16 TiB at 4 KiB), well beyond this project. TxnId and LSN are 64-bit because they increase monotonically and must never wrap.

---

## D-009: Metrics registry

**Subtask / phase:** Phase 0 – Common utilities

**Context:** Later phases register counters (buffer hits/misses, lock waits, fsyncs) and latency histograms. Updates happen on hot paths, under contention.

**Options considered**

| Option | Pros | Cons |
|---|---|---|
| Atomic counters + fixed power-of-two bucket histograms, in a name → metric registry | Lock-free updates (~2–3 ns); fixed memory; simple | Percentiles accurate only to within 2× |
| HdrHistogram-style log-linear buckets | Percentiles within ~1% | More memory and code; overkill before Phase 14 |
| Mutex-protected sample vectors | Exact percentiles | Contention and unbounded memory on hot paths |
| External library (prometheus-cpp, etc.) | Feature-rich | Large dependency; network exposition not needed |

**Decision:** `Counter` (one relaxed `atomic<uint64_t>`) and `Histogram` (64 power-of-two nanosecond buckets plus count, sum and max), owned by a `MetricsRegistry` singleton keyed by dotted names. The registry mutex is taken only on lookup. Metrics are never removed, so returned references stay valid. Percentiles are clamped to the observed max.

**Rationale:** Metrics must be cheap enough to leave on everywhere. Measured cost: 2.2 ns per counter increment and 2.9 ns per histogram record (Release, Apple M-series). 2× percentile precision tells apart the latency ranges we care about (µs vs ms). If Phase 14 needs tighter p99 numbers, the bucket scheme can be replaced behind the same interface (superseding entry).

---

## D-010: Failpoint framework

**Subtask / phase:** Phase 0 – Common utilities

**Context:** Crash-recovery tests (Phase 11) and deterministic concurrency tests need to stop, fail or crash the engine at exact code locations, such as "after WAL append, before page write".

**Options considered**

| Option | Pros | Cons |
|---|---|---|
| Runtime registry, always compiled in, with an atomic fast path | The same binary is tested and shipped; arming is per test with no rebuild | One atomic load per site in production |
| Compile-time macro, enabled only in test builds | Zero production cost | Tests run a different binary than production; needs an extra build configuration |
| Mock/dependency injection of I/O layers | No macros | Only covers I/O boundaries, not arbitrary points such as mid-B+ tree split |

Error reporting at a site:

| Option | Pros | Cons |
|---|---|---|
| Macro evaluates to `bool`; the site returns its own error | Independent of any error type (not designed until Phase 5) | Each site writes one `if` |
| Macro returns a framework error type | Less code per site | Couples the framework to an error type that does not exist yet |

**Decision:** An always-compiled runtime `FailpointRegistry` with actions `kReturnError`, `kCrash` (`std::_Exit(77)`) and `kPause`, plus an optional hit-count limit. `FAILPOINT(name)` first checks a global armed-count atomic and touches the mutex-protected registry only when something is armed. It evaluates to `true` only for `kReturnError`. Pause supports `WaitUntilPaused` and `Resume`, so tests can synchronize without sleeping.

**Rationale:** An unarmed site costs about 1.3 ns (measured), so there is no reason to build a separate test binary. `std::_Exit` skips destructors and buffered writes, the closest in-process approximation of a crash. A dedicated exit code (77) lets tests tell injected crashes from real ones. Pause is resumed by an epoch counter rather than the armed state, so a one-shot (`count=1`) pause still holds its thread until the test releases it. While any point is armed, every site takes the mutex (about 9 ns). That cost applies only during fault-injection tests.

---

## D-011: Logging

**Subtask / phase:** Phase 0 – Common utilities

**Context:** Debug output is needed from storage and recovery code, but tests and benchmarks must stay quiet by default.

**Options considered**

| Option | Pros | Cons |
|---|---|---|
| Minimal in-house leveled logger on `std::format` | No dependency; ~60 lines; disabled levels skip formatting entirely | No sinks, rotation or async output |
| spdlog | Fast, feature-rich, async | Extra dependency (and fmt); more than needed |
| Bare `fprintf(stderr, …)` | Trivial | No levels; lines from concurrent threads can interleave |

**Decision:** A `Logger` with levels debug/info/warn/error/off, set by `MINIDB_LOG_LEVEL` (default `warn`). It writes one formatted line per `fwrite` to stderr. The `LOG_*` macros check the level before evaluating any arguments.

**Rationale:** The engine's observability comes mainly from metrics and debug tools (pagedump, treeprint, loginspect). Logging is for occasional diagnostics. One `fwrite` per line keeps concurrent output readable without a logger mutex.

---

## D-012: Benchmark harness

**Subtask / phase:** Phase 0 – Repository setup

**Context:** Phase 0 needs a benchmark executable. Phase 14 will need end-to-end workload benchmarks (YCSB-style), not micro-benchmarks.

**Options considered**

| Option | Pros | Cons |
|---|---|---|
| Hand-written timing harness | No dependency; full control over workload shape; matches the needs of the Phase 14 workload driver | No automatic iteration tuning or statistics |
| Google Benchmark | Statistically careful micro-benchmarks | Another dependency; designed for micro-benchmarks, not multi-threaded workload drivers |

**Decision:** A small hand-written harness (`benchmarks/micro_bench.cpp`) for hot-path primitives, run from the Release preset.

**Rationale:** The important benchmarks (Phase 14) are workload drivers with Zipfian keys, many threads and percentile reporting through our own metrics registry. Google Benchmark does not fit that model. If a micro-benchmark later needs rigorous statistics, Google Benchmark can be added for that case alone.
