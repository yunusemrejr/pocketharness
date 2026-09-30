# Pocket Harness 0.13.0 measurements

Measured on 2026-09-30 on the same AMD Ryzen 5 7530U Linux computer used for
0.12.0. The baseline source is commit `071859989578a92969ed604baa6b3636d633766f`
(0.12.0); the candidate is 0.13.0. Compilation is outside timed regions.
Final paired orders alternate. The initial fixture investigation ran baseline
then candidate for each pair and is retained separately. Live/native comparisons use the exact
installed 0.12.0 release binary and the locally compiled candidate, so compiler
and provider variability limit attribution. Fixture comparisons compile both
source revisions with the same updated fixture and compiler.

Delivery note: the 0.13.0 tag was blocked before publication by GCC 13's
`-Werror=range-loop-construct` in an existing audio test. The delivered version
is 0.13.1, with those two test bindings changed to references and the version
string updated. The measured execution implementation is otherwise unchanged;
the original measurement filenames remain intact.

## Controlled agent fixtures

Three final alternating pairs use deterministic model responses delayed 40 ms/request and 12 ms
per judgment. The simple task edits and verifies a title; the complex task reads
24 distinct evidence files, writes its deliverable and verifies it. A third task
executes eight reads returning the same large evidence. All 18 executions pass
file, observed successful verification, tool-count and conversation-validity checks.
The final fixture distinguishes observed reviewer stdout from command text: the
baseline omits that receipt and the candidate includes it. Initial fixture checks
did not require this additional receipt assertion and are retained separately.

| Fixture | Median milliseconds before → after | Estimated input tokens | Output tokens | Model requests | Tools | Coordination calls |
|---|---:|---:|---:|---:|---:|---:|
| Simple title edit | 192 → 192 | 4,798 → 4,616 (-3.8%) | 80 → 80 | 4 → 4 | 2 → 2 | 2 → 2 |
| Complex steady progress | 1,235 → 1,224 (-0.9%) | 66,712 → 64,723 (-3.0%) | 580 → 580 | 29 → 29 | 26 → 26 | 4 → 3 |
| Repeated large result | 464 → 378 (-18.5%) | 39,384 → 38,276 (-2.8%) | 180 → 180 | 9 → 9 | 8 → 8 | 8 → 1 |

Simple-task timing is unchanged. The complex fixture saves one 12 ms judgment;
this synthetic saving does not establish a general complex-task wall-speed gain.
Compact context reduces input while preserving the same work. The first simple
system message is 3,017 → 2,497 bytes. Reusing
exact repeated evidence before relevance judgment removes seven decisions;
decision-state JSON falls 162,696 → 20,337 bytes (-87.5%); this excludes the
judge's question envelope. The tools still execute.
An inspection of 30 recent sessions found 764 tool results, 60 over 2 KB and no
exact large repeats, so the repeated-output benefit is demonstrated on the
controlled case, not claimed as a measured benefit in those recent sessions.

The refreshed fixture estimate includes the system message that the real
provider prepends after request-body construction. The 0.12.0 report used an
estimate omitting that message. Both revisions here use complete request
envelopes; historical fixture totals are not directly comparable. These are
synthetic model-request estimates, not billed API tokens; judge-token usage is
excluded. Coordination includes planning and
review model requests, decisions and local observers.

## Live sessions and regression investigation

Live tokens are provider-reported model-request totals and exclude Jev/Span
judge-token usage. Reported pricing is incomplete, so these samples support
prompt/coordination comparisons rather than total-cost claims.

The initial three simple pairs all answer exactly `4` with zero tools/judges.
Reported input is 2,177 → 2,068 tokens (-5.0%), output two tokens. Wall median
regresses 944 → 1,131 ms (+19.7%), while the mean is 1,065 → 1,028 ms.
Main-request interval median is 873 → 1,091 ms; outside-request residual is
45.9 → 39.9 ms. The request boundary includes preparation, recording, transport,
provider execution and retries, so it is not pure server inference time. One
candidate request has zero cache hits. Two additional alternating pairs give
wall medians 1,071 → 972 ms; outside-request residuals 40.5 → 38.4 ms and
waited-child CPU 49.7 → 47.0 ms. Both answers in each pair are correct. Prompt
input remains 109 tokens lower, with one longer reported reasoning output per
revision. Timing varies in both directions; no live-simple wall-speed guarantee
is established. Initial and confirmation samples are both retained.

