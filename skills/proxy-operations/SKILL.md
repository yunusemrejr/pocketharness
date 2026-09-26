---
name: proxy-operations
description: "Configure and diagnose HTTP, HTTPS or SOCKS proxies and evaluate legitimate proxy providers for authorized traffic; cover DNS, TLS, credentials, routing and bounded measurement."
---

# Proxy operations

Identify the client's protocol, destination, authentication mode and desired routing scope before changing settings. A proxy URL is not interchangeable with an HTTP API endpoint, VPN or browser extension. Prefer per-command or per-context configuration so unrelated traffic keeps its existing route.

Read [configuration and diagnosis](references/configuration-diagnosis.md) for curl/browser setup, DNS location, CONNECT, credentials and failure isolation. Read [provider evaluation](references/provider-evaluation.md) when finding or comparing services, checking IP sourcing, retention, pricing and measured reliability. Refresh provider facts from current first-party material; do not maintain an evergreen list of supposedly best or free proxies.

Use endpoints the task is authorized to contact. Confirm DNS behavior and exit route using a controlled endpoint before routing sensitive data. Keep TLS certificate verification active; identify which connection failed rather than disabling trust checks. Credentials belong in the existing secret mechanism, not examples, public logs, command history or source control. Sanitize diagnostic artifacts before sharing them.

Compare direct and proxied requests with matched payloads and bounded timeouts. Separate destination errors from proxy authentication, name resolution and tunnel failures. Change one variable at a time and retain a reversible configuration diff. Report the actual client configuration, verified route, remaining limitations and evidence for any provider recommendation. Proxy availability does not authorize bypassing access controls, expanding request volume or changing account permissions.
