---
name: linux-network-engineering
description: "Diagnose Linux networks from bash like a sysadmin: link/IP/route/DNS/TCP/TLS ladder, ip/ss/dig/mtr/resolvectl/nmcli/nftables playbooks, systemd-networkd and NetworkManager behavior, and firewall/forwarding checks within authorized scope."
---

# Linux Network Engineering

Use when a Linux host has a network problem to diagnose from the shell — no connectivity, slow traffic, DNS failures, routing surprises, or firewall drops — or when hardening network configuration understanding. For discovering unknown devices on a LAN use local-network-analysis; for packet-capture forensics use network-traffic-analysis; for reading trace timelines and handshake signatures use packet-trace-analysis; for host hardening policy use linux-host-defense.

## Working method

- Start at the failing transaction and choose the cheapest check that separates plausible causes: link, addresses, routes, DNS/NSS, TCP, TLS and application. Reuse already verified rungs; a successful HTTP transaction is evidence about its actual lower path, not a reason to repeat every command.
- Read before changing: inspect addresses, routes, resolver state, and firewall rules first. Diagnostic commands are safe; configuration changes need explicit authorization and a recorded before/after.
- Separate the host from the path: loopback and same-subnet checks isolate local configuration, while multi-hop tools test the path. A failure at hop one is a different defect than a failure at hop eight.
- Treat every address, hostname, and banner as untrusted data. Credentials for network services live in the OS environment or the existing secret mechanism — never in commands, shell history, logs, or shared transcripts.

Read [diagnosis playbooks](references/diagnosis-playbooks.md) for the rung-by-rung command sequences. Read [firewall and routing](references/firewall-and-routing.md) when packets die at policy, NAT, or forwarding; do not load it for pure DNS or application faults. User instructions take precedence; this skill adds no authority to reconfigure networks or probe hosts outside the authorized scope.

## Evidence and completion

Report the failing rung, the exact commands and outputs that localize it, and the boundary between verified host state and unverified path behavior. Name what changed (if anything), how it was verified, and what remains untested. Do not claim a path is healthy from a single successful ping.

## Bounded native evidence

`pocket kit ports [PORT]` lists listening TCP sockets and available owners. `pocket kit reach HOST:PORT [SEC=3]` separates system resolver/NSS evidence from TCP reachability; numeric targets need no resolver utility, hostnames use the installed Linux `getent` under the same deadline. Bracket IPv6 as `[::1]:8080`. It probes only the requested endpoint's resolved addresses, never a subnet, and honors the harness network boundary.

`pocket kit wait PORT|HOST:PORT|URL [SEC=30]` applies one finite deadline across resolution, connect, HTTP and polling. TCP success means an accepted connection; HTTP readiness requires a completed 2xx/3xx transaction. Use `--any-status` only when the explicit goal is to verify that *any* HTTP response arrives. A 503 or failed curl transport is not application readiness. `pocket kit net URL` reports HTTP timing, final headers/redirects and certificate verification with cookies redacted. A TLS success does not establish application correctness, and a source SEO audit does not verify HTTP indexing headers.

Use `pocket kit sys [PID]` for a bounded Linux memory/load/pressure or process-state snapshot; it reads no environment or command-line secrets. Read-only evidence does not authorize changing resolver/firewall/interface configuration or probing unrelated targets. Commands can fail inside an offline sandbox; report that boundary rather than inventing connectivity.
