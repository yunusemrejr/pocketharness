<img src="mascot.png" alt="PocketHarness mascot: a pocket-sized toolbox robot" width="180" align="right">

# PocketHarness

A deliberately tiny, Linux-native coding-agent harness in a single C++ binary.
It feels like Pi when used interactively — a chat TUI, slash commands,
autonomous goals — but there is no framework underneath: no SDKs, no
runtimes, no plugins, no MCP, no orchestration layers, no dependency jungle,
and no other languages in the box. One competent engineer should be able to
understand essentially the entire architecture in an afternoon.

Conceptually it is one loop: the terminal feeds an agent, the agent calls a
model provider, the model optionally calls tools, and Linux executes them.
Everything else is a small native function hung on that loop — a lone
supervisor (the overseer) instead of stacked councils, cheap structured
judges instead of vibes, a live model catalog instead of hardcoded slugs,
frozen session prefixes instead of re-sent context, twin first passes
(`/double`) instead of single-shot plans, and kernel confinement instead of
prompt-shaped safety. General-purpose work is delegated to Linux itself
(`curl` for HTTPS, Chrome for screenshots, FFmpeg for video, llama.cpp for
the free local judge) rather than wrapped, vendored, or plugged in.

> PocketHarness intentionally delegates general-purpose functionality to Linux
> instead of accumulating wrappers, plugins and orchestration layers.

> PocketHarness assumes models can make mistakes. Critical security boundaries
> are enforced by the harness and Linux kernel rather than by prompting the
> model to behave safely.

**Pi-like behavior, BusyBox-like architecture. Linux is part of the harness.**

The runtime loop stays obvious:

```
terminal → agent loop → model provider → optional tool call → Linux/filesystem → result → agent loop
```

## Install

Requirements: `g++` (C++20), `make`, `curl`. No bundled third-party code.
Optional: Chrome/Chromium for `kit shot/dom/frame/video`, FFmpeg with H.264
encoding for `kit video`, and a llama.cpp `llama-server` + GGUF for the free local
judge. Native audio/SFX need none of these. Video export needs no Node/npm,
browser SDK or linked codec library.

```bash
git clone https://github.com/yunusemrejr/pocketharness
cd pocketharness
./install.sh        # builds, tests, installs to ~/.local/bin/pocket
```

Or manually: `make && make test && make install`.

Verify:

```bash
command -v pocket
pocket --version
pocket --help
```

## Adaptive execution and workflows

Small tasks skip planning overhead. With adaptive thinking and an implicit model
choice, safe simple work can use the configured fast role; edits, failures and
broader work return to main and raise reasoning/review proportionately. Explicit
model and thinking choices stay authoritative. Passed hooks are reused until the
work changes, and steady progress avoids redundant local observer calls.

```sh
pocket --workflow "Build a PHP 8 website and deploy via Git/SSH on Namecheap"
```

The offline JSON diagnostic shows native policy and available workflow guides.
Bundled recipes cover the native, scripting, web and ML stacks, checkpointed
validation/delivery, media production and shared hosting. Design guidance preserves
project identity and repairs specific generic patterns. See [execution and workflow
contracts](docs/execution-workflows.md) and [measured benchmarks](docs/benchmarks-0.12.0.md).

Tagged releases publish a checked Linux binary with bundled skills and checksums.
The archive installer preserves your configuration, sessions and personal skills.
Source installation remains available for your compiler/platform.

## Quick start

```bash
cd ~/projects/foo
pocket                  # interactive agent, workspace = current directory
pocket /some/project    # explicit workspace
pocket -p "Review src/network.cpp for concurrency problems."
pocket -m glm -p "Review src/network.cpp."
pocket -m ollama:qwen3:8b -t none --max-tokens 2048 -p "Explain this project."
pocket --resume         # newest idle session in this workspace
pocket --resume ID      # continue a specific saved session
pocket --sessions       # list workspaces, active/idle sessions, previews
pocket -g "make the settings page elegant and fix the save bug"   # autonomous goal
pocket --models flash   # fuzzy-search the live catalog (pocket --refresh-catalog)
pocket kit              # native superpowers (also available to the model)
```

Set provider keys as environment variables, or keep them in
`~/.config/pocketharness/env` (`KEY=value` lines, mode 0600, loaded at
startup, never overriding the shell). Never in files that get committed:

```bash
export DEEPSEEK_API_KEY=...
export OPENROUTER_API_KEY=...     # also enables the Span/Jev judges
```

Slash commands: `/models` (assign main · fast · fallback · review · subagent, fuzzy
search over the catalog) `/model` `/goal` `/queue` `/thinking` `/double` `/compact` `/undo`
`/skills` `/session` `/sessions` `/resume` `/brain` `/catalog` `/security` `/help` `/quit`. Keys: Enter submits, Ctrl-J/Alt-Enter newline,
Up/Down history, Esc pauses work, Ctrl-C cancels (idle empty prompt quits), Ctrl-D quits.
Paste is bracketed (multi-line paste never submits early); the pinned bottom
bar shows the input box, full provider/model/thinking, combined metered cost,
and context/tok-s/cache KPIs, with the full session ID above them. While a
turn runs, a reserved status row shows a live spinner with elapsed time,
even during silent thinking or tool calls and after terminal resizes. Quiet
animation ticks repaint only that row, keeping large drafts responsive.
`NO_COLOR` preserves motion; `POCKET_NO_ANIM=1` uses a static indicator with
elapsed time. The composer
stays live during generation: Enter queues a message visibly. Work yields after
the current tool batch so follow-ups take effect before another model request.
Esc stops the current work and holds the queue until a new follow-up or
explicit resume. `/queue` shows pending messages, `/queue clear` discards them,
and `/queue resume` releases the held queue. Local commands such as `/session`
leave a held queue paused. Up to 64 messages / 256 KiB can be queued; if full,
the unsent draft is retained. A queued `/thinking`, `/model`, or `/models` picker
keeps its following keystrokes until it opens; Esc returns control to the composer
and holds the queue.
Paste or drop an image file (PNG/JPEG/GIF/WebP, max 5 MiB) to attach it to
the next message — vision models read it inline (`--image PATH` does the
same for `-p`).

