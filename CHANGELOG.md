# Changelog

## 0.7.0

- Recover from provider hiccups automatically. "Provider returned error",
  overloaded/upstream/rate-limit bodies, gateway codes (408/409/425/52x),
  in-stream error events, JSON error bodies inside HTTP 200 and truncated
  streams now retry with backoff while nothing visible was emitted; the
  agent then retries once more on the same model after fallbacks.
- Models without image input: a vision rejection marks the model blind for
  the session and resends with images replaced by a short placeholder.
- Double mode is faster and tighter: the evidence loop ends with a forced
  conclusion round (budget-exceeding tool calls never run), a straggling
  second stream is cut once the first finishes, and the reconcile/parent
  seed carries a digest of what the analyses already inspected so evidence
  is not re-gathered.
- Council reviewers run in parallel; the request brief runs concurrently
  with skill hinting; a down decisions endpoint no longer stalls twice
  (primary then backup). Skill hints without a Jev verdict only fire when
  the request names the skill.
- Tools: lenient argument intake (common aliases like `file_path`,
  `old_string`, `cmd`; numeric strings; null/unknown keys dropped);
  `bash` no longer blocks until timeout on backgrounded servers (pipes are
  released 1.5s after the shell exits, with a note), and `pkill -f` can no
  longer match its own shell. `pocket kit SUB --help` prints usage.
- rm guard: heredoc bodies are data, not commands; `${VAR}` stays one word;
  `rm -rf "$D"` on a `D=$(mktemp -d)` scratch dir is allowed.
- TUI: the footer names the live phase (thinking, writing, bash, read,
  awaiting approval), shows elapsed time as m:ss and flags silent stretches
  ("quiet 20s"); each thinking span gets its header; failed tools render
  red and error/retry notices yellow; turns end with "✓ done in Ns" and
  errors in red; huge pasted input is folded in the transcript echo.

- Harden Double mode while keeping its shape (two concurrent same-model
  first passes, one reconciliation, one unified plan, normal parent loop).
  First passes can now gather evidence with read-only tools (`read` plus a
  strictly allowlisted single-command `bash`: no pipes, redirection,
  composition, or writes); every stream gets at most one retry on transient
  provider failures; each phase runs under a real deadline that cancels
  slow streams instead of joining forever. Reconciliation uses bounded
  derived thinking (never `off`) and sees per-analysis route marks:
  provider-reported model ids are verified against the requested model,
  with silent providers labeled unverified and substituted routes marked
  degraded. A failed reconciliation no longer crowns the longer analysis —
  both bounded views seed the parent with an instruction to compare and
  commit — and lone survivors are labeled uncorroborated. `double passes`
  is now precisely defined and persisted across resume; `/double` status
  clarifies that `2×` means twin first-pass mode. 20 new tests cover
  concurrency, leakage, routes, read-only enforcement, retry, deadline,
  cancellation, degraded paths, accounting, and resume.

## 0.6.0

- Add Double mode (`/double`, `/double on|off|status`): each direct turn
  opens with two concurrent independent first-pass analyses by the current
  model — same provider and configuration, same session context — reconciled
  by one bounded call into a single unified plan that seeds the normal
  single-stream loop. First passes propose tool actions in text but never
  execute; every state change still happens exactly once. A failed stream
  degrades to its survivor, or to a plain single turn when both fail.
  Token, request, cost, and cache accounting cover every stream; `/session`
  counts double passes. Goals stay single-stream. The toggle persists per
  session; the prompt shows `·2×` while on.
- Show a live spinner with elapsed time in the TUI status line while a turn
  runs, so silent models and long tools never look frozen. Honors
  `POCKET_NO_ANIM` with a static bullet.
- Clear a stale held-queue flag when a turn starts with nothing queued, so a
  cancelled turn no longer mislabels the next goal turn's footer "paused".
- Fix `fmtK` printing "1000.0k" at 999,950+ tokens; it now rolls to "1.0M".

## 0.5.5

- The recursive-rm guard now judges each `rm` invocation by its own flags
  and targets. Previously any `rm` plus any `-r` anywhere in a command line
  (e.g. `grep -r ...; rm -f x`) made every absolute path in the line, URL
  fragments included, look like an rm target and blocked the whole command.
  `rm -rf "$TMPDIR/name"` scratch cleanup is allowed.
- `edit` accepts mixed forms: a top-level `old_text`/`new_text` beside an
  `edits` array (even an empty one) is applied as one more step instead of
  failing the call.
- Writes to `/tmp/...` outside allowed roots now say to use `$TMPDIR/...`.

## 0.5.4

- Goal audits no longer kill autonomous goals. An auditor that hits its
  output limit is retried once with a larger budget, a failing fast auditor
  falls back to the main model, and an unavailable or unclear audit resumes
  work (bounded by the cycle budget) instead of pausing with
  "goal audit failed" or "no valid DONE/CONTINUE verdict".
