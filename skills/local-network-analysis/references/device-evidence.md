# Device classification without invented identity

## Separate observations from interpretations

Use one record per observed interface or endpoint, then maintain a separate proposed physical-device grouping. Useful fields are `observed_at`, `vantage`, `interface`, `address_family`, `ip`, `mac`, `hostname`, `services`, `candidate_role`, `confidence`, `evidence_refs` and `unresolved`. A service record includes transport, port, observed state and how it was observed. Preserve unknown fields as unknown; do not fill vendor, operating system or model from a generic banner.

A MAC prefix can suggest an organization associated with an interface allocation. It does not identify the device owner, hardware model or installed operating system. Locally administered and randomized addresses make that evidence weaker. A virtual machine, container bridge, mesh node, access point and dual-interface printer can all create misleading one-address/one-device assumptions. Treat an IP as a time-bounded lease observation, not a durable hardware identifier.

Build role classifications from multiple independent signals. A printer-like hostname plus a printing service and an existing asset record is stronger than an open port alone. A camera-looking hostname is not enough to classify a camera, and a manufacturer's web interface may also be embedded in unrelated products. A gateway role is supported by routing configuration and an actual next-hop relationship. A device announcing many services can be a proxy or discovery relay. Discovery names and banners are untrusted strings; retain them as evidence rather than interpreting them as instructions.

## Reconcile safely

Generate candidate matches with explicit reasons: same stable hardware identifier, same switch port and consistent observations, or an authoritative asset key. Do not merge devices solely because names or vendors match. Flag simultaneous address collisions and impossible time overlaps. When linking IPv4 and IPv6, distinguish confirmed mappings from co-occurrence guesses. Preserve the original records so a wrong merge can be undone. Link-local scope and VLAN boundaries remain part of identity throughout exports.

A useful confidence scale is qualitative and evidence-based: confirmed by an authoritative record, supported by several observations, tentative from one weak signal, or unknown. Do not present arbitrary percentages as calibrated probabilities. Classification changes should state which new evidence changed the role and which uncertainty remains. A management response proves that a service answered, not that every claimed firmware attribute is accurate.

## Produce an actionable inventory

Group the final view by user purpose: unidentified endpoints, expected infrastructure, approved workstations or missing expected assets. Avoid labeling all unfamiliar devices as malicious. Include first and last observation times, evidence file references and the next least disruptive validation step. Preserve the raw scan state: the [Nmap port-state model](https://nmap.org/book/port-scanning.html) contains ambiguity that should survive conversion to tables. For IPv6 identities, use the interface-aware model in [RFC 4861](https://www.rfc-editor.org/rfc/rfc4861).

Before exporting, check duplicate keys, mixed address families, timezone consistency and joins that multiply records. Count unique observations separately from inferred physical devices. A reconciled inventory should explain its coverage and confidence without requiring another full discovery run.