## Recursive agency (no orchestration framework)

`pocket -p` composes with Linux instead of a subagent framework:

```bash
pocket -p "Inspect auth" > /tmp/auth-review &
pocket -p "Inspect storage" > /tmp/storage-review &
wait
```

PocketHarness itself does not know what a "swarm" is. Child instances get
their own session, stay within the parent workspace, never inherit `--unsafe`,
never out-network an `--offline` parent, and stop nesting past depth 5.
Depth, workspace, and the net grant travel via a `$TMPDIR/pocket.parent`
file (never via environment, which the model can read). Provider keys are
never inherited implicitly: a child authenticates only through explicit
user-level `expose_env` passthrough. Kernel confinement is inherited and
cannot be shed — under `--offline` the whole subtree loses `AF_INET`.
Local diagnostics remain small native commands:

```sh
pocket kit reach localhost:8080 .5
pocket kit wait http://127.0.0.1:8080/health 5
pocket kit sys                  # pressure, memory and filesystem evidence
pocket kit sys 1234             # status and I/O; no environment or command line
pocket kit seo index.html       # source defects and separate recommendations
pocket kit asset inspect scene.glb
```

HTTP readiness requires transport success and 2xx/3xx; `--any-status` explicitly
checks only whether an HTTP response arrived. TCP diagnostics use bounded NSS
resolution and nonblocking connections, including bracketed IPv6. SEO does not
infer ranking, robots.txt policy or rendered metadata from source alone.
Asset inspection checks local glTF/GLB structure, dependencies, references and
accessor ranges before rendering. Embedded video is paused and sought for each
absolute scene time; required images must decode before export. See the bundled
Three.js starter for a local asset scene with explicit time-driven motion.

## The overseer

Weak and strong models get the same standards, enforced by the harness:

1. **Brief.** A substantial request ("fix", "build", "redesign", ...) first
   can go to the `fast` model as a planning council when complexity or risk warrants it: intent, the questions a
   domain expert would ask (identity, typography, architecture, data,
   security...) answered decisively from the project's evidence and the
   wisdom book, acceptance criteria, and the average outcome to avoid. The
   brief rides along with the request; the user's words win on conflict.
2. **Skill hints.** Native task/manifest routes select available skills; ambiguous substantial work uses BM25 and Jev in
   one batched call; the hint names them for loading. Native task evidence and Jev decisions identify UI/UX/GUI work and require the bundled
   `ai-design-slop` doctrine to be read first (writing a UI file without it
   triggers the same demand).
3. **Guardian on every change.** Writes/edits are scanned natively
   (placeholders, "rest unchanged" elisions, stubs, conflict markers,
   invalid JSON, AI-tell prose). New findings return immediately without
   repeating unchanged warnings. Targeted Jev batches inspect design identity,
   factual claims, prose precision and substantial/risky code. A material
   revision can receive one fresh batch; detected credential material stays in
   native checks. These are advisory signals, requiring concrete verification.
   No-op writes/edits skip undo, quality hooks and review invalidation.
   `post_edit` hooks run after actual changes; `post_bash` receives the command
   and bounded outcome. `pocket kit quality FILE --semantic --intent TEXT`
   uses the same content checks for explicit inspection.
4. **Watchmaker.** The same call failing three times gets a "change
   approach" note; long turns get a convergence check; identical batches stop.
   Model quirks are absorbed instead of ending the turn: a reasoning-only
   (empty) reply is retried without thinking; a reply cut off at the output
   cap doubles the budget (up to a quarter of the window) and asks for the
   work in smaller pieces.
5. **Stop gate.** When the model answers without tools, Span reads the turn:
   asking permission, announcing work without doing it, dropped
   requirements, premature completion → a short `[overseer]` nudge and the
   turn continues. Files changed but never verified → a verification demand.
   Failing `stop` hooks → their output. Changed work → the **review
   council**: Span prefilters (clean work skips the paid review), then a proportionate subset of the configured
   `review` roster answers LGTM or defects; a majority of objections goes
   back to the worker once.
6. **Goals.** `/goal TEXT` (or `pocket -g`) runs turns until an audit says
   the goal is met: the `fast` model audits the evidence digest and lists
   what remains. A small decision model cannot certify completion. Stricter limits,
   review always on. `/goal` or `/goal status` shows the saved goal;
   `/goal pause`, `/goal resume`, and `/goal clear` control it. Esc pauses without
   deleting progress. A follow-up to a paused goal resumes it with that input.
   Goals survive session restart: `pocket --resume` restores them paused until
   you submit a follow-up or use `/goal resume`.

Judge activity appears in the transcript while work runs: Jev, Span, and the
local LM report requests and outcomes. Cached answers are explicitly marked
as reused; an unavailable or cancelled judge is not reported as successful.
During long tool runs, a cached local progress check can suggest changing a
failing approach. It uses only an already running local server, has a 1.5-second
budget, and never starts a server or falls back to a paid model.

Every check is bounded (a few nudges per turn); every remote judge
refines and never gates; offline, the native classifier and heuristics
still run. `/brain` shows nudges, reviews, fallbacks, distillations and
side cost; `review: false` / `autonomy: false` switch the overseer off.

## Roles, fallback, cost

`/models` assigns five roles, saved in `~/.config/pocketharness/roles.json`
as defaults and frozen in each session for resume:
`main` does the work; `fast` writes briefs, summaries and goal audits;
`fallback` takes a request when `main` stays down after its own retries
(5xx/429/transport — never on 4xx); `review` is the council (comma-separate
several models for a majority vote); `subagent` supplies the default model for
recursive `pocket -p` processes. An explicit child `-m` overrides that default.
Use the picker, or `/models subagent provider:model`, `/models review model1,model2`,
and `/models fallback -` to clear an optional role.

