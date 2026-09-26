---
name: debugging
description: Systematic debugging — reproduction, minimization, bisection, hypothesis/verification discipline, instrumentation, and knowing when to stop and reconsider. Use whenever fixing any bug, no matter how small the task looks.
---

# Debugging (discipline over intuition)

## The loop

**Reproduce → Minimize → Bisect → Hypothesize → Verify → Fix → Regress**

Skipping steps is how bugs become three bugs.

1. **Reproduce first.** No repro = no debug, only archaeology. Get a command, a payload, a click sequence that fails deterministically. "Sometimes on my machine" must become "when X runs with Y in state Z". If you can't reproduce, capture the environment (versions, config, data sample, logs, exact commands) and say so explicitly instead of guessing.
2. **Minimize the failing case.** Smallest input, fewest lines, one file. Minimizing is *diagnosis* — each removal either kills the bug or tells you what it needs. A 40-line reproducer with one surprising line is 90% of the fix.
3. **Bisect the cause:**
   - Time: `git bisect` (run the reproducer at each commit — make it a script: `git bisect run ./repro.sh`), or the change log (what shipped between "worked" and "broke").
   - Code: comment/halve input paths, disable subsystems one at a time.
   - Data: halve the dataset until the trigger row surfaces.
   - Config: default config vs. production config diff, apply half.
4. **One hypothesis at a time.** State it in one sentence where it would be *wrong* (falsifiable). Then design the check. A hypothesis you can't falsify ("maybe it's a race") isn't a hypothesis.
5. **Instrument at boundaries, not inside.** Log enter/exit + state of the suspect boundary (what came in, what went out). Read the actual error precisely: file, line, symbol, and the *whole* message — the cause is usually in the line you didn't read.
6. **Fix the cause, not the symptom.** Symptom-fix (retry the failed request, drop the odd row, widen the timeout) is a decision — fine as a stop-bleed, but the incident stays open until the cause has a fix or a written "accept this" decision.
7. **Regress:** add/adjust the test that failed at step 1 (or the minimal script) so the bug can't return silently. Run the full relevant suite — the fix broke something else is data about your model of the system.

## Language-of-the-bug heuristics

- **Intermittent:** concurrency (shared state, check the write side), time (clock, timeout, expiring credential), data (specific row/case), resource (fd/memory exhaustion under load).
- **Works in dev, not prod:** environment diffs — config, versions, *data*, time zones, limits (fd, RAM, path length), missing migrations. Enumerate the diff; don't vibes it.
- **"It used to work":** what changed is suspect — dep bump, config, data format, schedule (cron at month boundary), or *the other service* changed. `git log` + deploy log + dependency changelogs, in that order.
- **Performance:** profile before touching code (see `web-performance` / `linux` for tools). 90% of perf bugs are N+1 queries, unindexed scans, memcpy-in-a-loop, or a missing cache that was never added.
- **Crash/heap:** `coredumpctl` / minidump for the stack; check `dmesg` (OOM kills show up as kernel kills, not app logs).
- **State bugs (diff appears after X):** find where state is *cached* (memory, disk, DB, CDN, browser) and check whether the cache is invalidated the way the writer believes it.

## Tools by symptom (quick)

- Wrong output with no crash: log at boundaries + diff against a known-good input.
- Hang: stuck-in-syscall → `strace -p PID`; JS → Performance panel (long task → its callees); `kill -QUIT`/dlv for Go; jstack for JVM.
- Memory leak: heap snapshots before/after N cycles (browser), `valgrind`/`heaptrack` (native), RSS curve over time (system).
- Race: `TSAN`/`-fsanitize=thread` (C/C++), lock-free reproducer under a load script; print ownership transitions.
- Data wrong: `git bisect` on data pipeline commits; compare row counts/summaries between stages.

## When to stop

- Two failed hypotheses + no new information → **change level**: read the whole offending code path linearly, or find someone who built it.
- The reproducer is environment-dependent and unbuildable → document what's captured, what's assumed, and what would make it reproducible; ask the user for the missing bit (prod access? a failing payload?).
- **Never** ship a confidence-claim ("fixed the race") without an execution or a test that exercises the condition.

## The 3am test

Before you call it fixed, ask: if this recurs at 3am, does the next engineer (you, sleep-deprived) get from "it's broken" to "this line" in < 30 min with the artifacts you left (alert → log → repro path)? If no, invest 10 minutes in the artifact, not the code.