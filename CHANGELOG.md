# Changelog

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
