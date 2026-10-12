# Architecture

## Layers

The engine is layered. Each layer may call only layers below it.

```
Client
  └─ server       sockets, sessions, text protocol          (Phase 13)
      └─ parser / planner   SQL → AST → bound plan          (Phase 5)
          └─ execution      iterator-style operators        (Phases 5–6)
              └─ transaction   txn lifecycle, undo          (Phase 7)
                  └─ concurrency  lock manager, MVCC        (Phases 9, 10, 12)
                      └─ catalog / index / table heap       (Phases 2, 4, 6)
                          └─ buffer   buffer pool, latches  (Phases 3, 8)
                              └─ recovery  WAL, ARIES       (Phase 11)
                                  └─ storage  disk, pages   (Phase 1)
common: typed IDs, metrics, failpoints, logging  — usable from every layer
```

`recovery` sits below `buffer` because the buffer pool must call `LogManager::FlushUntil(pageLSN)` before writing a dirty page, so WAL flushing is a dependency of eviction. Recovery *orchestration* (analysis/redo/undo at startup) runs above all of these and drives them.

## Rules

- **Downward-only dependencies.** For example, `storage` never includes from `buffer`, and nothing below `execution` knows SQL exists.
- **Latches vs locks.** Latches protect in-memory structures for microseconds and are held by threads. Locks protect logical data for a transaction's lifetime. Never hold a latch while waiting for a lock (enforced from Phase 8).
- **Every design choice is logged** in [`design_decisions.md`](../design_decisions.md).

## Directory layout

| Path | Contents |
|---|---|
| `include/db/<module>/` | Public headers of each module |
| `src/<module>/` | Implementation |
| `tests/<module>/` | GoogleTest unit and integration tests |
| `benchmarks/` | Benchmark executables |
| `tools/` | Debug tools: `pagedump` (Phase 1), `treeprint` (Phase 6), `loginspect` (Phase 11) |
| `docs/` | Design notes |

## Common utilities (Phase 0)

| Header | Purpose |
|---|---|
| `db/common/ids.h` | `PageId`, `FrameId`, `TableId`, `IndexId`, `TxnId`, `LSN`: distinct types, invalid by default |
| `db/common/metrics.h` | `MetricsRegistry` with lock-free `Counter` and `Histogram`, and the `ScopedTimer` helper |
| `db/common/failpoint.h` | `FAILPOINT("name")` injection sites; tests arm them to error, crash or pause |
| `db/common/logger.h` | `LOG_DEBUG/INFO/WARN/ERROR`, level set by `MINIDB_LOG_LEVEL` |
