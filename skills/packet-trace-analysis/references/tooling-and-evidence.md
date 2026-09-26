# Tooling and evidence

## tshark-first workflow

Prefer `tshark` for reproducible, scriptable analysis and Wireshark or termshark for interactive exploration of the same file. Keep the raw capture immutable: hash it on receipt, work on copies or with read-only flags, and cite packet numbers, stream indices, and absolute timestamps so another analyst can reproduce every claim.

Useful starting patterns (capture file `trace.pcapng`, documentation addresses):

```text
tshark -r trace.pcapng -q -z io,stat,1,"COUNT(tcp) tcp","COUNT(dns) dns"
tshark -r trace.pcapng -Y "tcp.flags.syn==1 && tcp.flags.ack==0" -T fields -e frame.number -e ip.src -e tcp.dstport -e frame.time_epoch
tshark -r trace.pcapng -q -z follow,tcp,ascii,0
```

The first summarizes traffic mix over time, the second lists SYNs with epoch timestamps, the third follows stream zero. Replace display filters (`-Y`) per question; never confuse them with capture filters, which discard traffic at collection and cannot be undone afterward. Verify filter syntax against the installed version — field names change across releases.

## Streams, reassembly, and statistics

Follow streams (`-z follow,tcp,<n>`) to read transactions, and use `-z io,stat` plus `-z tcp,calc` style analyses (exact option names vary by version — check installed help) for retransmission counts and window behavior. For DNS, aggregate query/response pairs by transaction ID rather than eyeballing packets. For TLS, extract handshake fields (versions, SNI, certificate subjects) as columns before drawing conclusions.

Statistics describe the capture, not the network: drops during collection, snaplen truncation, and sampling all bias results. Quote the capture's own health (interface drops, kernel drops, duration, packet counts) alongside every measurement.

## Evidence format

Write findings as: transaction timeline (time-ordered, with packet/stream references), measured indicators (handshake RTT, retransmission counts, window minima, DNS latency), the surviving explanation, ruled-out alternatives with the evidence that ruled them out, and one bounded next check. Reference packets by number and streams by index; include the capture hash and the exact filter expressions used.

Distinguish confidence levels explicitly: observed bytes, inferred mechanism, and hypothesized cause are three different claims. A pattern consistent with policing is not proof of policing — name the measurement that would confirm it.

## Encrypted and sensitive traffic

Encryption bounds the analysis: with TLS 1.2+ or QUIC, handshake metadata and timing are visible but payloads are not. Session keys (from controlled test endpoints with explicit authorization) enable decryption for diagnosis; key material is a credential — it lives in the OS environment or the existing secret mechanism, is used ephemerally, and never lands in files, logs, or shared artifacts.

Captures routinely contain credentials, session tokens, personal data, and internal hostnames. Extract only the fields the question needs, redact aggressively in shared findings, and never upload raw captures outside the authorized scope. When findings must travel, export the minimal field set rather than the capture.

## Primary references

- https://www.wireshark.org/docs/man-pages/tshark.html
- https://www.wireshark.org/docs/man-pages/wireshark-filter.html
- https://www.tcpdump.org/manpages/pcap-filter.7.html