The initial live Python task implements Dijkstra across source, tests and README
with the standard library. Independent acceptance checks shortest paths, zero
weights, cycles, absent adjacency/start and negative weights in disconnected
components. Baseline passes 2/3; one returns an empty dict for an absent start,
despite passing its own tests. Candidate passes 3/3, including 11–21 delivered
tests. Median time per attempt is 23.938 → 34.494 s (+44.1%), input
31,088 → 56,879 (+83.0%), output 3,612 → 5,866, tools 7 → 10 and observed Jev/Span
calls 3 → 6. The baseline median includes its failed attempt and therefore is not
a median of correct task completions. Extra tool-side checks account for three
initial judge batches; generated code/tests differ between stochastic runs.

Trace inspection found a reproducible evidence defect: the council digest kept
the Bash exit-status header but discarded actual stdout/stderr, including
passing unittest counts and details. One
review then demanded repeated full test/example output despite accepting the
code. Another run expanded numeric validation and tests and repeatedly checked
passing results. Median main-request intervals are 18.204 → 24.549 s; cache-hit
input is 18,048 → 39,808 and miss input 13,040 → 17,071; no compaction occurred.
This is a measured regression, not concealed by the deterministic token savings.

The final refinement preserves bounded command receipts including exit evidence,
asks reviewers for concrete defects rather than log volume, combines overlapping
small-task assessments and skips a complex-task prefilter that cannot bypass its
required independent review. Unknown assessments retain normal review. Final
samples below verify that refinement; they do not attribute stochastic generated
output differences solely to the harness.

The second complex sample, after receipt/prefilter refinements, has baseline
3/3 versus candidate 2/3 independent passes. Median time per attempt is
29.061 → 24.192 s (-16.8%), input 62,934 → 35,908 (-42.9%), output
5,377 → 4,319, tools 11 → 9 and observed Jev/Span calls 3 → 4. Lower medians
with mixed correctness do not establish a successful-task speed/quality gain.
The failing candidate passes its own 20 tests but violates the absent-start
acceptance case. Its trace identifies the cause: the generated brief invents
"unknown start returns {}", and the main model explicitly chooses that generated
criterion over the user's "including start" requirement. The source is fully
visible to the reviewer, so this failure is not attributed to the receipt fix.

The final correction gives generated plans explicit advisory status throughout
planning, execution, council and goal audit. Explicit human requirements remain
authoritative; generated edge-case constraints cannot replace them. A generic
empty-export/header preservation regression checks this precedence. No extra
uncertain judge loop is added. Final sampling verifies the released behavior,
while both earlier samples remain available.

Final simple samples all answer `4` with zero tools/judges. Reported input is
2,177 → 2,074 (-4.7%), output two tokens. Wall median is 819 → 715 ms (-12.7%),
with wide overlapping ranges (644–978 versus 629–1,224 ms). Earlier samples
varied in both directions; this does not establish a consistent live-simple
wall-speed gain.

The final complex sample passes the independent oracle and delivered tests in
all six attempts. Baseline delivers 7–11 tests and candidate 14–23. The same
workload/provider roles give these medians:

| Final live complex task | Before | After | Change |
|---|---:|---:|---:|
| Completion time | 23.443 s | 33.163 s | +41.5% |
| Reported input tokens | 33,684 | 58,028 | +72.3% |
| Reported output tokens | 3,951 | 6,348 | +60.7% |
| Tool calls | 8 | 10 | +25.0% |
| Observed Jev/Span calls | 3 | 4 | +33.3% |

This remains a live complex latency/token regression. The added tool-side checks
are intentional, bounded quality work; removal of the redundant final prefilter
keeps their net increase to one judge call. All final traces have one planning
notice, one independent council review and no council-objection loop. Stochastic
code/test generation and extra verification differ between revisions. The
sample does not isolate harness overhead from provider/generation differences,
and no complex live speed or token gain is claimed. Do not dismiss this regression
because the fixed transcript improves: it remains an unresolved performance
limit under these configured provider conditions. The planner precedence defect
has a regression test and all final acceptance cases pass, which does not promise
error-free model output on other tasks.