Total cost includes main/fallback attempts (including billed failures), judges,
skill selection, reviews, briefs, summaries and recursive children without
double counting. Codex subscription usage is excluded; local calls are free.
Catalog estimates are marked `~`; unavailable billing data is marked
`+ unreported` rather than silently presented as an exact total. Provider billing
statements remain authoritative. Child receipts are informational data in scratch,
not an accounting or security boundary.

## Double mode

`/double` toggles a second opinion for direct turns. With Double on, each
turn opens with two concurrent first-pass analyses — instance A plans
directly, instance B stress-tests — both from the same model, provider, and
configuration over the same session context, then reconciles them into one
unified plan that seeds the normal single-stream loop:

```
independent A ─┐
               ├─ compare → reconcile → one plan → normal turn → one answer
independent B ─┘
```

The first passes propose tool actions in text but never execute: every state
change still happens exactly once, in the main loop. If one stream fails, the
turn continues with the survivor; if both fail, it continues as a plain
single turn. Token, request, and cost accounting cover every stream, and
`/session` counts double passes. Goal work already plans via brief and audit,
so goals stay single-stream. The toggle persists in the session until
changed; the prompt shows `·2×` while it is on.

The analysis passes are read-only at the kernel level, not just by command
filtering: their shell children get the workspace, extra roots, scratch and
`/tmp` read-only through Landlock, no network, and a PATH without any writable
directory, so even an allowlisted tool (`find -fprint0`, `tree -o`) cannot
write. Without Landlock they refuse to run shell commands.

## The five tools

The model-facing surface is exactly: `read` `write` `edit` `bash` `skill`.

Sessions work autonomously by default: implement, verify, and finish the requested
work. `max_rounds` bounds one work chunk; new successful observations allow another
chunk without a manual "go on". A request has at most 12 work checkpoints; goals
share their work/audit checkpoint budget. Repeated tool batches or a chunk with no
new successful observations pause work with a saved reason. `autonomy: false`
retains the hard round limit. Routine commands need no approval; the
destructive-command guard still applies.

There are intentionally no tools for git, grep, find, curl, npm, python,
compilers, test runners, todos, memory, or background jobs — the model uses
normal programs through `bash`. Every wrapper would be another schema,
authority boundary, test surface, and context cost.

- **read** — bounded file reads (1-based, line-numbered, offset/limit)
  from the workspace, allowed roots, the session tmp dir, and `/tmp`.
  Reading a PNG/JPEG/GIF/WebP attaches its pixels to the next message, so
  a vision model can `kit shot` its UI and actually look at it.
  Streams the requested range with a 200-line default; large files need not fit
  in memory. FIFOs and devices are refused, and scanning is bounded to 64 MiB.
- **write** — atomic create/replace (tmp file + rename), parents created
  inside allowed roots, never through symlinks.
  Native read/write/edit accept `$TMPDIR/path` for private session scratch,
  using the same directory as bash. Arbitrary `/tmp` writes require bash or
  an explicit allowed write root.
- **edit** — exact replacement; fails unless `old_text` occurs exactly
  `expected_matches` times (default 1). An `edits` array applies up to 64
  sequential replacements with one atomic write: any mismatch leaves the file
  untouched. Files/results over 4 MiB fail before writing; truncated reads can
  never become edits.
- **bash** — normal Linux commands with captured stdout/stderr, exit status,
  timeout, cancellation, `pipefail`, sandboxing, and network access on by default
  (`--offline` denies it: guard fails fast, seccomp blocks the sockets).
- **skill** — `list` / `search` / `load` Markdown skills (names first,
  BM25 search with descriptions, full text only when deliberately loaded).

Everything else is a subcommand of the same binary, staged on the sandbox
`PATH` each session so the model reaches it through `bash`:

```
pocket kit web URL          readable page text + numbered links
pocket kit search QUERY     keyless web search (DuckDuckGo html → lite)
pocket kit dom URL          JS-rendered text via headless Chrome
pocket kit shot URL OUT.png screenshot for visual QA (desktop/mobile sizes)
pocket kit frame SCENE.html OUT.png --time 1.5   seek a deterministic frame
pocket kit video SCENE.html OUT.mp4 --duration 6 --fps 30 --size 1920x1080 [--audio mix.wav] [--start S]
pocket kit vcheck FILE.mp4 [--duration S]        finished-video QA: streams, black/frozen/silent spans, loudness, clipping
pocket kit vsheet FILE.mp4 OUT.png [--n 12]       the whole video as one timestamped contact sheet
pocket kit say OUT.wav "script" | --file S.txt    neural narration + measured timing (json, srt, captions.html, cues.css)
pocket kit music OUT.wav --style lofi --duration 60 --seed 3   generative stereo score (6 styles, arranged, mixed, reverbed)
pocket kit mix OUT.wav --voice v.wav --music m.wav --at 4.2:whoosh.wav   ducking, effects cues, -14 LUFS, peak ceiling
pocket kit theme "topic words"                    palette + font pair from the subject (avoids the default looks)
pocket kit asset search|get|font ...              CC0 3D models/HDRIs/textures, open images/audio, Google Fonts, three.js
pocket kit sfx OUT.wav whoosh [--duration .6]   seeded native sound effects
pocket kit img FILE...      png/jpeg/gif/webp/svg type + dimensions
pocket kit svg FILE         structure, viewBox, ids, animation count (lint covers hygiene)
pocket kit spring K C M     physical spring → CSS linear() easing + duration
pocket kit wav OUT "C4+E4+G4:1 | saw> C2:.5 C2 | K:.25 H S H"   chords, mixed tracks, drums
pocket kit audio FILE.wav   loudness, peak, clipping, pitch, silence
pocket kit lint [PATH...]   one rule engine: security (secrets, injection, weak crypto, TLS off, unsafe C), backend
                            (SQL concat, no timeouts, leaky errors, loose CORS/cookies/JWT), perf (I/O in loops, SELECT *),
                            DRY (duplicated blocks across files), coding patterns, UI slop + a11y (default AI palettes and
                            fonts, glow halos, contrast, focus, alt, lang, reduced motion) and SVG (scripts, external refs,
                            embedded rasters, wasted precision, filters, no accessible name). --min h|m|l. Also runs on
                            every write and on the changed files before a turn ends: agents never have to ask for it.
pocket kit slop FILE...     placeholders, stubs, conflict markers, AI-tell prose
pocket kit find QUERY       ranked code search (BM25 over chunks, no index)
pocket kit sym NAME|.       definitions (outline with ".") — LSP-lite
pocket kit refs NAME        whole-word references
pocket kit probe            OS, CPU, memory, disk, GPU, toolchain
pocket kit ports [PORT]     listening TCP sockets with owning pid + command
pocket kit wait PORT|URL    block until a server answers (instead of sleep)
pocket kit net URL          HTTP timing (dns/connect/tls/ttfb), redirects, headers
pocket kit seo FILE|URL     title, meta, canonical, headings, alt, share cards, JSON-LD
pocket kit csv FILE         dataset profile: types, missing, stats, class balance
pocket kit bench [-n N] CMD min/median/p95 wall time, CPU, peak RSS
```

