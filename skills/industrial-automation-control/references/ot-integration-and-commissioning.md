# OT integration and staged commissioning

## Understand the data and command path

Map each signal from physical source through controller, gateway, historian and operator interface. Record engineering units, datatype, scaling, update interval, quality and timestamp origin. A gateway can convert an integer register into a floating-point display while losing quality or stale-data information. A valid transport response does not prove a sensor value is current. Reject impossible ranges and distinguish an unavailable sensor from a legitimate zero.

For register protocols, verify byte order, word order, signedness, register numbering convention and atomicity of multi-register values using vendor documentation and known test values. Do not discover write behavior on live equipment by trial and error. For OPC UA, inspect namespace identifiers, NodeIds, access permissions, status codes and source/server timestamps. The [OPC Foundation overview](https://opcfoundation.org/about/opc-technologies/opc-ua/) describes the architecture; the actual server information model and security configuration determine the usable contract. A connected session is not evidence that every write is authorized or appropriate.

Commands should carry explicit intent and acknowledge resulting state where supported. Retries need idempotency or a protocol-aware reconciliation step; repeating a start pulse is not equivalent to retrying a read. Distinguish commanded position, accepted command and measured position. Define freshness deadlines and how the system behaves when acknowledgments or telemetry disappear. A communication reconnect must not replay obsolete commands from a queue without checking their validity.

## Keep operational security compatible with the process

Build an inventory from existing controller projects, engineering records and passive traffic before proposing probes. Older devices may respond badly to ordinary IT scanning. Remote maintenance, credentials, segmentation and time synchronization should be reviewed in the context of process availability and support requirements. [NIST SP 800-82 Rev. 3](https://csrc.nist.gov/pubs/sp/800/82/r3/final) provides OT-specific guidance; it does not make a generic security tool safe for every plant. Preserve already granted scope while identifying any missing live-operation boundary.

## Stage deployment and recovery

Prepare a versioned controller project, parameter set, hardware mapping and rollback artifact before a live change. Verify that backups can actually be restored to the target firmware and hardware revision. Test logic offline, then on a simulator or isolated bench, then under the plant's commissioning process. Agree with the responsible operator on stop conditions, observations and recovery actions. A deployment that changes a process needs operational coordination, not merely a successful file transfer.

After download, verify running version, mode, retained variables, alarm state and I/O feedback. Test normal operation and selected failure paths within the approved envelope. Do not force inputs, defeat safety relays or override interlocks to make acceptance tests pass. Safe shutdown is process-specific: loss of cooling, lubrication or containment may require continued controlled operation. Record actual behavior, deviations and unresolved items. A signed test record or vendor safety procedure has a different authority from an agent's simulation report; preserve that distinction in handoffs.