## Native failure deadlines and media

A loopback listener with backlog zero and an already occupied accept queue
reproduces the old blocking-connect bug. With a requested 100 ms deadline, all
three baseline commands exceed the external 2 s cutoff. All three candidates
return failure in 102.4–102.8 ms; the median is 102.59 ms. The connection still
fails, but execution now honors its resource bound. Unit tests also cover
stalled NSS resolution, HTTP transport/status evidence and finite argument
validation. This is a correctness/deadline improvement, not a successful-network
throughput claim.

Three alternating video pairs export 3 seconds, 960×540, 12 fps. All 36 decoded
RGB frames of the retained final videos hash identically. Median wall time is
3.348 → 3.181 s and waited-child
CPU is 1.352 → 1.393 s. The small wall difference and compiler/environment
variance do not establish a video speed gain. New real-media checks establish
footage seeking, offsets, cue placement, explicit looping, fallback sources and
required-asset failure without replacing completed output. CSS/WAAPI, H.264/AAC,
audio mix/decode and `vcheck` checks continue to pass.

A local fixture with a 256 MiB GLB binary chunk and small metadata (one buffer,
no meshes/accessors) is preflighted in a median 2.116 ms across five runs, with
peak RSS at most 4,628 KiB. Metadata inspection skips the binary payload instead
of loading geometry. This is a new capability, without a baseline time comparison.
This measures skipping a large payload, not processing a complex mesh graph.
Preflight does not certify full glTF conformance, remote dependencies, extension
payloads or rendered appearance. The bundled Three.js starter was separately
rendered at absolute times: frames differ when rotated, a repeated time produces
identical pixels, and a two-second export has 16 H.264 frames with passing
`vcheck`. Inspect real output before delivering any scene.

Native diagnostics and media commands make no model or coordination calls;
LLM token metrics do not apply. The explicit semantic quality check is different:
a live factual-claim fixture answered 2/2 questions in one Jev batch and flagged
the unsupported claim (p=0.88), at reported judge cost $0.000027. That advisory
signal requires evidence checking and does not guarantee quality.

## Validation and reproduction

GCC, Clang and AddressSanitizer/UndefinedBehaviorSanitizer each pass **405 tests**.
Workflow/linked-guide contracts and isolated package/install checks pass. The
local source binary is 2,923,208 bytes with the existing standard C++/libc/math
linked libraries; no new runtime service, Node SDK or agent framework is added.
Python remains development tooling. Browser rendering still needs an available
Chromium and FFmpeg, and semantic checks need configured judge credentials.

```sh
make -j2 test
make check-workflows
make bench
python3 tests/media_integration.py ./pocket --out build/media-integration
python3 tests/benchmark_media.py --before /path/to/released/0.12.0/pocket \
  --after ./pocket --out build/video-pairs --runs 3 --workload video
./scripts/test-release.sh
```

For fixtures, compile current `tests/benchmark_agent.cpp` against native baseline
objects (excluding `main.o`), then compare with `make bench`. Use the same GCC and
fixture code on both revisions. Run outside `/tmp` because confinement tests
exercise workspace authority separately from permitted temporary roots.

Raw results: [final agent fixtures](benchmark-data/0.13.0/fixture-pairs.json),
[initial agent fixtures](benchmark-data/0.13.0/fixture-initial.json),
[final simple live](benchmark-data/0.13.0/live-final.json),
[TCP deadline](benchmark-data/0.13.0/tcp-deadline.json),
[video pairs](benchmark-data/0.13.0/video-pairs.json),
[GLB preflight](benchmark-data/0.13.0/gltf-preflight.json),
[initial simple live](benchmark-data/0.13.0/live-initial.json),
[initial timing investigation](benchmark-data/0.13.0/live-initial-timing.json),
[simple confirmation](benchmark-data/0.13.0/live-confirm.json),
[initial complex live](benchmark-data/0.13.0/live-complex-initial.json) and
[complex timing investigation](benchmark-data/0.13.0/live-complex-initial-timing.json) and
[second complex sample](benchmark-data/0.13.0/live-complex-evidence-refinement.json),
[final complex live](benchmark-data/0.13.0/live-complex-final.json) and
[final complex timing](benchmark-data/0.13.0/live-complex-final-timing.json).
