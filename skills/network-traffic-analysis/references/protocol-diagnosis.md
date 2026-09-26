# Diagnose transactions, not isolated red packets

## Reconstruct a causal timeline

Pick a failing transaction using a known time, endpoint or request identifier. Follow its DNS resolution, connection establishment, encryption handshake and application exchange where visible. Associate retransmissions and retries with that transaction rather than counting all warnings in a file. Record packet numbers for each event and distinguish observed time from inferred application timing. DNS over HTTPS, proxies and connection reuse may hide or relocate familiar stages.

For TCP, examine handshake completion, sequence progression, acknowledgments, advertised windows, resets and close behavior. A retransmission label is a dissector interpretation, not direct proof of physical packet loss. Reordering, a missing capture direction or duplicate mirror traffic can resemble a transport problem. A zero receive window suggests receiver-side flow control; it is different from sender congestion control. A reset identifies an abrupt connection event but does not by itself prove which application component caused it.

Use relative timing carefully. The delay between a request and first response byte includes processing and potentially network time; it is not a pure RTT. An ACK-based RTT estimate depends on visibility, delayed acknowledgments and retransmission ambiguity. Compare distributions across comparable transactions rather than averaging unrelated connections. Throughput in bits per second is `8 * observed_bytes / interval_seconds`, but define the byte layer and interval. Application goodput excludes protocol overhead and duplicate payload transmissions.

## DNS, TLS and application boundaries

For DNS, pair requests and responses with endpoint, identifier and question, accounting for retries and cached answers. Distinguish no response in the capture, negative answers, truncation and resolver errors. A successful lookup does not establish reachability of the returned address. For TLS, report only visible versions, handshake messages, alerts and timing. Do not infer request content from encrypted record sizes. Decryption requires legitimately available session secrets and explicit data-handling scope; do not collect credentials to compensate for insufficient capture evidence.

Endpoint capture may show invalid outbound checksums because hardware computes them after the capture point. Large segmentation-offload buffers can also look unlike packets on the wire. Compare with a network-side capture or endpoint counters before diagnosing malformed transmission. The [Wireshark User's Guide](https://www.wireshark.org/docs/wsug_html/) describes capture interpretation and protocol analysis; the [display-field index](https://www.wireshark.org/docs/dfref/index) is the authority for version-specific fields.

## Make conclusions falsifiable

State a finding as a bounded observation: the client retransmitted a SYN three times with no reply visible at this capture point. Then list hypotheses, such as filtering, unreachable service or missing return-path visibility. Select the smallest next check that distinguishes them, such as an authorized server-side log or simultaneous short capture. Keep resolved and unresolved transactions separate. A useful result explains what the evidence supports, what remains unknown and exactly how a second analyst can verify the packet-level claim.