A tool costs a schema in every request; a kit subcommand costs one line in
the system prompt.

Scenes are HTML whose motion is CSS/SVG animation (seeked deterministically, so a
scene is a timeline written in CSS and cue times come from `kit say`) or that define
`window.renderFrame(timeSeconds)`, optionally with a `renderReady` promise. The native renderer drives the existing browser over private pipes,
seeks every frame, streams PNG directly into FFmpeg and publishes the output
only after successful completion. Each render gets its own browser profile.
Cancellation, JavaScript/encoder errors and timeouts preserve existing output.
Limits are 900 seconds, 54,000 frames and a wall-clock deadline that scales with the
frame count (`--timeout` up to 4 h); render longer work in parts with `--start`. `kit frame` needs only Chrome. TypeScript must be
compiled by the project's own build step first; offline scenes use local assets.

### Video studio

The bundled `video-studio` skill is loaded automatically for video requests (Jev
decides, with a keyword fallback; the first `kit video` render also waits once for it).
It carries the publish-versus-private decision, the hook and retention structure,
art direction that rejects the default AI looks, branding rules (only identity the
user gave, generic Subscribe/Follow/Like when several platforms), the audio mix and
a QA gate. The tools underneath are native and cheap:

- Narration: Piper neural voices, synthesised at about 0.1x real time on at most four
  low-priority cores. `pocket kit say --setup` downloads the engine and a voice once
  (about 90 MB, user-run: the sandbox sees the voices directory read-only).
- Music: `kit music` composes chord progressions with voice-leading, bass, arpeggios,
  motifs, drums, sidechain pump, a riser and breakdown, reverb and a bus compressor in
  about a second of CPU for a minute of audio.
- `kit mix` normalises voice, ducks the music while someone speaks, places effects,
  and finishes at -14 LUFS with a look-ahead limiter.
- `kit frame`/`kit video` lint the scene at sampled times (safe margins, clipping,
  overlap, legibility floor, contrast, blank frames and failed assets). Shared
  source review reports generic copy and template cues as separate suggestions;
  intentional fonts and palettes alone are not defects. Encoding is tagged
  BT.709, so colours match the scene.
- Real assets: Poly Haven CC0 models, HDRIs and textures, Openverse images and audio,
  Google Fonts as local files, and three.js for software-WebGL scenes, each with a
  written `ATTRIBUTION.txt`.

[The narrated example](examples/capabilities/video/render.sh) runs the whole pipeline
(voice, score, mix, captions, click-timed like/subscribe, render, QA).

Run [the motion + audio example](examples/capabilities/motion/render.sh), or start
from [the editable scene](skills/motion-graphics-production/assets/capability-demo.html).
[Capability examples](examples/capabilities/README.md) also include a standalone
C++ shortest-path implementation with a separate oracle, and a numeric classifier
with train-only preprocessing and held-out evaluation. These are tested starting
points; model quality and task-specific verification still matter.


