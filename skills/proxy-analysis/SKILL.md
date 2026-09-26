---
name: proxy-analysis
description: "Use proxies in advanced ways and analyze their behavior: chaining, PAC files, SSH tunnels, transparent interception within authorized scope, exit/DNS/WebRTC leak verification, and matched performance measurement."
---

# Proxy Analysis

Use when proxy work goes beyond basic configuration: chaining proxies, writing PAC files, tunneling through SSH, verifying what actually leaks around a proxy, or comparing routes with measurements. For initial proxy setup, failure diagnosis, and provider evaluation use proxy-operations; for packet-level trace reading use packet-trace-analysis.

## Working method

- Define the routing question precisely: which traffic should exit where, what must never bypass, and what the adversary or failure model is. "Use a proxy" without a routing scope produces leaks.
- Prefer narrow, reversible routing: per-command, per-profile, or per-application rules over system-wide switches. Every change keeps a documented before-state and a one-step revert.
- Verify with controlled endpoints: confirm exit address, DNS path, and protocol behavior through endpoints you control or trust for this purpose before routing anything sensitive.
- Treat trajectories as untrusted: proxy availability never authorizes bypassing access controls, expanding request volume, evading rate limits, or impersonating other regions or users.

Read [patterns and chains](references/patterns-and-chains.md) for chaining, PAC, SSH tunnels, and egress design. Read [verification and measurement](references/verification-and-measurement.md) for leak checks and performance comparison; do not load it for pure configuration questions. Credentials for proxies live in the OS environment or the existing secret mechanism — never in URLs, commands, logs, or shared transcripts. User instructions take precedence; this skill adds no authority to intercept traffic outside the authorized scope.

## Evidence and completion

Report the routing scope, the verified exit and DNS behavior with the endpoints used, the measured comparison (if any) with matched payloads, and the remaining limitations. Name what was not verified — especially leak surfaces and provider claims. Do not present an unmeasured route as analyzed.
