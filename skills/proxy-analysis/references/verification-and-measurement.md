# Verification and measurement

## Exit verification

Confirm the exit address through a controlled endpoint that echoes the observed source IP — run by you or explicitly trusted for this check. Verify separately for IPv4 and IPv6: a proxy that handles only IPv4 while the client prefers IPv6 leaks every connection. Disable or route IPv6 deliberately; an unconsidered IPv6 path is the most common proxy bypass.

Repeat the check per application and per protocol. Browser, curl, package manager, and background services each have their own proxy settings; one verified client says nothing about the others. System-wide environment variables (`http_proxy`, `https_proxy`, `all_proxy`, `no_proxy`) are honored inconsistently — audit each client instead of assuming inheritance.

## DNS path verification

DNS must follow the intended path: through the proxy (SOCKS5 remote resolution, DNS-over-HTTPS to a chosen resolver) or to an approved local resolver — never accidentally to a network observer. Test with a name only the intended resolver can answer, plus a public name, and inspect which resolver actually responded (response source, not just content).

Watch for split behavior: `nsswitch`, systemd-resolved per-link DNS, VPN-pushed resolvers, and browser secure-DNS settings can each override the assumed path for different queries. `resolvectl status` and per-query inspection beat assumptions. DNS query names themselves are sensitive data; treat resolver logs accordingly.

## WebRTC, QUIC, and other bypasses

WebRTC can discover local and public addresses via STUN independent of the HTTP proxy configuration; test with the actual browser profile and its real settings, since extensions and flags change the behavior. QUIC/HTTP-3 runs over UDP, which many HTTP proxies do not forward — clients may fall back to TCP or bypass the proxy depending on configuration. Verify UDP handling explicitly rather than assuming the TCP test covers it.

Application-level bypasses (hardcoded IPs, custom DNS, bundled certificates, telemetry endpoints with their own transports) defeat proxy policy silently. Inventory the application's actual connections during a test run and account for each one.

## Performance comparison

Compare routes with matched payloads, matched timing, and bounded timeouts: same URL or endpoint, same payload size, alternating direct and proxied requests to cancel out time-of-day effects. Measure connect time, time-to-first-byte, and total time separately — a proxy serially adds a handshake and a hop, so connect latency and throughput tell different stories.

Change one variable at a time (proxy, protocol, payload, concurrency) and retain the raw numbers with sample counts. A single fast request proves nothing; report medians and spread over a bounded run. Do not benchmark against third parties without authorization, and keep request volume minimal — measurement is not load testing.

## Anonymity limits

A proxy changes the observed source address; it does not anonymize. Browser fingerprints, login sessions, TLS fingerprints, traffic timing, payment identity, and the proxy provider's own logs all re-identify. Never promise anonymity, and never use anonymity-adjacent language ("untraceable", "invisible") in findings. State precisely which observer learns what under the verified configuration, and which observers remain unaddressed.

Redact proxy credentials, authorization headers, full URLs with tokens, and client identifiers from every artifact. Treat verbose traces as sensitive even with empty bodies, and keep measurement data inside the authorized scope.

## Primary references

- https://curl.se/docs/manpage.html
- https://www.wireshark.org/docs/man-pages/wireshark-filter.html
- https://developer.mozilla.org/en-US/docs/Web/HTTP/Proxy_servers_and_tunneling
