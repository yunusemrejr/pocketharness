---
name: local-network-analysis
description: Inventory an authorized local network, plan bounded host and service discovery, and classify devices with explicit evidence and uncertainty.
---

Turn a local-network question into a reproducible inventory that distinguishes observed endpoints from inferred device roles. Start with the user's subnet, interface, site and purpose; retain authorization already provided. Do not infer ownership or permission merely from a private address. When scope is incomplete, inspect local configuration and existing artifacts while resolving the missing boundary.

Prefer existing router leases, switch tables, neighbor caches and asset records before transmitting discovery probes. Record the observation time and network vantage point: a laptop behind a router cannot inventory an entire enterprise from its neighbor table. Cover IPv4 and IPv6 explicitly, without attempting to enumerate an IPv6 address space.

Use [discovery-and-scope](references/discovery-and-scope.md) for interface selection, passive records, bounded Nmap discovery and limitations of negative results. Use [device-evidence](references/device-evidence.md) for role classification, identity reconciliation and an evidence-backed inventory schema.

Choose the least disruptive observation that answers the question. Discovery, service probes and vulnerability tests are different operations; do not silently escalate among them. For fragile embedded or industrial devices, use existing passive records or a supported management interface unless active testing is already authorized and appropriate.

Deliver the inventory, confidence per attribute, coverage gaps and concrete follow-up actions. Preserve raw evidence paths so another agent can verify classifications without repeating the scan.
