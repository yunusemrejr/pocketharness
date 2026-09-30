# Changelog

## 0.12.0

Adaptive native execution, reusable project workflows, and media reliability.

- Native task policy scales planning, reasoning, configured fast/main models,
  reviewers and local observation with complexity, risk and observed failures.
  Explicit model/thinking choices stay authoritative; growing or failing work
  escalates and steady progress skips redundant coordination.
- Successful stop hooks are reused only for an unchanged work revision. Edits
  invalidate lint/review state, so reviewer-requested fixes receive fresh checks.
- Deterministic skill routing selects installed stack/media/hosting guides,
  avoids repeated loaded-skill hints and uses Jev for ambiguous substantial work.
  Reusable recipes cover native/web/scripting/ML discovery through delivery;
  UI guidance preserves project identity and targets concrete generic patterns.
- Native music synthesis avoids invariant per-sample work. Mixing preserves a
  full music bed when short cues are present, validates WAV layouts and timeline
  bounds, downmixes surround and filters high-rate inputs before downsampling.
- Native mix editing supports input trims, voice gain and fade-in. Browser
  capture freezes CSS/WAAPI animations before delayed asset readiness, then
  seeks each frame deterministically; short animations no longer disappear.
- Preserve continuous TUI activity, recover the footer after tiny-terminal
  resizing, and honor cancellation before final judgments and reviewer starts.
- Tag releases run compiler/sanitizer checks before publishing a Linux binary,
  bundled skills and checksum. Source and binary installs use one staged path
  that preserves user configuration, sessions and personal skills.

Measured fixture and native media results, limitations and reproduction commands
are recorded in `docs/benchmarks-0.12.0.md`.

## 0.11.4

Continuous TUI liveness and cancellation fixes.

- The animated activity row has priority over wrapped metadata, so the spinner
  stays visible in narrow or short terminals and while the model is silent.
  Quiet ticks repaint only that row without rewrapping drafts or changing the
  transcript's saved cursor. `NO_COLOR` still permits animation;
  `POCKET_NO_ANIM=1` retains the static indicator and elapsed time.
- Growing a terminal back from fewer than six rows restores the pinned footer.
  Previously, one tiny resize disabled the footer for the rest of the session.
- An active goal no longer displays "paused" just because older follow-ups are
  held. The queue remains held until explicitly released.
- Escape at the final reply or during the Jev review prefilter stops new council
  requests. Reviewer workers check cancellation before starting; interrupted
  councils retain incurred costs without publishing a review verdict.

Four regression tests cover real PTY animation frames, resizing, retained
drafts and queues, and cancellation at the main-response and review boundaries.

## 0.11.3

Composer and footer redraw cost, on the path taken for every keystroke and
every streamed token.

- The wrap loop took a fast path for printable ASCII, skipping the decode, the
  width lookup and — the expensive part — a `substr` that allocated a fresh
  `std::string` for **every glyph**. A 200 000-character draft re-wrapped on
  each keystroke went from 9.7 ms to 3.7 ms, with byte-identical output. Tabs,
  control bytes and multi-byte or wide characters still take the general path.
- The footer built its rows once to size the footer and again to draw it, so
  every redraw wrapped the whole status block twice. It is now built once and
  passed to the draw.
- Live process-group tracking grew from 256 to 1024 slots, so a long goal that
  leaves many dev servers running still has every group reaped on exit.
- Reading an image over the 5 MiB cap now reports its real size and the limit;
  it used to say "over 5 MiB or unreadable" for both a too-big and a broken file.

Tests: 345 -> 347. The layout test pins the exact-width wrap, tab stops, control
bytes and a wide glyph at a wrap boundary; dropping the fast path's trailing
wrap makes it fail, so it guards the equivalence rather than just passing.

## 0.11.2

Thread-ownership fix for the TUI footer, and judge registration hardening.

- The footer no longer reads the live agent. It redraws on every streamed token
  and built its text by walking `Agent`'s messages on the editor thread while
  the turn worker appended to them; that is a `std::vector` reallocation under
  a concurrent read, which aborts the process in `std::get` of a half-moved
  `json::Value`. The turn thread already publishes the same text through its
  Status event, so the footer renders that snapshot instead, and `/model`,
  `/thinking` and role assignment republish it. No visible change: the footer
  still tracks the running turn at the same 250 ms cadence.
- Local-judge registration now serialises on its own mutex. It mutates the
  shared users-file bookkeeping, and the fatal-signal path reads it
  concurrently, which is undefined behaviour; two judges registering at once
  could also overwrite the tracked descriptor and leak the other.

