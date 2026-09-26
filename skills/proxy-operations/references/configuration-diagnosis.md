# Proxy configuration and diagnosis

## Identify both connections

Separate client-to-proxy transport from proxy-to-destination transport. An HTTP proxy may tunnel HTTPS using CONNECT; an HTTPS proxy additionally secures the connection to the proxy. SOCKS is a different protocol. A destination's certificate failure and a proxy's certificate failure belong to different trust relationships. Keep verification enabled and examine the failing peer and configured trust store.

For curl, a non-secret example is `curl --proxy socks5h://127.0.0.1:1080 --connect-timeout 5 --max-time 15 https://example.com/`. `socks5h` delegates destination hostname resolution to the proxy; ordinary `socks5` resolves locally. Confirm version-specific behavior with installed help and the [curl manual](https://curl.se/docs/manpage.html). This example illustrates configuration, not an instruction to contact a particular service during unrelated work.

Inventory existing per-process settings before changing them. Proxy environment-variable handling varies by client, including casing and bypass-list interpretation. Prefer a narrowly scoped command or browser context over shell-profile or system-wide changes. Document local-address exclusions deliberately; a bypass list that is too broad can send traffic directly when the task expected proxy routing. Do not assume browser traffic, command-line requests and application SDK calls honor the same variables.

## Handle credentials without spreading them

Use the task's existing secret mechanism. A credential-bearing URL can appear in process listings, shell history, debug output and error messages. Prefer a supported protected configuration file or secret-backed client option when available; remove temporary material after its purpose ends. Redact proxy authorization headers and URLs before publishing logs. A verbose network trace should be treated as potentially sensitive even when the request body is empty.

## Diagnose one layer at a time

First establish whether the local proxy endpoint is reachable. Then distinguish authentication failure, DNS failure, tunnel rejection, destination TLS failure, destination HTTP error and response-body mismatch. Compare with a direct request only when direct access is within scope; it may intentionally be unavailable. Use identical method, payload and destination for meaningful comparisons.

A 200 status from an IP-check page proves that particular request reached that page through some route; it does not prove that every protocol, hostname or future request uses the proxy. Verify the actual application path and DNS policy separately. Do not log a user's private destinations to a third-party diagnostic endpoint merely to test connectivity.

Browser automation should configure the proxy through the supported browser or context option, then verify routing inside that context. Playwright supports proxy configuration at browser or context scope; see its [network documentation](https://playwright.dev/docs/network). Preserve unrelated authenticated contexts.

Bound retries, request count and diagnostic duration. If changing the proxy makes the failure disappear, retain the original evidence and investigate why; do not silently mark destination correctness as proven.
