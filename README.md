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
architecture in an afternoon. (~5k lines of C++, ~10 translation units.)

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
pocket -p -m orcarouter:glm-5.3-flash "Review src/network.cpp."
pocket --resume         # continue the newest session
pocket --sessions       # list sessions
```

Set provider keys as environment variables (never in files that get committed):

```bash
export DEEPSEEK_API_KEY=...
export OPENROUTER_API_KEY=...
```

Slash commands: `/model` `/thinking` `/compact` `/skills` `/session`
`/security` `/help` `/quit`. Keys: Enter submits, Ctrl-J/Alt-Enter newline,
Up/Down history, Ctrl-C cancels (empty prompt quits), Ctrl-D quits.

## Recursive agency (no orchestration framework)

`pocket -p` composes with Linux instead of a subagent framework:

```bash
pocket -p "Inspect auth" > /tmp/auth-review &
pocket -p "Inspect storage" > /tmp/storage-review &
wait
```

PocketHarness itself does not know what a "swarm" is. Child instances get
their own session, stay within the parent workspace, never inherit `--unsafe`
or `--network`, and stop nesting past depth 5. Provider keys reach children
through a 0600 session keyfile (never through the model's scrubbed
environment); kernel confinement is inherited and cannot be shed.

## The five tools

The model-facing surface is exactly: `read` `write` `edit` `bash` `skill`.

There are intentionally no tools for git, grep, find, curl, npm, python,
compilers, test runners, todos, memory, or background jobs — the model uses
normal programs through `bash`. Every wrapper would be another schema,
authority boundary, test surface, and context cost.

- **read** — bounded file reads (1-based, line-numbered, offset/limit).
- **write** — atomic create/replace (tmp file + rename), parents created
  inside allowed roots, never through symlinks.
- **edit** — exact replacement; fails unless `old_text` occurs exactly
  `expected_matches` times (default 1). Never edits a surprise match.
- **bash** — normal Linux commands with captured stdout/stderr, exit status,
  timeout, cancellation, sandboxing, and no network by default.
- **skill** — `list` / `search` / `load` Markdown skills (metadata first,
  full text only when deliberately loaded).

## Configuration

One transparent user config, optional project overlay:

```
~/.config/pocketharness/config.json
./.pocket/config.json                  # may set models/providers only
```

```json
{
  "default_model": "orcarouter:glm-5.3-flash",
  "thinking": "off",
  "providers": {
    "orcarouter": {
      "protocol": "openai",
      "base_url": "https://api.orcarouter.ai/v1",
      "key_env": "ORCAROUTER_API_KEY"
    }
  },
  "models": {
    "glm": { "provider": "orcarouter", "model": "glm-5.3-flash", "context": 200000 }
  },
  "tool_network": false,
  "bash_timeout": 120,
  "output_limit": 262144,
  "allow_read": [],
  "allow_write": [],
  "expose_env": []
}
```

Security-sensitive keys (`tool_network`, `allow_read`, `allow_write`,
`expose_env`, timeouts) from **project** config are ignored with a warning —
a repository must never silently escalate its own authority. Invalid config
produces precise errors, never silent guesses.

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

The last explicitly selected model and thinking level persist across sessions.
HTTPS is done by invoking the installed **`curl` binary** (argv-based, never
shell strings); the API key travels in a 0600 `-K` config file, never in
argv, logs, or sessions. PocketHarness ships no TLS/HTTP stack of its own.

Thinking levels (`off/low/medium/high/max`, `/thinking`) map to
`reasoning_effort` / Anthropic thinking budgets, and are omitted entirely
when `off` for maximum endpoint compatibility.

### Prompt caching

PocketHarness treats cache reuse as an invariant, not luck:

- The system prompt is **frozen** at session start (sidecar
  `<id>.meta.json`) and reused byte-identically on `--resume`.
- Tool schemas and ordering never change mid-session.
- Messages are append-only; skills arrive as tool results at the tail, never
  spliced into the prefix. No timestamps, metrics, or dynamic data pollute
  the prefix. Serialization is deterministic.
- Every request in a session carries a stable `session_id` (OpenRouter) for
  sticky routing to the warm-cache endpoint.
- DeepSeek `prompt_cache_hit/miss_tokens`, OpenRouter `cached_tokens`/`cost`,
  and Anthropic cache reads are parsed and shown in `/session` and `-p`
  stderr output. Unreported = unknown, never inferred.

Compaction legitimately establishes a new prefix; afterwards the new prefix
stays stable again. There is no cache manager, daemon, or subsystem — just a
stable prefix and honest counters.

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
loads one. See `examples/skills/` for a starter skill.

## Sessions & context

Append-only JSONL under `~/.local/share/pocketharness/sessions/` — one event
per line, fsync'd, tolerant of a torn last line after a crash. No SQLite, no
indexing, no daemons. Keys never touch session files.

Context: estimated usage vs configured window, `/compact` on demand plus
automatic summarization past ~80% (older turns summarized, recent raw turns
kept with tool pairs intact). No memory graphs, no governors, no injected
observations. Durable knowledge belongs in project files or skills.

## Security architecture

Threats assumed: model hallucination, prompt injection from repo content,
malicious symlinks, credential exfiltration, privilege escalation, child
authority inflation, torn writes, shell/argv injection, terminal-escape abuse.

Default posture:

```
Workspace read/write        YES (that is the agent's job)
Session tmp + own state     YES (narrow)
System binaries/libraries   read/execute
$HOME, ~/.ssh, other repos  NO by default
Provider network (harness)  YES
Model bash network          NO by default
sudo / setuid gain          NO (NO_NEW_PRIVS)
Running as root             REFUSED (unless --allow-root)
```

Mechanisms (a few Linux primitives, not a policy framework):

- **openat2 containment** (`RESOLVE_BENEATH | RESOLVE_NO_MAGICLINKS`,
  `RESOLVE_NO_SYMLINKS` for writes) anchored at open root fds. No
  string-prefix checks. Pre-openat2 kernels get a strict symlink-refusing
  fallback plus a loud warning — never a silent downgrade.
- **Landlock** confines every model command: RO system runtime, RW workspace
  + session tmp + own state + `/tmp`. `~/.ssh`, shell configs, sibling
  projects, and cloud credentials are simply unreachable.
- **NO_NEW_PRIVS** on every model child; setuid/sudo gains impossible.
- **Sanitized environment**: default-deny allowlist (`PATH HOME USER LANG
  TERM PWD TMPDIR ...` + `LC_*`); `*_API_KEY/*_TOKEN/*_SECRET/AWS_*/SSH_*`
  never pass. Deliberate passthrough only via user-level `expose_env`.
- **Network isolation** via seccomp (blocks `AF_INET/AF_INET6 socket()`,
  keeps `AF_UNIX`), enforced without privileges. `--network` lifts it
  explicitly for one invocation. `seccomp unavailable` is reported, not
  hidden.
- **Destructive-command guard**: last-resort screening (`rm -rf .`, `git
  reset --hard`, `git clean -f`, `mkfs`, `dd of=/dev`, fork bombs,
  `chmod -R /`, ...) → human `[y/N]` approval in the TUI, fail-closed in
  `-p` unless `--allow-destructive`. The model can never approve itself.
- **Terminal sanitization**: ESC/CSI/OSC/C0 stripped from all untrusted
  output before rendering.
- **Explicit overrides only**: `--allow-read/--allow-write PATH`,
  `--network`, `--allow-root`, `--allow-destructive`, `--unsafe`.
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
make            # build ./pocket
make test       # build + run 45 tests (functional + security)
make install    # install to ~/.local/bin
make clean
```

## Anti-goals (the constitution)

No extension/plugin API, hooks, event bus, MCP, dependency graph, councils,
swarms, subagent/workflow frameworks, embedded browser, semantic memory,
telemetry, local-ML helpers, prompt-injection systems,
capability registries, Linux-command wrappers, DI frameworks, or enterprise
ceremony. Boring function > framework; struct > hierarchy; file > service;
subprocess > plugin; Linux primitive > custom subsystem; deletion >
abstraction.

If PocketHarness starts resembling the frameworks it replaced, simplify.