- Parse audit verdicts tolerant of markdown and labels (`**Verdict:** DONE`)
  without letting prose such as "not done" certify completion.
- Goal round-limit checkpoints no longer log the same stop line twice.

## 0.5.3

- Add deterministic local HTML frame and MP4 export through `kit frame/video`.
  Reuse installed Chrome/Chromium and FFmpeg over private pipes, with bounded
  frames/deadlines, unique profiles, cancellation, JavaScript error reporting,
  output validation and atomic publication. No browser SDK or linked media
  dependencies. Optional audio is padded/trimmed to the video duration.
- Add seeded native `kit sfx` presets with oscillator sweeps, noise, envelopes
  and filtering. Validate all audio options and bound synthesis memory/work.
  Repair malformed WAV handling, low-rate loops, truncated chunks, pitch
  bounds and incorrect antiphase stereo measurements; support common PCM/float.
- Keep JSON, CSS and numeric tool arguments independent of desktop decimal
  separators while preserving Unicode terminal behavior.
- Repair false-success image/screenshot/scan commands, concurrent browser profile
  collisions, and unstable spring integration. Missing inputs, bad numeric
  options, absent output and malformed image headers now fail explicitly.
- Replace imaginary tools and missing scripts in media guides with runnable
  native workflows. Ship deterministic motion/audio examples and standalone
  C++ coding/ML examples with differential and held-out acceptance checks.
- Apply Jev paper guidance to batched judgments: separate untrusted evidence
  from criteria, preserve structured context on fallback, and abstain on quality
  judgments lacking sufficient evidence. Unknown results retain normal review;
  local classifiers cannot certify evidence or completion. Include an offline
  calibration recipe; fixed policy cutoffs are not claimed to be calibrated.

## 0.5.2

- Deliver queued follow-ups after the current tool batch, including during long
  autonomous turns. Preserve goal progress when yielding to new input.
- Treat the round limit as a progress checkpoint for autonomous prompts and
  goals. Continue useful work within a bounded checkpoint budget; pause repeated
  or unproductive work with a saved reason. Goal completion always requires the
  evidence auditor, not a small classifier's verdict.
- Compact growing context at a configurable 96,000-token working target, retain
  original and latest user requirements, preserve complete reasoning/tool pairs,
  and back off after failed summaries. Keep DeepSeek's required reasoning replay.
- Add a bounded, cached local LM advisory during long tool runs. It uses an
  already running server, never starts one or falls back to a paid model, and
  cannot approve completion or block useful work.
- Show the full session ID; add `/sessions` and `/resume [ID|last]` inside the
  TUI. Restore history, model, roles, cost and paused goal under an exclusive
  session lease. Reject switches with pending input, images or queued work.
- Flush outstanding child usage to its owning session before switching or
  exiting. Preflight validation cannot consume another session's receipts.
- Publish goal completion only after its checkpoint is saved, and update busy
  TUI goal status from worker snapshots so `/goal status` stays consistent.
- Persist timestamped completion, cancellation and failure outcomes so resumed
  sessions and `/session` explain why work stopped. Keep diagnostics out of model
  replay. Repair footer/transcript overlap on narrow terminals.
- Preserve access to an installed Node runtime selected by PATH without exposing
  its containing home directory. Make native read/write/edit and bash share the
  private `$TMPDIR` scratch path, with traversal and symlink checks.

## 0.5.1

- Keep the composer live while the agent works. Show queued messages, process
  them in order, and hold pending work after Esc until explicit follow-up/resume.
- Preserve picker navigation typed after queued `/thinking`, `/model`, and
  `/models` commands, including across response completion and approval prompts.
- Add goal pause/resume/clear/status with persisted progress and safe paused
  restoration after restart. Cancellation no longer deletes the goal.
- Show Jev, Span, and local LM activity, including real outcomes, cancellation,
  fallback, and cached-answer reuse, in the TUI transcript.

## 0.5.0

- Preserve follow-up input while a response streams; fix input-reader handoff,
  dangling callbacks after turns, multiline paste, slash-command boundaries,
  approval/picker input, Unicode cursor movement, and narrow/resized terminals.
- Show full provider, model and thinking level in the footer. Aggregate metered
  model, retry, judge, skill-selection, review, summary and recursive-child costs.
  Exclude Codex subscription usage; mark estimates and missing billing data.
- `/models` selects main, fast, fallback, review council and subagent defaults.
  Validate selections before applying them and preserve session-specific roles
  across resume. Recursive children inherit the selected model and settings
  without implicitly receiving credentials.
- Serialize session append/repair, reject nonregular state files, restore the
  actual latest idle session, and keep workspace notices separate from private
  transcripts. Same-workdir sessions exchange bounded status/change notices;
  native writes/edits/undo use cancellable workspace locks.
- Refuse stale undo after another writer changes a file; contain deletion,
  preserve private file permissions, bound undo memory, handle empty reads and
  quote hook substitutions without recursive expansion.
  Project configuration cannot install shell hooks that bypass command approval.
