---
name: local-webapp-workflows
description: Build and verify local webapps through their actual server, browser, persistence and restart lifecycle while keeping process ownership and delivery explicit.
---

# Local webapp workflows

Discover the stack, entrypoint, configuration/storage, existing process and actual port owner before launching anything. Use installed versions and the language/framework guide. Bind to loopback by default. Reuse a user-owned server when it is the correct project; do not kill unrelated processes to free a port.

Implement through the existing backend/frontend owners, with secrets on the server and a defined API/error contract. Preserve user data; migration and reset behavior must be explicit. UI work gets `ai-design-slop` and actual input/focus/empty/error checks, preserving the project's established identity.

Start through the project's real launcher, with an identifiable process/session and shutdown owner. Inspect startup output, then wait for a successful HTTP response instead of sleeping a fixed interval. Available `pocket kit port`/`wait`/`http` can help; inspect current help for syntax. Readiness includes the required dependency/data state, not merely a listening socket.

Verify the main journey in an available real browser, its failure path, persistence after restart, nested-route refresh and browser console/network behavior. When browser access is unavailable, perform HTTP/backend/static checks and state the remaining interaction gap. A screenshot is evidence of appearance only.

Deliver working launch/stop commands, the actual URL and validated persistence/runtime requirements. Verify the installed or packaged startup when requested. Keep the server running only when it is the requested result, and report its lifecycle. Use [project workflows](../project-workflows/SKILL.md) for phased execution and [shared hosting deployment](../shared-hosting-deployment/SKILL.md) when delivery moves to a remote host.