The [Jev paper](https://arxiv.org/pdf/2609.29429v1) motivates keeping probability
scores, checking evidence quality and evaluating thresholds per task domain.
Pocket batches the questions, treats supplied state as untrusted evidence,
preserves structured context on fallback and retains requirement/outcome excerpts
with explicit omission markers. Uncertain evidence makes quality judgments
unknown, so normal review still runs. Fixed cutoffs are routing policy, not
measured error guarantees; a tiny local LM cannot approve completion.

## Configuration

One transparent user config, optional project overlay. See
[the example config](examples/config.example.json) for cloud and local model aliases:

```
~/.config/pocketharness/config.json
./.pocket/config.json                  # models/aliases only, never providers
```

```json
{
  "default_model": "glm",
  "thinking": "auto",
  "providers": {
    "orcarouter": {
      "protocol": "openai",
      "base_url": "https://api.orcarouter.ai/v1",
      "key_env": "ORCAROUTER_API_KEY"
    }
  },
  "models": {
    "glm": { "provider": "orcarouter", "model": "z-ai/glm-5.3-flash" }
  },
  "roles": { "fast": "deepseek:deepseek-flash", "review": "deepseek:deepseek-flash,openrouter:z-ai/glm-5.3-flash" },
  "hooks": {
    "post_edit": ["case {file} in *.py) python3 -m py_compile {file};; esac"],
    "pre_bash": [],
    "post_bash": [],
    "stop": ["make -s test"]
  },
  "review": true,
  "autonomy": true,
  "jev": true,
  "local_lm": {
    "server": "~/llama.cpp/llama-server", "model": "~/models/Qwen3.5-0.8B-Q4_0.gguf",
    "key_file": "", "port": 18735, "threads": 4, "ctx": 4096
  },
  "tool_network": true,
  "bash_timeout": 120,
  "output_limit": 262144,
  "disk_budget_gb": 40,
  "disk_reserve_gb": 10,
  "max_file_gb": 32,
  "max_rounds": 100,
  "working_context_tokens": 96000,
  "allow_read": [],
  "allow_write": [],
  "expose_env": []
}
```

Hooks run in exactly the bash-tool sandbox (`{file}`/`{cmd}` expand
shell-quoted): `post_edit` failures return to the model, a failing
`pre_bash` blocks the command. `post_bash` runs once after each executed
command with `{cmd}`, `{ok}` (`true`/`false`) and `{result}` (first 4,000 bytes).
A failed outcome hook makes the tool result fail while preserving its original
exit evidence. Placeholders expand from the original template once; quoted
command/result text cannot introduce another substitution. Read-only evidence
passes skip user hooks, and cancellation prevents new outcome hooks.
Failing `stop` hooks keep the turn going. A
`goal_done` hook runs before a goal may be certified: any failure sends the
agent back to work with the hook output (put the project's real acceptance
check there). A failing `stop` hook is re-run after each fix, three times per turn.
`local_lm` is optional: the harness starts `llama-server` on loopback only
when a judgement needs it (niced, thread-capped). Sessions serialize access to
its slot, cache repeated questions, and reuse prompt prefixes. Missing remote
decisions fall back locally within a bounded time budget. Local transcript
judgments assist the worker but cannot independently certify goal completion or
skip the review council. An existing externally started server is left running.

Disk guard: a bash command is stopped when it consumes more than
`disk_budget_gb` of free space, or keeps writing once free space is below
`disk_reserve_gb`. No single file a tool process writes may exceed
`max_file_gb` (`RLIMIT_FSIZE`, inherited by background children). A session
watchdog also stops backgrounded tool processes when free space is below the
reserve and still falling. `0` disables a check.

Security-sensitive keys (`providers`, `tool_network`, `allow_read`,
`allow_write`, `expose_env`, `local_lm`, hooks, timeouts, `max_rounds`, disk limits) from **project**
config are ignored
with a warning — a repository must never silently escalate its own authority,
and especially never redirect provider endpoints (which decide where API
keys are sent). Provider `base_url` must be `https`, or `http` loopback for
local daemons; `key_env` must be a shell variable name, and may be omitted
entirely for loopback providers (LM Studio / Ollama / llama.cpp run keyless
by default — no dummy key needed). IPv4 loopback addresses and `[::1]` are
validated as addresses; names such as `127.attacker.example` are rejected. Invalid config
produces precise errors, never silent guesses.

```json
{
  "providers": {
    "lmstudio": { "protocol": "openai", "base_url": "http://127.0.0.1:1234/v1" }
  },
  "models": {
    "local": { "provider": "lmstudio", "model": "<model-id-as-shown-in-lm-studio>" }
  }
}
```

Built-in providers (just export the key): `openrouter orcarouter deepseek
friendli together deepinfra cerebras groq mistral xai gemini nvidia
fireworks moonshot zai agnes atria longcat ollama-cloud qwen runinfra
streamlake xiaomi stepfun kimi-coding minimax minimax-plan anthropic openai codex`, plus
local `ollama lmstudio llamacpp`. `codex` uses your ChatGPT login from
`~/.codex/auth.json` (read-only: run `codex` to refresh it) over the Codex
Responses API. Use `provider:model-id` directly. The local presets point to loopback
ports 11434, 1234, and 8080 respectively. Install/load the model in your server;
PocketHarness does not start a model daemon or allocate its GPU memory.

Model aliases can tune compatibility without another provider implementation:

```json
{
  "models": {
    "small": {
      "provider": "ollama", "model": "qwen3:8b",
      "context": 8192, "max_tokens": 2048,
      "reasoning": "effort", "stream_usage": true
    },
    "claude": {
      "provider": "anthropic", "model": "claude-sonnet-4-6",
      "max_tokens": 8192, "reasoning": "adaptive", "prompt_cache": true
    }
  }
}
```

`reasoning` accepts `auto`, `effort`, `budget`, `adaptive`, or `none` (omit
reasoning controls for models that reject them). `budget`/`adaptive` apply to
Anthropic; OpenAI-compatible servers use effort. `token_parameter` chooses
`max_tokens` or `max_completion_tokens` (the built-in OpenAI provider uses the
latter). Set `stream_usage: false` for older compatible servers. `prompt_cache`
controls Anthropic automatic caching and OpenAI cache keys; it defaults to true.
Matching explicit `provider:model` specs inherit the alias settings too.

`--max-tokens N` overrides the model's completion budget for this invocation.
The effective budget is capped at a quarter of the context window; unpublished
local windows default conservatively to 32k. Pin `context` to your daemon's
**loaded** context size, which can be smaller than the model's advertised limit.
For economy, start with `-t none --max-tokens 2048` on models that support `none`.

Use the model id LM Studio shows in its server panel; the context window
resolves live from the daemon's `/models` listing when published. Start the
LM Studio server (default `127.0.0.1:1234`) before running pocket.

## Providers: wire protocols, not brands

No per-vendor classes. Three protocol implementations driven by config data:

- **openai** — OpenAI-compatible chat + tool calling (OpenRouter, OrcaRouter,
  DeepSeek, Friendli, Together, DeepInfra, local servers, ...).
- **anthropic** — Anthropic Messages + tool use (Kimi coding, MiniMax too).
- **codex** — OpenAI Responses over the ChatGPT Codex backend, encrypted
  reasoning replayed to the same model.

**Self-maintaining catalog.** `stateDir()/catalog.json` merges every keyed
provider's `/models` listing (context, reasoning support, vision, prices),
refreshed in a background thread once a day, so new model slugs appear
without edits. A model the catalog marks as non-reasoning never receives
effort parameters.

**Learned quirks.** When a provider rejects a request with a recognizable
400 (unsupported reasoning effort, `stream_options`, the wrong max-token
parameter), the harness records the quirk for that model in `brain.json`
and retries without it — once, then forever after. Per-route EWMA
health (provider, model and explicit routing; week-old data expires) is tracked
the same way (`/brain`), so two models behind one gateway are judged separately.

Model specs: `alias`, `provider:model`, or `provider:model@routing`:

```
glm
orcarouter:glm-5.3-flash
openrouter:hy4-preview@deepinfra   # pins OpenRouter routing, no fallbacks
deepseek:deepseek-chat
friendli:glm-5.3-flash
```

Each session owns its model and thinking level (stored in the session,
restored by `--resume`); new sessions start from config unless `-m`/`-t` say
otherwise, so concurrent sessions never affect each other.
HTTPS is done by invoking the installed **`curl` binary** (argv-based, never
shell strings, HTTPS-only protocol lock + TLS 1.2+ for `https://` URLs).
Per request, the harness stages body, response headers, and the secret
bearer (`-K` config, 0600) in a fresh parent-only directory under the state
dir; the key never appears in argv, logs, sessions, child environments, or
any child-visible filesystem. The staging dir is unlinked after each
request. PocketHarness ships no TLS/HTTP stack of its own.

Failed requests retry with backoff instead of failing the turn: transport
errors and HTTP 429/5xx are retried up to 4 attempts (1s/2s/4s cooldowns,
announced in the UI, cancellable with Ctrl-C). Numeric `Retry-After` headers are honored, capped at 60 seconds. Retries happen only while
the answer has not started streaming — once tokens are visible, a failure
fails fast rather than duplicating output. Other HTTP 4xx responses never retry.

Thinking levels: `adaptive/auto/off/none/minimal/low/medium/high/xhigh/max`
(`/thinking` or `-t`). The default `adaptive` picks a level per request: `high`
for a new instruction, an overseer nudge, a failed tool or a fresh compaction,
`medium` right after edits (checking them), `low` while reading successfully.
Anthropic wires stay at one level there, because changing thinking settings
invalidates their message cache. `auto` and `off` omit effort controls;
`none` explicitly disables reasoning where supported. DeepSeek also receives
its thinking toggle. `max` is sent as `max`; use `xhigh` when that is the model's
supported maximum. Each endpoint decides which levels its model supports.
Anthropic manual budgets always stay below the output limit; use
`reasoning: "adaptive"` for models using adaptive thinking.

Reasoning streams dimly in the TUI and to stderr in `-p`. Continuation data
(DeepSeek reasoning, OpenRouter reasoning details, Anthropic signed thinking
blocks) survives tool calls and resume, and is only replayed to its originating
provider/model. This data is stored in the private session log. Multiple
Anthropic tool results are grouped into one user message.

Malformed, errored, empty, incomplete, or oversized streams fail locally; partial
tool calls are never executed. Servers returning ordinary JSON despite
`stream: true` are supported. Transport responses are bounded to 8 MiB.

Context windows resolve live: unless a model pins `"context"` explicitly,
the harness `GET`s `{base}/models` once per endpoint per process (`{"data"}`, bare
arrays, and Gemini `{"models"}` shapes) and adopts the published window
for that exact model id. Anything unpublished or unreachable keeps the
configured default; the probe never fails a run.

### Prompt caching

PocketHarness keeps prompt prefixes stable by design:

- The system prompt is **frozen** at session start (sidecar
  `<id>.meta.json`) and reused byte-identically on `--resume`.
- Tool schemas and ordering never change mid-session.
- Messages are append-only; skills arrive as tool results at the tail, never
  spliced into the prefix. No timestamps, metrics, or dynamic data pollute
  the prefix. Serialization is deterministic.
- OpenRouter requests carry a stable `session_id`; OpenAI requests carry a
  stable `prompt_cache_key`. Anthropic requests enable automatic caching.
- DeepSeek `prompt_cache_hit/miss_tokens`, OpenRouter `cached_tokens`/`cost`,
  and Anthropic cache reads are parsed and shown in `/session` and `-p`
  stderr output. Unreported = unknown, never inferred.

Compaction legitimately establishes a new prefix; afterwards the new prefix
stays stable again. The threshold adapts: with measured cache reuse
(resent context is cheap) compaction waits until 90% of the window;
without it, it runs at 75%, summarized by the cheaper `fast` model.
Byte-identical large tool results are replaced by a reference to the
earlier call, and oversized outputs (12–48 KB) are distilled by Jev,
which keeps the chunks relevant to the task (errors always kept) instead
of a blind head/tail cut. There is no cache manager, daemon, or subsystem — just a
stable prefix and honest counters.

Tool results are capped **once**, keeping their head and tail within 12k bytes.
Appending more messages never shrinks or rewrites earlier results. Full results
stay on disk, while bounded copies keep memory and subsequent requests small.
Actual reuse still depends on the provider, cache lifetime, and minimum prefix
size; no cache-hit rate is promised.

JSON serialization preserves valid UTF-8 and replaces invalid bytes with `�`,
including truncated characters and raw bytes in older session logs. This prevents
binary/legacy-encoded tool output from breaking all subsequent requests. Source
files are unchanged; decode them explicitly with their actual encoding when text
fidelity matters. Restart the updated `pocket` and use `--resume` to recover an
affected session.

Cache percentages use only samples with a known denominator. OpenAI-style
uncached tokens are total input minus reported cached input; Anthropic total
input includes uncached input, cache writes, and cache reads. The TUI shows a
rolling window over 20 complete samples. Compaction requests count toward usage.

Wire behavior follows the official [OpenAI Chat API](https://developers.openai.com/api/reference/resources/chat/subresources/completions/methods/create),
[Anthropic caching](https://platform.claude.com/docs/en/build-with-claude/prompt-caching),
[DeepSeek thinking](https://api-docs.deepseek.com/guides/thinking_mode/), and
[Ollama compatibility](https://docs.ollama.com/api/openai-compatibility) contracts.

## System prompt

The built-in base prompt is deliberately minimal: a few lines of capabilities
plus the always-on engineering principles (high quality, minimalism, low LoC,
low entropy, safety-first). It is file-overridable:

```
./.pocket/system.md                  # project override (wins)
~/.config/pocketharness/system.md    # user override
<built-in>                           # fallback
```

Empty files fall through. The active source is announced at startup, shown in
`/session`, and recorded in the session sidecar. Overrides replace the base
only — project instructions and the workspace line are still appended, and
the result is still frozen for cache stability. A project override is
repository-controlled input (like `POCKET.md`); kernel security boundaries
never depend on prompt text.

Project instructions come from the nearest `POCKET.md` (preferred) or
`AGENTS.md` walking up from the workspace. The frozen prompt also carries
the wisdom doctrine and an **awareness block** captured at session start:
host (OS, CPU, memory, disk, GPU, toolchain), date, git state, the
project's verify command, the guardrails actually enforced, and
`.pocket/memory.md` (durable project notes the model appends to).
Briefs, skill hints and overseer notes arrive as ordinary appended
messages, visible in the transcript, never spliced into the prefix.
`~/.config/pocketharness/wisdom.md` (`## ` sections) replaces the built-in
wisdom book.

## Skills

The intentionally flexible layer. 165 skills ship with the repo (UI/UX,
motion, SVG, audio, video, research, ML, security, systems, web, ...)
and `make install` places them in the bundled layer. A skill is just:

```
~/.local/share/pocketharness/skills/<name>/SKILL.md  # bundled (make install)
~/.config/pocketharness/skills/<name>/SKILL.md       # yours (wins)
./.pocket/skills/<name>/SKILL.md                     # project (wins on collision)
```

No manifests, no code, no SDK. Directory name + first heading + the
frontmatter `description:` (or first paragraph) are the metadata. The model sees only that skills exist until it
loads one. `search` ranks by token overlap (name hits outrank heading,
heading outranks preview). See `examples/skills/` for a starter skill, and
`skills/web-research/` for the bundled curl-based web client guide
(`make install` refreshes the bundled skill directory; personal skills in
`~/.config/pocketharness/skills` remain separate and take precedence).

There is no `web_search` tool, fetch subsystem, or browser runtime: web
research is `curl` via `bash`, taught by the skill, gated by the same
`--offline` switch as all tool networking. Fetched content is untrusted
data, never harness authority.

## Sessions & context

Append-only JSONL under `~/.local/share/pocketharness/sessions/` — one event
per line, fsync'd, with 0600 permissions. A torn final record is discarded before
new events are appended. A nonblocking `flock` allows one process to own a session;
other sessions remain independent. Locks release on exit/crash. `--resume` selects
an idle session in the current workspace; an explicit id from another workspace
fails with its location. Legacy sessions without workspace metadata can be
resumed by explicit id. Session listing reads only a small preview of each log.
The TUI shows the full ID. `/sessions` or `/resume` opens a picker for this
workspace; `/resume ID` and `/resume last` switch directly. Switching requires
idle work with no pending draft, queued input or images. A failed switch leaves
the current session intact. Models, roles, history, cost and goals belong to the
selected session; restored goals wait paused for a follow-up. `/session` and the
picker show the last recorded stop reason. Timestamped outcome events are saved
separately from the conversation sent to the model.

Same-workdir sessions exchange bounded status and file-change notices through a
separate directory scoped to that workspace. Conversations, role choices and
model history stay separate. Native write/edit/undo operations share an
interruptible advisory lock; undo refuses to replace a file changed by another
writer. Arbitrary bash commands and external editors still require coordination.
Recursive children inherit model settings without implicitly receiving keys,
share only their workspace's coordination directory, and report cumulative
metered costs to the parent. Use explicit `expose_env` for child provider keys.

Tool batches are persisted before execution. Cancellation closes all outstanding
calls; after a crash, unanswered calls get an “outcome unknown” result and are
never automatically rerun. Persistence errors stop further side effects. No
SQLite, indexing, or daemons. Keys never touch session files.

Context: the last provider token count plus estimated additions since that
request, compared with the (possibly live-resolved) window. `/compact` on demand plus
automatic summarization at the 96,000-token working target, near the model's
window limit, or whenever the next completion would no
longer fit (a full `maxTokens` of headroom is reserved for the answer).
Set `working_context_tokens` to change the working target, or `0` to use only
the model's window limit. Failed summaries back off instead of retrying each
round. Original and latest user requests (up to 64 KiB each) are retained
verbatim alongside the work summary; the summary must preserve intervening
requirements too.
Compaction summarizes older turns and keeps recent raw turns with tool
pairs intact, including within long autonomous turns. Compaction checkpoints
record the cut point so resume reconstructs the summary **and retained tail**.
If compaction cannot make the next request fit, the harness stops locally.
Durable knowledge belongs in project files or skills.

## Security architecture

Threats assumed: model hallucination, prompt injection from repo content,
malicious symlinks, credential exfiltration, privilege escalation, child
authority inflation, torn writes, shell/argv injection, terminal-escape abuse.

Default posture:

```
Workspace read/write        YES (that is the agent's job)
Session tmp                 YES (narrow; never holds secrets)
State dir / sessions        PARENT ONLY (except scoped workspace notices)
System binaries/libraries   read/execute
$HOME, ~/.ssh, other repos  NO by default
Provider network (harness)  YES (confined curl, brokered key)
Model bash network          YES by default (--offline denies)
sudo / setuid gain          NO (NO_NEW_PRIVS)
Running as root             REFUSED (unless --allow-root)
```

Mechanisms (a few Linux primitives, not a policy framework):

- **openat2 containment** (`RESOLVE_BENEATH | RESOLVE_NO_MAGICLINKS`,
  `RESOLVE_NO_SYMLINKS` for writes) anchored at open root fds. No
  string-prefix checks. Pre-openat2 kernels get a strict symlink-refusing
  fallback plus a loud warning — never a silent downgrade.
- **Landlock** confines every model command: RO system runtime, RW workspace
  + session tmp + `/tmp`. The state dir, `~/.ssh`, shell configs, sibling
  projects, and cloud credentials are simply unreachable. Provider `curl`
  gets its own tighter profile: system RO, per-request staging dir RW,
  network allowed, foreign-arch syscalls killed, tight rlimits. Its environment
  excludes unrelated secrets; only proxy and CA configuration is retained.
  Ambient curl config files and authenticated redirects are disabled, and
  loopback connections bypass proxies.
  Versioned Node runtimes already selected by PATH under nvm, Volta, fnm,
  mise or asdf receive read/execute access to that runtime directory only;
  the containing home directory and other versions remain outside the grant.
- **Key broker, not key sharing**: provider keys live only in the parent's
  environment and per-request 0600 staging. No keyfile is ever handed to a
  model tool child, no session id / depth / parent flag travels via env (a recursive
  `pocket` reads those from `$TMPDIR/pocket.parent`), and keys never appear
  in argv, logs, or session files.
- **NO_NEW_PRIVS** on every model child; setuid/sudo gains impossible.
- **Sanitized environment**: default-deny allowlist (`PATH HOME USER LANG
  TERM PWD TMPDIR ...` + `LC_*`); `*_API_KEY/*_TOKEN/*_SECRET/AWS_*/SSH_*`
  never pass. Deliberate passthrough only via user-level `expose_env`
  (with a stderr warning for secret-looking names).
- **Network isolation** via seccomp (INET sockets fail `ECONNREFUSED`,
  `AF_UNIX` untouched, foreign-arch syscalls kill the process), enforced
  without privileges and inherited by the whole subtree. Applies only under
  `--offline`; `seccomp unavailable` is reported, not hidden. On kernels
  without seccomp the guard + parent-net propagation are the backstop.
- **SSH**: `ssh`/`git` remotes work as normal tool-network clients. Agent
  keys are NOT forwarded by default (`SSH_AUTH_SOCK` is scrubbed); opt in
  with user-level `expose_env: ["SSH_AUTH_SOCK"]` when you want the agent
  pushing over ssh. The fake `$HOME` carries no keys either way.
- **Destructive-command guard**: last-resort screening (`rm -rf .`, `git
  reset --hard`, `git clean -f`, `mkfs`, `dd of=/dev`, fork bombs,
  `chmod -R /`, computed recursive-delete targets, force-pushes, ...) → human `[y/N]` approval in the TUI, fail-closed in
  `-p` unless `--allow-destructive`. The model can never approve itself.
- **Terminal sanitization**: ESC/CSI/OSC/C0 stripped from all untrusted
  output before rendering.
- **Explicit overrides only**: `--allow-read/--allow-write PATH`,
  `--offline`, `--allow-root`, `--allow-destructive`, `--unsafe`.
  `--unsafe` is conspicuous, never default, never persisted, never
  inheritable by children, never activatable by repo files or the model.

`pocket --unsafe` relaxes filesystem/network/tool containment for one
invocation with a visible banner. Even then, the destructive guard stays on
and provider keys stay out of the model environment.

### Known limitations (honest)

- `/tmp` is shared RW by design (toolchains need it); treat it as untrusted
  scratch, not a secret store (`TMPDIR` points at the private session dir).
- Without Landlock/seccomp (old kernels, exotic sandboxes), `bash`
  containment degrades to guard + env + cwd; `/security` and startup notes
  always show what is actually enforced.
- The workspace itself is writable by design — the guard catches only
  obviously catastrophic classes, not all bad edits. Git is your undo.
- Different sessions may edit the same workspace. Native file tools serialize
  mutations and exchange notices; arbitrary bash commands and external editors
  do not participate in that lock. Read current contents before changing files.
- This is a mistake-tolerant harness, not a hostile-code sandbox.

## Layout

```
~/src/pocketharness/                 source / Git repo
~/.local/bin/pocket                  installed executable
~/.config/pocketharness/config.json  user config (+ roles.json, env, system.md, wisdom.md, skills/)
~/.local/share/pocketharness/        sessions, catalog.json, brain.json, bundled skills
~/.cache/pocketharness/              disposable cache
```

```bash
make               # build ./pocket
make test          # functional + security + localhost curl integration tests
make sanitize      # AddressSanitizer + UndefinedBehaviorSanitizer
make install       # install to ~/.local/bin + bundled skills
make clean
```

Tests require localhost sockets and a writable checkout for isolated fixtures.
They use synthetic provider responses and make no paid model calls. CI builds
with GCC and Clang, then runs the sanitizer suite.

## Anti-goals (the constitution)

No extension/plugin API, event bus, MCP, dependency graph, swarms,
subagent/workflow frameworks, embedded browser runtime, vector database,
telemetry, capability registries, Linux-command wrappers, DI frameworks,
or enterprise ceremony. No libcurl/Boost/ncurses/OpenSSL linkage, no
SQLite, no second language in the harness. What bigger harnesses build as
subsystems — councils, observers, guardians, memory, model routing,
indexing — exists here only as a few bounded functions in the core, each
replaceable by deleting it. Boring function > framework; struct >
hierarchy; file > service; subprocess > plugin; Linux primitive > custom
subsystem; deletion > abstraction.

If PocketHarness starts resembling the frameworks it replaced, simplify.