Tests: 343 -> 345.

## 0.11.1

TUI robustness and interruptibility, plus catalog and transport cleanups.

Session flow:

- The TUI now restores the terminal on *every* exit path, including an
  exception. A throw used to unwind past the teardown tail and leave the
  scroll region and bracketed paste set, and a joinable watcher thread
  unwound straight into `std::terminate`. Both now run through one guard.
- The status/KPI snapshot is taken only on the thread running the turn, which
  owns `messages_`, the stats and the goal string. `/double` runs tools on two
  evidence workers and the reconcile pass streams on a third, and those reach
  the same callbacks: snapshotting from them raced the turn thread over
  `std::string` and `std::variant` members, which can abort the process.
- `/models <role> <specs>` no longer freezes. Resolving an unknown model can
  block on a live `GET /models` probe and the command had no input gate at
  all, so the TUI looked dead for up to 5s per model with no way out.
- `/catalog` and the model probe honour Ctrl-C. Both take the gate's cancel
  flag, so an interrupt aborts the in-flight request instead of waiting out the
  12s per-provider timeout. An interrupted refresh leaves the cache unchanged
  rather than rewriting it from a partial answer.

Efficiency:

- Assigning a role reads and parses the model catalog once instead of once per
  selected model (plus once more for the picker title). A three-model review
  council did four full parses before the first prompt.
- `spawn` claims a process-group slot without syscalls. Pruning dead groups
  costs one `kill(2)` per occupied slot and ran on every spawn, putting ~512
  syscalls between each tool launch and its first poll; it now only runs when
  the table is actually full.
- The Codex backend read and parsed `~/.codex/auth.json` and decoded a JWT
  twice per request, once inside `providerApiKey` and again for the account id.

Correctness:

- `read` with an offset past the end of a file now fails immediately with the
  file size. A line number can never exceed the byte count, so the request was
  already unsatisfiable, but it scanned the whole 64 MiB budget first just to
  report "offset beyond EOF".

Tests: 341 -> 343.

## 0.11.0

MiniMax Token Plan support, plus goal-flow and session-flow fixes.

- New built-in `minimax-plan` provider: the OpenAI-compatible Token Plan
  endpoint (`https://api.minimax.io/v1`, key in `MINIMAX_TOKEN_PLAN_API_KEY`).
  Subscription keys (`sk-cp-...`) are a separate credential from pay-as-you-go
  `MINIMAX_API_KEY`, so the existing `minimax` Anthropic provider is unchanged.
  Use `minimax-plan:MiniMax-M3` (1M context) directly, or add aliases for the
  plan's M-series models with `token_parameter: max_completion_tokens` and
  `reasoning: none` (MiniMax Chat Completions has no `reasoning_effort` field).

Goal completion:

