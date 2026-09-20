<img src="mascot.png" alt="PocketHarness mascot: a pocket-sized toolbox robot" width="180" align="right">

# PocketHarness

A deliberately tiny, Linux-native C++ coding-agent TUI harness. It feels like
Pi when used interactively, but architecturally it is the opposite of the big
agent frameworks: no SDKs, no runtimes, no plugins, no MCP, no hooks, no
orchestration layers, no dependency jungle.

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

One competent engineer should be able to understand essentially the entire
architecture in an afternoon. (About 8k lines of C++ including headers, 12 translation units.)

## Install

Requirements: `g++` (C++20), `make`, `curl`. No bundled third-party code.

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

## Quick start

```bash
cd ~/projects/foo
pocket                  # interactive agent, workspace = current directory
pocket /some/project    # explicit workspace
pocket -p "Review src/network.cpp for concurrency problems."
pocket -m glm -p "Review src/network.cpp."
pocket -m ollama:qwen3:8b -t none --max-tokens 2048 -p "Explain this project."
pocket --resume         # newest idle session in this workspace
pocket --sessions       # list workspaces, active/idle sessions, previews
```

Set provider keys as environment variables (never in files that get committed):

```bash
export DEEPSEEK_API_KEY=...
export OPENROUTER_API_KEY=...
```

Slash commands: `/model` `/thinking` `/compact` `/skills` `/session`
`/security` `/help` `/quit`. Keys: Enter submits, Ctrl-J/Alt-Enter newline,
Up/Down history, Ctrl-C cancels (empty prompt quits), Ctrl-D quits.
Paste is bracketed (multi-line paste never submits early); the pinned bottom
bar always shows the input box plus context/tok-s/cache KPIs.
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

## The five tools

The model-facing surface is exactly: `read` `write` `edit` `bash` `skill`.

Sessions work autonomously by default: implement, verify, and continue up to the
configured round limit. Three identical tool batches stop a stuck loop. Routine
commands need no approval; the destructive-command guard still applies.

There are intentionally no tools for git, grep, find, curl, npm, python,
compilers, test runners, todos, memory, or background jobs — the model uses
normal programs through `bash`. Every wrapper would be another schema,
authority boundary, test surface, and context cost.

- **read** — bounded file reads (1-based, line-numbered, offset/limit)
  from the workspace, allowed roots, the session tmp dir, and `/tmp`.
  Streams the requested range with a 200-line default; large files need not fit
  in memory. FIFOs and devices are refused, and scanning is bounded to 64 MiB.
- **write** — atomic create/replace (tmp file + rename), parents created
  inside allowed roots, never through symlinks.
- **edit** — exact replacement; fails unless `old_text` occurs exactly
  `expected_matches` times (default 1). An `edits` array applies up to 64
  sequential replacements with one atomic write: any mismatch leaves the file
  untouched. Files/results over 4 MiB fail before writing; truncated reads can
  never become edits.
- **bash** — normal Linux commands with captured stdout/stderr, exit status,
  timeout, cancellation, `pipefail`, sandboxing, and network access on by default
  (`--offline` denies it: guard fails fast, seccomp blocks the sockets).
