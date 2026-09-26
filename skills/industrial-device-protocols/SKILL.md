---
name: industrial-device-protocols
description: Identify industrial device interfaces and decode Modbus, OPC UA, MQTT, CAN and EtherCAT wire contracts using vendor mappings, units, timestamps and quality flags.
---

# Industrial device protocols

Use for interpreting captured traffic, building a device adapter, mapping telemetry or diagnosing protocol and data-type mismatches. Establish device model, firmware, interface, topology, transport, address scheme and the exact vendor register or object dictionary. A familiar port or connector does not establish the application protocol or command semantics.

Read [protocol identification](references/protocol-identification.md) for family distinctions and capture analysis. Read [decoding contracts](references/decoding-contracts.md) for bounded parsing, byte order, units, quality and reproducible fixtures.

Separate offline decoding, authorized read-only queries and operations that can change a real device. Packet capture analysis does not require sending commands. A connection test, discovery broadcast or register read may still load or affect a production network; use the user's authorized target and scope rather than scanning broadly. This skill provides protocol interpretation, not authorization to move actuators or change safety settings.

Preserve original bytes, capture timestamps and direction beside decoded values. Return unknown fields and ambiguous layouts explicitly. Validate checksums, lengths, identifiers and state before interpreting payloads; plausible numbers are not proof of a correct byte order.

Deliver a documented wire contract with source references, positive and malformed fixtures, scaling and unit rules, quality handling and bounded resource use. For actual control logic, apply the separate control-system workflow and device-specific operating constraints.
