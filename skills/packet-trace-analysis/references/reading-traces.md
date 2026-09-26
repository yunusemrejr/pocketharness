# Reading traces

Examples use documentation addresses (`192.0.2.0/24`, `198.51.100.0/24`, `203.0.113.0/24`). Directions below assume the capture ran near the client; mirror the reasoning for server-side captures.

## TCP: handshake, stream, teardown

A healthy open is SYN → SYN/ACK → ACK with the handshake RTT bounding the path latency; a SYN with no reply is filtering, a dead peer, or a wrong address — check the destination and the return path before blaming the application. SYN/ACK retransmissions from the server while the client's ACKs never arrive indicate return-path loss, not server slowness.

Follow sequence and acknowledgment numbers along the stream: steady ACK advance means delivery; repeated ACKs of the same number with retransmitted segments mean loss; a shrinking advertised window means a slow or stalled receiver, while a full window with no progress means the sender or the path stopped. Distinguish retransmission (same sequence sent again — loss or timeout) from reordering (sequences arriving out of order — parallel paths) from duplication (same packet twice — mirroring artifacts or actual duplication).

Teardown readings: FIN/ACK each direction is clean; RST after data can be normal (abortive close) or a middlebox kill — check who sent it and what preceded it. An RST from an unexpected party with a plausible sequence number suggests injection or a stateful device expiring the flow.

## Loss and timing signatures

- Periodic single retransmissions with recovery: mild congestion or policing; correlate with throughput dips, not with single-packet alarms.
- Retransmission storms with exponential backoff: severe loss or a dead peer; check whether ACKs return at all before concluding which side failed.
- Long pause then resume without retransmission: application stall, not network loss — the network delivered everything, nobody sent more.
- Growing RTT with eventual loss: bufferbloat or queue buildup on the path; shrinking RTT after loss: route change or queue drain.
- TCP timestamps (when present) separate wire time from host processing delay; without them, endpoint versus network attribution stays uncertain.

Offload lies to host captures: checksum errors on outgoing packets, giant "packets" beyond MTU (TSO/GRO), and ACKs that appear before their data are capture artifacts of segmentation offload, not wire behavior. Confirm before reporting them as findings.

## DNS flows

A DNS transaction is a query and a matching response (transaction ID, question section, flags). Missing responses mean a dead resolver, filtering, or a wrong destination — verify with a direct query to a known resolver before theorizing. `SERVFAIL` points at validation (DNSSEC), resolver failure, or policy refusal; `NXDOMAIN` with a correct question means the name genuinely does not exist in that view; empty `NOERROR` answers often mean a policy sinkhole or an explicit empty zone.

Watch for truncation (`TC` bit) with TCP retry, slow retries across multiple resolvers (client-side timeout and failover, not one slow server), and search-domain suffixes expanding short names into surprising queries. Response TTLs and authority sections distinguish cached answers from authoritative ones.

## TLS handshake anatomy (without decryption)

Read the handshake for negotiation, not content: ClientHello versions and cipher/ALPN offers, ServerHello selection, certificate chain (subjects, issuers, validity, SANs — all visible), then key exchange and Finished. Common failures: no shared version or cipher (rigid client or server), unknown CA or expired certificate (trust or clock), hostname mismatch against SANs (wrong virtual host or SNI), and handshake timeout after ClientHello (filtering or a non-TLS peer).

TLS 1.3 shortens the visible exchange (encrypted certificates) — version fingerprinting from bytes alone is unreliable across 1.2 and 1.3; use the negotiated version from the handshake, not heuristics. Never claim knowledge of encrypted payloads; when the question needs application data, say so and require endpoint evidence instead.

## HTTP/2, QUIC, and path tools

HTTP/2 multiplexes streams over one TCP connection: read per-stream frames (HEADERS, DATA, RST_STREAM, WINDOW_UPDATE) rather than connection-level bytes. A stalled stream with a healthy connection is flow control or application state. QUIC moves to UDP with connection migration and its own loss recovery — path tools that assume TCP (and captures that assume stable five-tuples) mislead; check connection IDs, not just addresses and ports.

For `traceroute`/`mtr` output: read per-hop RTT as router-response time, not forwarding time — a slow hop with fast later hops is ICMP deprioritization, not congestion. Persistent loss starting at one hop and continuing downstream implicates that hop; loss at one hop that clears downstream is that router's control-plane policing. Load-balanced paths produce per-probe route flapping; Paris-style or flow-pinned tracing stabilizes the picture.

## Primary references

Check the documentation for the installed version when behavior matters. These are reference entry points, not permission to capture.

- https://www.wireshark.org/docs/wsug_html_chunked/
- https://www.wireshark.org/docs/man-pages/tshark.html
- https://www.tcpdump.org/manpages/tcpdump.1.html
