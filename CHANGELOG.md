# Changelog

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