- **skill** — `list` / `search` / `load` Markdown skills (metadata first,
  full text only when deliberately loaded).

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
  "tool_network": true,
  "bash_timeout": 120,
  "output_limit": 262144,
  "max_rounds": 100,
  "allow_read": [],
  "allow_write": [],
  "expose_env": []
}
```

Security-sensitive keys (`providers`, `tool_network`, `allow_read`,
`allow_write`, `expose_env`, timeouts, `max_rounds`) from **project**
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

Built-in provider names also include `openai`, `ollama`, `lmstudio`, and
`llamacpp`; use `provider:model-id` directly. The local presets point to loopback
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

No per-vendor classes. Two protocol implementations driven by config data:

- **openai** — OpenAI-compatible chat + tool calling (OpenRouter, OrcaRouter,
  DeepSeek, Friendli, Together, DeepInfra, local servers, ...).
- **anthropic** — Anthropic Messages + tool use.

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

Thinking levels: `auto/off/none/minimal/low/medium/high/xhigh/max` (`/thinking`
or `-t`). The default `auto` and compatibility setting `off` omit effort controls;
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
stays stable again. There is no cache manager, daemon, or subsystem — just a
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
`AGENTS.md` walking up from the workspace. Effective context is always just:
frozen system prompt + project instructions + loaded skills + conversation —
nothing hidden, nothing injected per-turn.

## Skills

The intentionally flexible layer. A skill is just:

```
~/.config/pocketharness/skills/<name>/SKILL.md   # global
./.pocket/skills/<name>/SKILL.md                 # project (wins on collision)
```

No manifests, no code, no SDK. Directory name + first heading + first
paragraph are the metadata. The model sees only that skills exist until it
loads one. `search` ranks by token overlap (name hits outrank heading,
heading outranks preview). See `examples/skills/` for a starter skill, and
`skills/web-research/` for the bundled curl-based web client guide
(`make install-skills` copies bundled skills into the user skill dir;
never part of `make install`).

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

Tool batches are persisted before execution. Cancellation closes all outstanding
calls; after a crash, unanswered calls get an “outcome unknown” result and are
never automatically rerun. Persistence errors stop further side effects. No
SQLite, indexing, or daemons. Keys never touch session files.

Context: the last provider token count plus estimated additions since that
request, compared with the (possibly live-resolved) window. `/compact` on demand plus
automatic summarization at ~90%, or whenever the next completion would no
longer fit (a full `maxTokens` of headroom is reserved for the answer).
Compaction summarizes older turns and keeps recent raw turns with tool
pairs intact, including within long autonomous turns. Compaction checkpoints
record the cut point so resume reconstructs the summary **and retained tail**.
If compaction cannot make the next request fit, the harness stops locally.
No memory graphs, no governors, no injected observations. Durable knowledge
belongs in project files or skills.

## Security architecture

Threats assumed: model hallucination, prompt injection from repo content,
malicious symlinks, credential exfiltration, privilege escalation, child
authority inflation, torn writes, shell/argv injection, terminal-escape abuse.

Default posture:

```
Workspace read/write        YES (that is the agent's job)
Session tmp                 YES (narrow; never holds secrets)
State dir / sessions        PARENT ONLY (no child profile grants it)
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
- Different sessions may edit the same workspace. Session locks prevent log
  corruption; they are not workspace-wide transaction locks.
- This is a mistake-tolerant harness, not a hostile-code sandbox.

## Layout

```
~/src/pocketharness/                 source / Git repo
~/.local/bin/pocket                  installed executable
~/.config/pocketharness/config.json  user config (+ system.md, skills/)
~/.local/share/pocketharness/        sessions + state
~/.cache/pocketharness/              disposable cache
```

```bash
make               # build ./pocket
make test          # functional + security + localhost curl integration tests
make sanitize      # AddressSanitizer + UndefinedBehaviorSanitizer
make install       # install to ~/.local/bin
make install-skills  # copy bundled skills to ~/.config/pocketharness/skills
make clean
```

Tests require localhost sockets and a writable checkout for isolated fixtures.
They use synthetic provider responses and make no paid model calls. CI builds
with GCC and Clang, then runs the sanitizer suite.

## Anti-goals (the constitution)

No extension/plugin API, hooks, event bus, MCP, dependency graph, councils,
swarms, subagent/workflow frameworks, embedded browser, semantic memory,
telemetry, local-ML helpers, prompt-injection systems,
capability registries, Linux-command wrappers, DI frameworks, or enterprise
ceremony. No libcurl/Boost/ncurses/OpenSSL linkage, no SQLite, no browser
runtime, no `web_search` subsystem, no special Git or web tool, no
background-job framework. Boring function > framework; struct > hierarchy;
file > service; subprocess > plugin; Linux primitive > custom subsystem;
deletion > abstraction.

If PocketHarness starts resembling the frameworks it replaced, simplify.
