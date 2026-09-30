# Pocket Harness 0.12.0 measurements

Measured on 2026-09-30 on an AMD Ryzen 5 7530U (12 logical CPUs), Ubuntu 26.04.1,
with GCC 15 and the same configured provider roles. Baseline is commit
`3e93e52a94f94f928ae366c6948d477f6fdeaab3` (0.11.3); candidate is 0.12.0.
Compilation is outside timed regions. Native commands run sequentially; paired
orders alternate. These small samples describe these workloads on this machine,
not provider-wide latency guarantees.

## Agent execution

Five pairs use deterministic model responses delayed 40 ms/request and 12 ms per
judge/observer call. The simple case edits and verifies a title. The complex case
reads 24 distinct evidence files, writes its deliverable, and verifies it. Both
check the final file, tool count and conversation validity. Synthetic prompt
estimates are labeled fixture tokens; they are **not billed API usage**.

| Fixture | Median before → after | Model requests | Coordination calls | Tool calls | Fixture input / output tokens |
|---|---:|---:|---:|---:|---:|
| Simple title edit | 263 → 191 ms (-27.4%) | 5 → 4 | 4 → 2 | 2 → 2 | 3615/100 → 2342/80 |
| Complex steady progress | 1256 → 1233 ms (-1.8%) | 29 → 29 | 6 → 4 | 26 → 26 | 45633/580 → 45633/580 |

Coordination counts include planning/review model requests, decisions and local
observer calls. The simple case removes its planning request and an unnecessary
decision; the complex case retains its planner/reviewer and removes two observers
that previously ran during healthy discovery. All ten paired executions pass.
The complex time difference is small; its measured gain is fewer coordination
calls, with no token reduction in this fixed transcript.

Three paired live runs ask `Compute 2 + 2. Answer with just the digit 4.` using
implicit model selection, 1024 max output tokens and three max rounds. All six
return exactly `4`. Median elapsed time is **1441 → 950 ms (-34.1%)**; each run
uses zero tools. Reported harness input/output tokens are 2177/2 → 2174/2.
Completion judge calls fall 1 → 0; the observed health gate retains the configured
DeepSeek main provider instead of the historically slower/unreliable fast role.
Jev side-token usage is not included in the harness token count, and the provider
does not report complete costs, so no total-cost claim is made.

An initial live candidate was slower: 2857 → 3471 ms. Inspection of learned
provider history found the fast role had approximately 40% observed health and
20.96 s latency versus main's 100% and 2.96 s. The final policy rejects that route
and removes trivial completion judgments. The paired live measurements above
were rerun after both fixes; unrelated configuration was preserved.

The live multi-file workload implements `dijkstra(edges, start)` in Python,
writes unittest coverage and README, runs tests, and rejects negative weights
anywhere, including unreachable components. An external acceptance oracle checks
shortest paths, zero weights, absent adjacency, cycles and disconnected negative
weights separately from the generated tests. All six executions pass both the
independent oracle and their own 11–16 generated tests. Three alternating live
pairs give these medians:

| Live Python task | Before | After | Change |
|---|---:|---:|---:|
| Completion time | 35.374 s | 38.370 s | +8.5% |
| Reported input tokens | 34,809 | 41,496 | +19.2% |
| Reported output tokens | 4,701 | 4,675 | -0.6% |
| Tool calls | 9 | 8 | -11.1% |
| Observed Jev/Span calls | 4 | 3 | -25.0% |

This is a measured regression in median elapsed time and reported input, and no
complex live speed/token gain is claimed. Inspection found no retry, compaction,
extra observer or repeated council loop: each trace has one planning notice and
one reviewer notice. Median reported generation time falls 20.472 → 19.580 s;
the remaining elapsed time includes remote planning, judgments, review, tools and
transport, which are not individually timed here. Main response counts differ
(before 10/6/6, after 7/6/8), reflecting stochastic implementation and batching.
Nearly all extra input is reported cache reuse: hit tokens 21,760 → 28,800 while
miss tokens remain 13,049 → 13,065. The last candidate pair is faster (31.692 vs
34.760 s), so the samples do not establish a consistent causal latency penalty.
Remote phase variability and differing generated traces remain practical limits;
conservatively retain the measured +8.5% median result.

## Native music and video

Three sequential pairs synthesize 60 seconds at 48 kHz stereo, seed 7, for each
style. The optimization hoists note-invariant frequency and pan calculations and
skips notes outside the requested timeline. Entire WAV hashes match in every
style; faster synthesis produces the same PCM16 samples.

| Workload | Median wall seconds before → after | Reduction |
|---|---:|---:|
| Lo-fi score | 3.182 → 1.966 | 38.2% |
| Corporate score | 2.010 → 1.735 | 13.7% |
| Cinematic score | 1.343 → 1.018 | 24.2% |
| Tech score | 1.781 → 1.380 | 22.5% |
| 3 s video, 960×540, 12 fps | 3.319 → 3.199 | 3.6% |

Video waited-child CPU time is 1.317 → 1.137 s (-13.7%). All 36 decoded RGB frames
hash identically. Wall time varied between runs; the 3.6% change is modest and not
evidence of a broad video latency gain. The substantive animation improvement is
correctness: short CSS/WAAPI animations remain seekable after delayed readiness.
Music/video commands make zero model or coordination calls; LLM tokens do not
apply to these native measurements.

Real Chrome/FFmpeg integration checks exact CSS/WAAPI pixel extents at 0 and
50 ms despite a 500 ms readiness delay, native audio clipping, H.264/AAC streams,
frame count, full decode and `vcheck`. Audio regressions also cover full-bed mixes
with short cues, corrupt RIFF layouts, center-only surround dialogue, high-rate
alias rejection, timeline bounds and short cue loudness. The reusable six-second
motion example exports 144 frames with both audio and video at 6.000 s and no
`vcheck` faults. Quality-changing resampling/downmix fixes have dedicated numerical
or integration tests; waveform parity claims apply to unchanged synthesis/export
workloads only.

## Reproduce and inspect raw data

```sh
make -j4 test
make check-workflows
make bench
python3 tests/media_integration.py ./pocket --out build/media-integration
python3 tests/benchmark_media.py --before /path/to/0.11.3/pocket \
  --after ./pocket --out build/media-benchmark --runs 3
./scripts/test-release.sh
```

For the fixture comparison, build the baseline sources at the commit above and
compile the current `tests/benchmark_agent.cpp` against their native objects
(excluding `src/main.o`); compare that executable with `make bench`. The fixture
adds no dependency and makes no provider calls. Run it from a workspace outside
`/tmp`, since confinement tests distinguish workspace paths from allowed temp
paths. Live tasks use fresh empty workspaces and the unchanged user roles; they
require valid configured provider credentials and incur API usage. Prompts,
per-run counts, hashes and full timing samples are available in
[fixture data](benchmark-data/policy-final-paired.json),
[live simple data](benchmark-data/live-final.json),
[live complex data](benchmark-data/live-complex.json) and
[native media data](benchmark-data/media-final.json).

GCC, Clang and AddressSanitizer/UndefinedBehaviorSanitizer each pass **373 tests**.
The source binary is approximately 2.8 MB, 2.4% larger than baseline, with the same
standard C++/libc/math linked dependencies. Python is used for development checks;
it is not a new mandatory runtime dependency. Remote hosting deployment and model
training are reusable capability-aware workflows; this release does not establish
credentials, launch a Colab training job or deploy an unrelated production site.
