---
name: bash-workflows
description: Discover, implement, validate and deliver Bash scripts with explicit exit behavior, quoted paths, bounded subprocesses and reliable cleanup.
---

# Bash workflows

Inspect the actual shebang, caller, working directory, supported Bash version and external commands. Reuse the project's existing launcher and validation scripts. A POSIX `sh` script cannot assume Bash arrays or `[[ ... ]]`.

Treat command text as code. Quote expansions and pass data as arguments or standard input; never interpolate untrusted input into `eval`, `bash -c` or command substitutions. `JSON.stringify` is not shell quoting. Use arrays for argument lists in Bash, `--` where the command supports it, and collision-safe `mktemp` paths. Do not repurpose `HOME` or other runtime environment variables for scratch paths.

Define the intended exit contract. `set -e` has conditional/pipeline exceptions; handle expected failures explicitly and use `pipefail` only when supported and its behavior is wanted. Capture a subprocess's status before cleanup overwrites it. Pair resources with `trap`, preserve an original failure, and avoid deleting paths not created or owned by this run. Set finite limits for retries, background jobs and generated output.

Validate with `bash -n`, available ShellCheck, and a real bounded run in a temporary fixture: a path containing spaces, an empty value and a required command failing. Check cancellation/cleanup when the script owns long-running work. Deliver the executable, its dependency/working-directory contract and the observed validation. Reuse unchanged checks and follow the common [project workflow](../project-workflows/SKILL.md) for authorized Git/installation delivery.