- The deliverable gate no longer forges artifacts. A format only counts as a
  requested output when it is in an output position — trailing ("as mp4"),
  after an output preposition ("to svg"), or heading an artifact noun ("a pdf
  report") — and goals that produce source ("write a script that converts webm
  to mp4", "add an mp4 export button") are exempt. Previously those goals were
  pushed into creating files they never asked for, littering the workspace.
- The gate is evaluated once per goal instead of on every audit cycle, which
  repeated a full recursive workspace walk (up to 20k entries) for nothing.

Session flow:

- Line mode (piped or redirected I/O) handles Ctrl-C. The first interrupt asks
  for a graceful cancel so the turn unwinds, usage is flushed and tool children
  are reaped; a second restores the default action to escape a wedged turn.
  Previously the default action killed the process mid-turn.
- Keystrokes typed while a tool-approval prompt is up are no longer discarded.
  Typeahead is re-queued so a buffered "yes do X" stays a follow-up and a
  message started mid-prompt survives.

Correctness:

- `${TMPDIR}/x`, `$TMPDIR/x` and a bare `$TMPDIR` all expand to the session
  scratch path. Previously only the bare form matched, so the others resolved
  inside the workspace and created a literal `$TMPDIR` directory in the repo.
- `ensureDir` separates every path component. A one-character prefix was never
  followed by a separator, so `a/b` created `a` and then `ab`.
- Serialising a JSON number no longer casts out-of-range doubles to `long long`
  (undefined behaviour, reachable from any large number in a provider reply).
- `spawn` skips a failed `poll` before reading `revents`, which was undefined
  on any non-`EINTR` error.
- Overwriting a file larger than 4 MiB now says so. The undo pre-image is not
  captured at that size, and the result previously implied `/undo` could
  restore the original.
- A signal during the session-listing preview read no longer makes a session
  vanish from `--sessions`.

## 0.10.0

Automatic quality gates: security, backend, performance, DRY, UI slop and SVG.

- New `kit lint [PATH...]`: one table-driven native rule engine (`src/kit_lint.cpp`)
  for JS/TS, Python, C/C++, shell, Go, PHP, Java, SQL, HTML/CSS/JSX/Vue and SVG.
  Security (hard-coded secrets and token shapes, SQL built from strings, shell
  injection, eval, weak hashes, disabled TLS verification, unsafe C string calls),
  backend (HTTP calls without timeout, error objects sent to clients, wildcard
  CORS, JWT without expiry, cookies without httpOnly), efficiency (queries and
  fetches inside loops, SELECT *, sync I/O in servers, transition: all), coding
  patterns (mutable defaults, bare except, empty catch, suppressed checks, deep
  nesting), DRY (duplicated blocks within and across files), UI/accessibility
  (WCAG contrast per CSS rule, missing alt/lang/viewport, removed focus ring,
  text under 12px, animation without reduced-motion) and slop (indigo/purple
  gradients, Tailwind indigo demo accents, cream + terracotta, Inter/Space
  Grotesk/Geist/Instrument Serif, glow halos, emoji icons, invented stats,
  buzzword copy) plus SVG hygiene (scripts, external refs, embedded rasters,
  editor metadata, wasted coordinate precision, filters, text, no accessible name,
  unused ids). Findings are aggregated per rule with line numbers.
- It runs without being asked: every `write`/`edit` returns its findings (medium and
  up on first touch, high on later edits), and before a turn ends the changed files
  get one whole-change sweep (high findings plus cross-file duplication) that sends
  the agent back to fix them. Test files, vendored and generated paths are exempt
  from secret and duplication rules.
- Bare `pocket kit SUB` with a required argument prints its usage and exits 0 instead
  of reading as a failed tool call.
- Bash timeouts now say how to run long jobs (background + poll, or a larger timeout);
  an oversized image read says how to capture a smaller one.
- Generated media (`wav`, `sfx`, `music`, `mix`, `say`) creates missing output
  directories; a temp-file failure now names the OS error.
- DRY in the harness itself: one seccomp loader for both sandbox profiles, one shared
  tail for the twin first-pass prompts. The awareness block's superpower list is shorter.

## 0.9.0

Motion video, rebuilt around what a publishable video needs.

- New `video-studio` skill and request gate: a video request gets the same
  Jev-judged demand as UI work to load the doctrine (publish vs private mode,
  hook and retention structure, art direction against the default AI looks,
  branding rules, audio mix, QA gate), and the first `kit video` render is
  stopped once until it was read.
- `kit say`: neural narration through Piper (`--setup` downloads it once), one
  model load per script, sentence-level timing measured from the audio, word
  windows estimated from syllables, `{TTS|tee tee ess}` pronunciation overrides.
  Writes `.json`, `.srt`, a ready `.captions.html` and `.cues.css` (`--b1-s`,
  `--u3-s`, ...) so scenes are timed to the voice. Runs on at most four cores at
  low priority. The sandbox exposes the voices directory read-only; installing
  stays a user action.
- `kit music`: generative stereo score, six styles, seeded and deterministic.
  Voice-led chord loops, bass, arpeggios, motif, drums, sidechain pump, risers,
  a breakdown near 60%, Freeverb reverb, ping-pong delay and a bus compressor.
- `kit mix`: cue-sheet mixer with speech-aware ducking, effect cues, BS.1770-4
  integrated loudness (checked against FFmpeg's meter), look-ahead peak limiter.
- `kit asset search|get|font`: Poly Haven CC0 models/HDRIs/textures, Openverse
  images/audio with licence and credit lines, Google Fonts as local woff2, and a
  pinned three.js with loaders. `ATTRIBUTION.txt` is written automatically.
- `kit theme`: palette and font pair derived from the subject; accent arcs exclude
  indigo-to-magenta and the terracotta band, the neutral is tinted, contrast is
  enforced.
- `kit vsheet`: the whole video as one timestamped contact sheet, so a vision
  model reviews it in one image read.
- `kit frame` / `kit video`: a scene lint (safe margin, frame-edge and overflow
  clipping, overlap, legibility floor, contrast, blank frames, failed images and
  fonts, monospace display type, default fonts, purple gradients); pure CSS/SVG
  scenes need no `renderFrame`; BT.709-tagged encoding; software WebGL and
  file:// modules enabled for three.js scenes; limits raised to 900 s, 54,000
  frames and a 4 h deadline.
- Fixed: `document.getAnimations()` forgets finished animations, so a scene
  sought out of order (lint sampling, `--start`) could never be sought back.
  The renderer now keeps every animation it has seen.
- `kit vcheck` reports integrated loudness and flags programmes that are too
  quiet or too loud.
- JSON numbers print in the shortest form that round-trips (`3.854`, not
  `3.8540000000000001`).

## 0.8.3

- Goals are checked against the deliverable, not only the transcript. When a
  goal asks to save, export or render a named format (mp4, pdf, png, docx and
  so on), the harness first looks for a structurally valid file of that format
  (magic bytes, not just the extension). A missing or empty file sends the
  agent back to work before the audit can certify anything. It fires once per
  goal run, so a legitimate exception cannot loop.
- New `goal_done` hook (user config only, like every hook): commands that must
  pass before a goal is certified. A failure returns to work with the output.
- Stop hooks are re-run after each fix, up to three times per turn. Before, a
  hook that failed once was never checked again, so a still-broken build could
  end the turn.
- `kit video`: a stalled frame capture is retried once, and a second failure
  names the frame. `--start SECONDS` renders a piece of a longer timeline (audio
  cut to match) so long videos can be rendered in parts. The time budget scales
  with frame count instead of a flat 120 s, and `--timeout` accepts up to
  3600 s (600 before, which silently rejected larger values).
- New `kit vcheck FILE.mp4 [--duration S]`: finished-video QA from the file
  itself (streams, duration, audio/video length mismatch, black spans, frozen
  spans, silence, clipping). It found the faults that agents had been
  hand-writing ffprobe scripts for. It says plainly that it does not judge
  content.
- A failed `edit` now shows what to do next: the lines where an ambiguous
  `old_text` occurs, or the closest current text with line numbers, so the
  model no longer needs another read round (10 such failures in one day of
  logs).
- Node inside the sandbox: a `Cannot read package config ...: permission
  denied` error (node walking up to a hidden ancestor package.json) now says to
  add a package.json in the project root.

## 0.8.2

- UI design doctrine: the bundled `ai-design-slop` skill (indigo/purple and
  cream/terracotta slop generations, full tell checklist, nine-step procedure)
  is required reading for interface work. Jev decides in the same batched call
  as the skill hint whether a request is UI/UX/GUI work (no extra request; a
  keyword fallback offline) and tells the agent to load it. Backstop: the
  first write of an .html/.css/.jsx/.tsx/.vue/.svelte/.astro/.scss file
  before the doctrine was read gets the same demand.
- Disk guard: a runaway tool command can no longer fill the disk. An
  agent-written ffmpeg filter ended in a bare `apad`, which pads forever. It
  wrote 74 GiB of silence into one WAV in about 7 minutes, well inside the
  15 minute bash timeout, and left the disk 100% full. Now bash commands are
  stopped when they consume more than `disk_budget_gb` (default 40), or keep
  writing below `disk_reserve_gb` free (default 10). Any single file is
  capped at `max_file_gb` (default 32, `RLIMIT_FSIZE`), which also binds
  background children. A session watchdog stops backgrounded tool processes
  when free space is below the reserve and still falling. The model is told
  why its command stopped and to bound the writer instead of rerunning it.
  Project config cannot loosen these limits.

## 0.8.1

- Speed: the review council no longer waits on a stalled reviewer. After
  the first verdict, the rest get a grace window of 20-45s. After that
  they are cancelled, and the notice names every reviewer that gave no
  verdict and why ("no verdict: openrouter:z-ai/glm-5.3-flash timed
  out"). A simple fix-and-verify turn went from 210s to about 23s.
- Speed: the brief is advisory and now has a 20s deadline. A slow fast
  model no longer delays the work, and a late brief is never injected.
  Small, precise tasks get a short brief (intent plus 1-3 acceptance
  lines) instead of invented scope.
- OpenRouter `thinking=off` is now actually sent. Before, no knob was
  sent, so reasoning models ran at their default effort, which is `max`
  for mandatory reasoners such as GLM-5.3-flash. Those side calls often
  spent the whole budget on reasoning and returned nothing. The catalog
  now records whether reasoning is mandatory and its lowest effort. Off
  sends `{"enabled":false}`, or that lowest effort for mandatory
  reasoners. When a stale catalog triggers a "reasoning is mandatory"
  400, the request retries at `low` instead of learning a harmful
  `no_reasoning` quirk.
- Compaction rejects summaries that contain tool-call markup. It uses a
  framed `<work_log>` prompt and retries once with a stricter ask. After
  that it falls back to a native work digest, so the context is never
  replaced by `<tool_call>` garbage.
- Sandbox: Python from uv, pyenv, mise and asdf is found through PATH,
  and so are workspace `.venv/bin/python` symlinks into those managers.
  They get read-only runtime roots, so `uv venv` interpreters no longer
  fail with "Permission denied". This follows the existing Node manager
  support.
- `-p` stdout starts a new paragraph between rounds, so narration before
  a tool call no longer runs into the final answer.
- The awareness block says when `.pocket/memory.md` does not exist yet,
  so the first append no longer fails on the missing directory.
- `.pocket-test-*` directories left by interrupted test runs are
  gitignored.

## 0.8.0

- A malformed tool call no longer stops the turn or the goal. Arguments
  are repaired when that is safe: code fences, a double-encoded string,
  raw control characters inside strings, and trailing junk are handled.
  A call that cannot be trusted, such as cut-off JSON or several objects
  glued together, gets a failed tool result that explains how to re-issue
  it. The model sees that result and retries; before this, the whole
  response was rejected with "invalid tool arguments".
- The OpenAI stream merge no longer concatenates ids or names that a
  provider repeats in every chunk. A new id at an index that is already
  used starts a new call. Nameless calls are dropped, missing or duplicate
  ids are regenerated, and a response is capped at 64 calls.
- `edit` falls back to a unique block match that ignores whitespace when
  `old_text` differs only in indentation or trailing spaces, and
  re-indents the replacement to match. When no match is found, the hint
  now names the line where the first line matched but later lines
  differed, or says that the match was ambiguous.
- `bash` pipelines that end on exit code 141 (SIGPIPE, for example
  `... | head`) count as success and include a note.
- Adaptive thinking (`-t adaptive`) is the new default. Effort is chosen
  per request:
  - `high` for a new user message, after compaction, or after a failed
    tool call
  - `medium` after changes
  - `low` for read-only steps
  Anthropic always uses `high`.
- Efficiency:
  - The Jev visual judge runs only on the first change to a file in each
    turn.
  - Goal continuation resends the goal brief only after it has left the
    context.
- `kit wav` is now a polyphonic mixer:
  - `+` joins notes into chords.
  - `|` separates up to 16 tracks, and each track can set its own wave
    with `saw>`.
  - `K`, `S` and `H` are noise drum voices.
  - The mix is peak-normalized, so the output never clips.
- New kit tools:
  - `ports`: listening sockets with their pid and command
  - `wait PORT|URL`: use instead of `sleep`
  - `net URL`: timings, redirects, and missing security headers
  - `seo FILE|URL`: on-page SEO audit
  - `csv FILE`: dataset profile
  - `bench`: repeated timing with CPU and peak RSS
  The relevant skills now point to these tools.

## 0.7.2

- DeepSeek thinking mode no longer stops sessions and goals with
  `HTTP 400: The reasoning_content in the thinking mode must be passed back`.
  Assistant turns without DeepSeek reasoning (answered by the fallback
  model, or sent with thinking off) are now replayed with an empty
  `reasoning_content`, which the API accepts (verified live against the
  failing session history).
- That error is no longer mistaken for "model does not support reasoning":
  it retries the request once without thinking and learns nothing. A
  `no_reasoning` quirk previously learned for DeepSeek is ignored, and
  reasoning `none` on DeepSeek now sends an explicit `thinking: disabled`
  (DeepSeek thinks by default, so omitting the field did not turn it off).
- A request the main model still rejects with 400/422 after quirk handling
  goes to the fallback model instead of ending the turn or pausing the goal.

## 0.7.1

- Session exit cleans up after itself. Quitting, closing the
  terminal (SIGHUP), SIGTERM and SIGQUIT now stop every process group the
  session started, including backgrounded servers left by `bash` calls
  (TERM, a short grace, then KILL), so nothing keeps eating RAM or CPU.
- The local llama.cpp judge is shared safely between sessions: each live
  session holds a shared lock on `judge-PORT.users`, and the server is
  stopped only by the last session to leave (verified against its pid file
  and `/proc/PID/cmdline`). A session no longer kills a local model another
  PocketHarness session is still using.
- Local LM: sessions register early and, when no remote judge is
  available, start the configured model in the background at startup, so
  the first judgement does not pay the cold start. A model that is still
  loading is reported as such instead of triggering a 60s cooldown.
- Long goals: round checkpoints are framed as automatic resumes, not a
  hard limit, and the "two rounds remain" push is skipped while a goal is
  active, so the model stops cutting scope to fit a budget.

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
