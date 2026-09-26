# Capture quality before packet interpretation

## Preserve the acquisition context

Record host, interface, link type, capture point, start/end times, clock source, snap length, filter, tool version and packet-drop counters. A capture on a host sees traffic at a different point from a switch mirror or passive tap. Some VLAN tags, encapsulation headers and checksums can be affected by offload or the capture path. A file with no malformed packets is not automatically complete. Preserve the original file and compute a content hash before extracting subsets.

Inspect metadata and a small packet sample before loading a huge capture into an interactive application. `capinfos capture.pcapng` can summarize an existing file; `tshark -r capture.pcapng -q -z io,phs` produces a protocol hierarchy. These examples are read-only. Choose further fields from the installed dissector version rather than assuming a field name exists. The [TShark manual](https://www.wireshark.org/docs/man-pages/tshark.html) documents file analysis, field extraction and capture controls.

## Bound new collection

For a live task, select only an authorized interface and a traffic filter sufficient to answer the question. Set both a duration and a storage limit or bounded ring buffer. Avoid a broad indefinite capture, especially on shared systems. Full payload capture may be necessary for a protocol problem but should not be the default for a timing-only question. A short snap length saves storage but may prevent later application decoding; make that tradeoff explicit before collection.

Capture filters use libpcap syntax, such as `host 192.0.2.10 and tcp port 443`; the documentation address is an example, not a target. Display filters use Wireshark expressions, such as `ip.addr == 192.0.2.10 && tcp.port == 443`. Capture filtering discards unmatched traffic during collection; display filtering selects among packets already saved. The [Wireshark filter reference](https://www.wireshark.org/docs/man-pages/wireshark-filter.html) explains this distinction. A display filter's empty result does not establish that an application never acted, particularly if collection already excluded relevant traffic.

## Transform without losing evidence

For exported tables, retain frame number, timestamp, direction, length, transport tuple and the fields used in the conclusion. Quote CSV correctly because protocol fields may contain commas, newlines or attacker-controlled strings. Include capture hash and filter expression with the export. A packet count, byte count and stream count answer different questions; specify whether lengths include link headers or only payload. Avoid mixing retransmitted bytes into application goodput without saying so.

Check clock discontinuities, reordered packets, duplicate mirrored frames and missing directions. Packet loss inside the capture tool differs from network loss. If timestamps from multiple sources are aligned, record the offset method and uncertainty. Keep a focused evidence extract beside the original, with access and retention matching the sensitivity of captured payloads. Reproducing the analysis should require the saved capture and filter, not a fresh live capture.
