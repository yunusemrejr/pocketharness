# Patterns and chains

## Choosing the proxy type per job

HTTP proxies understand requests and can cache, filter, or require authentication per origin; HTTPS through them uses CONNECT tunneling, which hides content but reveals destination host and port to the proxy. SOCKS (especially SOCKS5 with remote DNS, `socks5h` in curl) forwards TCP (and optionally UDP) without interpreting the application protocol — choose it when the traffic is not plain HTTP or when local DNS resolution would leak intent. Neither type encrypts traffic to the destination by itself; TLS still ends at the origin server.

Match the type to the question: content-aware egress policy points at HTTP proxies; protocol-agnostic forwarding points at SOCKS; full-device capture points at VPN or transparent routing instead — a proxy is the wrong tool for traffic its client cannot be configured to send through it.

## Chaining

Chains layer proxies so each hop sees only its neighbors. Build them explicitly hop by hop (client → proxy A → proxy B → destination), verifying each hop's exit and DNS behavior before adding the next. Every hop adds latency, failure modes, and a party that observes metadata; a chain is only as trustworthy as its least trustworthy hop, and longer chains are harder to debug, not more secure by default.

Keep chain configuration in one reviewable place with per-hop purpose, credentials via the secret mechanism, and a diagram of what each hop can observe. Test chain failure by killing each hop in turn: the client must fail closed (no direct fallback) when the policy requires it.

## PAC files

Proxy auto-config scripts route per URL. Write them as small pure functions over host and URL, keep a default of `DIRECT` or the intended proxy explicit (never an accidental fallthrough), and test with the actual engine — PAC dialects differ across browsers and libraries (`myIpAddress`, `dnsResolve`, and time functions all have quirks). Log the routing decision during testing, then remove decision logging for production since URLs can carry sensitive query strings.

Serve PAC over a trustworthy channel and pin its source; a PAC file controls all matching traffic, so tampering with it reroutes the victim. Version it and keep the previous known-good copy for instant rollback.

## SSH tunnels as proxies

`ssh -D` (dynamic/SOCKS), `-L` (local forward), and `-R` (remote forward) turn an SSH server into a proxy or a tunnel endpoint. They authenticate with the existing SSH credentials (agent or keys from their normal locations — never pasted into commands or files), encrypt to the SSH server, and exit with the server's address. Verify the exit the same way as any proxy, and remember the SSH server observes metadata exactly as a proxy would.

Remote forwards (`-R`) expose local ports through the server — bind them to loopback unless wide exposure is explicitly intended and authorized. Tunnels die with the SSH connection; use autossh-style supervision or systemd units only where persistent tunnels are an approved design, and document who may traverse them.

## Transparent interception (authorized scope only)

Transparent or intercepting proxies capture traffic without client configuration — typically at a gateway with firewall redirection. This is legitimate for authorized egress control, malware analysis sandboxes, or managed-device policy, and illegitimate anywhere else. TLS interception additionally requires a client-trusted CA and explicit authorization; it breaks certificate pinning, changes the security properties of every intercepted connection, and creates a high-value key to protect.

Never deploy interception on networks or devices outside the authorized scope, and never present an interception capability as a monitoring solution without the legal and policy review it requires. Document the scope boundary in the same change that documents the configuration.

## Primary references

Check the documentation for the installed version when behavior matters. These are reference entry points, not permission to deploy.

- https://curl.se/docs/tutorial.html
- https://www.openssh.com/manual.html
- https://developer.mozilla.org/en-US/docs/Web/HTTP/Proxy_servers_and_tunneling
