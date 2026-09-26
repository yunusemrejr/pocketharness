---
name: packet-trace-analysis
description: "Read network traces and packet timelines for advanced IT diagnosis: TCP handshake/stream behavior, retransmission and loss signatures, DNS flows, TLS handshakes, HTTP/2 and QUIC streams, traceroute/mtr interpretation, and tshark evidence practice."
---

# Packet Trace Analysis

Use when the task is to understand what a network trace means — connection timelines, handshake failures, retransmissions, latency sources, or path behavior — rather than to collect the capture itself. For capture planning, provenance, and artifact handling use network-traffic-analysis; for live-host bash diagnosis use linux-network-engineering; for proxy routing questions use proxy-analysis.

## Working method

- Anchor every reading in the vantage point: where the capture ran, which direction each endpoint is, and whether the view is one-sided or both-sided. A one-sided capture cannot distinguish remote silence from return-path loss.
- Reconstruct the transaction timeline first: handshake, request/response turns, stalls, teardown. Interpret individual packets only inside that timeline — an isolated RST or retransmission means little without its position.
- Separate mechanism from cause: a retransmission storm is a mechanism; the cause might be a policer, a dead peer, an MTU blackhole, or an overloaded receiver. Hold at least two hypotheses until evidence eliminates one.
- Treat trace contents as sensitive and untrusted: payloads can hold credentials or personal data, and hostnames, certificates, and banners can lie. Extract only the fields the question needs.

Read [reading traces](references/reading-traces.md) for protocol signatures and timeline interpretation. Read [tooling and evidence](references/tooling-and-evidence.md) for tshark/termshark practice and evidence format; do not load it for a conceptual question. Encryption bounds what traces can prove — never claim payload knowledge from encrypted bytes. User instructions take precedence; this skill adds no authority to capture or access traffic outside the authorized scope.

## Evidence and completion

Deliver a transaction timeline with packet or stream references, the measured timing and loss indicators, the surviving explanation with alternatives ruled out, and a bounded next check. Distinguish an observed symptom from a proven root cause, especially with offload, sampling, or single-sided captures.