- Bound and cancel local/Jev/Span calls, reuse local prompt/cache results,
  serialize access to the local model slot, and fill missing decisions locally.
  Tiny local transcript guesses cannot certify a goal or skip council review.
  Merge provider learning safely across processes.
- Keep main context accounting separate from side calls, repair compaction turn
  boundaries, meter failed attempts, and validate tool IDs before side effects.
  Harden provider header parsing, malformed responses, and Codex token decoding.
- Add real PTY, localhost transport, recursive CLI, and multiprocess regressions.
  Installation replaces the executable atomically so existing sessions survive.
- Give recursive processes a bounded termination grace period to cancel their
  own tool groups and report usage; deeper children receive shorter deadlines.
- Stream the running executable into child sandboxes without a 64 MiB limit
  or whole-binary allocation; report staging failures before starting a turn.

## 0.4.0

- Overseer: expert brief before substantial work, Span-read stop gate
  (permission asks, announced-but-undone work, dropped requirements,
  premature completion), verification demands, `stop` hooks, a review
  council with Span prefilter and majority vote, a watchmaker for repeated
  failures, and `/goal` (`pocket -g`) with Span/LLM audits. All bounded.
- Judges: batched OpenRouter decisions (respan/span-01 for transcripts,
  typesafe Jev for content), an on-demand local llama.cpp Qwen judge, and a
  native naive-Bayes fallback. Jev also distills oversized tool output and
  flags template-grade UI/copy after edits.
- Native intelligence: fuzzy + BM25 ranking, learned provider quirks from
  400s, EWMA provider health, adaptive compaction threshold, duplicate-result
  references.
- `pocket kit`: web, search, headless-Chrome DOM/screenshot, img, svg,
  spring, wav, audio, slop, code index (find/sym/refs), probe. The binary is
  staged on the sandbox PATH, which also makes recursive `pocket` work under
  confinement.
- 30+ built-in providers, ChatGPT Codex (Responses API) login, a daily
  self-refreshing model catalog, `/models` role assignment (main, fast,
  fallback, review) with fuzzy search, fallback on provider outages, and
  total cost including judges.
- Hooks (`post_edit`, `pre_bash`, `stop`), `/undo`, `/brain`, `/catalog`,
  provider keys from `~/.config/pocketharness/env`, an awareness block
  (host, git, verify command, guardrails, project memory) and the wisdom
  doctrine in the frozen prompt.
- 165 bundled skills (installed by `make install`), frontmatter-aware and
  BM25-searched; non-C/C++ code assets removed.
- TUI: animated Game Boy-in-a-pocket banner, cost and goal in the status bar,
  cleaner glyphs, UTF-8-safe truncation.

## 0.3.1

- Serialize only valid UTF-8 JSON: preserve valid text and replace invalid bytes
  from binary output, legacy encodings, or truncated characters with U+FFFD.
  Applies to both providers and session writes; old sessions with raw invalid
  bytes can resume without repeatedly sending malformed requests.
- Reject unpaired low-surrogate JSON escapes. Add Unicode boundary, legacy
  session recovery, and real curl regression tests for both wire protocols.

## 0.3.0

- Keep capped tool-result prefixes stable as conversations grow; retain useful
  output tails and full results on disk. Correct cache denominators and account
  for context additions and compaction usage.
- Add OpenAI, Ollama, LM Studio, and llama.cpp presets, per-model output budgets,
  token parameter selection, usage-stream compatibility, adaptive thinking,
  and auto/none/minimal/xhigh effort levels. Preserve provider reasoning through
  tool calls and resume. Enable Anthropic automatic prompt caching.
- Validate complete tool batches and stream endings before execution; handle
  multiline SSE, JSON fallbacks, bounded responses and numeric Retry-After.
- Lock sessions against concurrent writers, scope resume to the workspace,
  recover interrupted batches without replaying side effects, repair torn tails,
  and preserve retained messages across repeated compaction/resume cycles.
- Stream bounded file ranges, validate tool argument types, and support atomic
  multi-replacement edits. Reject oversized edits rather than overwriting a
  file from a truncated read.
- Use pipefail and clean bash startup, reject NUL commands and fake loopback
  hosts, guard computed recursive deletes and force-pushes, isolate provider
  environments, and disable ambient curl configuration and redirects.
- Bound subprocess output callbacks, avoid EOF polling spins, handle closed
  stdin without SIGPIPE termination, and enforce timeouts during output floods.
  Noninteractive SIGINT/SIGTERM cancels the process group and saves the session.
- Encourage autonomous implementation and verification by default; stop
  repeated identical tool batches. Keep five tools, C++20, and no new runtime
  dependencies. Add GCC/Clang and sanitizer CI plus local HTTP integration tests.

Existing sessions remain readable. New compaction checkpoints and reasoning
continuation records require 0.3.0 for faithful replay. Provider/model support
is protocol-tested with fixtures; live cache savings and model behavior vary.
