# Native execution and reusable workflows

Pocket Harness retains one C++ loop and five tools. It has no YunusPi runtime,
plugin layer, todo service or agent framework. Native task policy and existing
Linux recursion provide the equivalents that fit this architecture.

`thinking: adaptive` allocates low effort to a simple task, medium to uncertain
work, and high to broad or consequential work. Tool observations reassess that
choice: actual process/tool failures raise effort; edits return work to the main model; growing
changes use broader review; healthy discovery avoids periodic local observer
calls. Simple safe work may use the configured `fast` role when the model was
chosen implicitly. Explicit `-m`, `/model`, `/thinking` and resumed session
choices stay authoritative. Images require the main model. A fast-role failure
returns to main; main-provider failure retains the existing fallback policy.
The fast role for execution and advisory planning must also pass the native health/latency history gate: observed
unreliable or substantially slower routing stays on main. Trivial direct answers
skip completion judgments; an implementation request without workspace work
still receives a completion check.

The original request establishes the risk floor. Successful read subtasks use
low reasoning; implementation uses medium; observed failures use high.
Configured reviewer rosters supply one reviewer for small changes and up to
three for complex/risky work. An objection followed by edits invalidates review
and lint state and permits one fresh review pass. Unchanged work reuses passed
stop hooks. Local observers address repeated failure or stalled progress with
bounded calls; Jev/Span retain ambiguity, output-quality and goal-audit duties.
Goals still need deliverables, passing completion hooks and audit evidence.

Independent specialists can use the existing recursive `pocket -p` path and
Linux background jobs. The workflow contract requires one writer per overlapping
file and allows concurrency only when independent work saves more time than
handoff/context costs. A subtask or todo reuses completed discovery, checks and
artifacts; changed dependencies invalidate the affected evidence. This is
execution guidance in the specialist workflows, rather than a new todo database
or an automatic swarm launcher. Actual tool mutations remain ordered.

## Inspect selection without a model call

```sh
pocket --workflow 'Fix the title in README.md'
pocket --workflow 'Build a PHP 8 website and deploy through Git/SSH to Namecheap'
pocket --workflow 'Fine-tune a language model with QLoRA in Google Colab'
pocket --workflow 'Edit sound and mix music with a video animation'
```

This prints JSON with the native complexity, adaptive reasoning, planning and
reviewer budget, UI/video flags and available guides. It does not create a
session, call a provider or prove the workflow has executed.

Explicit skill names and deterministic task domains precede bounded root
manifest evidence. Recommendations only name installed skills and skip guides
already loaded in that session, including restored sessions. Project skills
continue to override bundled/global skills. Jev evaluates ambiguous substantial
requests in one batch. UI and video mutation gates still require their doctrines.

The bundled `project-workflows` guide defines discovery, implementation,
validation and delivery with per-stack recipes. Specialist guides cover PHP 8+,
Node.js, vanilla JS, React CDN/Node, Go, Rust, Java, Python/Flask, Bash, C/C++,
Linux desktop applications, local webapps, algorithms/ML and local/Colab training.
Hosting guidance discovers actual Namecheap/GoDaddy Git/SSH and GitHub integration
before using it; provider names do not establish account capabilities. No hosting
account or training job was created by this release.

Design review preserves existing identity. Generic gradient/card heroes,
cream-and-cursive layouts, fake content and SaaS section templates are concrete
cues to inspect, rather than a ban on intentional colors, fonts or components.
Verify rendering and interaction after meaningful changes; repeat review only
when the artifact changes or evidence remains unresolved.

## Build, package and release

```sh
make -j4 test
make check-workflows
make bench
./scripts/test-release.sh
# After updating kVersion and CHANGELOG and committing verified changes:
git tag -a vVERSION -m 'Pocket Harness VERSION'
git push origin main vVERSION
```

The release workflow runs the reusable GCC, Clang and sanitizer jobs before
building and publishing a version-matched binary/skills archive plus SHA256.
The release archive supports the Linux architecture named in its filename;
source builds remain the portable path to other Linux systems. Binary and source
installation share a staged installer, replace bundled program data and preserve
personal skills, configuration and sessions. Verify the checksum before unpacking,
then run the archive's `install.sh`; `PREFIX` selects the binary directory while
bundled data follows Pocket's established home data root.

The version, installed routing, a real media command and binary checksum establish
that a local installation contains the released changes. A version string alone
is insufficient. See the benchmark report for measured scope and limitations.
