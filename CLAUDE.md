# CLAUDE.md

## Project

Mini Database Engine: a small relational database engine in C++20, built from scratch. The full build sequence (Phases 0–14) is in `../mini_database_engine_action_plan.pdf`, outside this repository.

The project plan is `../plan.md`. Layering rules are in `docs/architecture.md`.

## Build and test

- `cmake --workflow --preset debug`: configure, build and test with ASan+UBSan
- `cmake --workflow --preset tsan`: the same with ThreadSanitizer (run for any concurrency change)
- `cmake --build --preset debug && ctest --preset debug -R <regex>`: rebuild and run a subset of tests
- All presets build with `-Werror`. Format with clang-format 23.1.3 (CI checks it).

## code convention
- Add comments explaining non-obvious logic
- Google style naming: `PascalCase` types and methods, `snake_case_` members, `kConstant`, namespace `db`
- Use the typed IDs from `db/common/ids.h`, never raw integers, for page/frame/table/index/txn/LSN values
- Add a `FAILPOINT("module.event")` at crash-relevant points, and register metrics through `MetricsRegistry`
- Concurrency tests synchronize with barriers or failpoint pauses, never with sleeps

## Design decisions log (required)

Keep `design_decisions.md` at the repository root up to date. Every time a subtask involves a design choice (page size, record format, buffer/recovery policy, replacement policy, latch ordering, locking protocol, isolation level, deadlock victim policy, recovery model, server threading model, and so on), add an entry before or alongside the implementation.

Each entry must contain:

1. **Subtask / phase**: which subtask the decision belongs to (e.g. "Phase 3 – Buffer pool: replacement policy").
2. **Context**: the problem being solved and any constraints.
3. **Options considered**: every viable alternative, each with its **pros** and **cons**.
4. **Decision**: the option we chose.
5. **Rationale**: why we chose it over the others, and what trade-offs or limitations we accept.

Rules:

- Do not make a design choice silently in code. If it isn't in `design_decisions.md`, add it.
- If a later decision changes an earlier one, keep the original entry, mark it superseded, and link to the new entry.
- Discuss the options with the user before settling on a decision for anything that is hard to change later (on-disk formats, recovery policy, locking protocol).
