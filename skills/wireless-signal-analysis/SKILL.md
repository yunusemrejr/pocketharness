---
name: wireless-signal-analysis
description: Diagnose Wi-Fi and Bluetooth link quality using RSSI, SNR, channel occupancy and repeatable measurements; distinguish radio evidence from distance or device-identity guesses.
---

Define whether the task concerns coverage, interference, roaming, throughput or Bluetooth discovery before choosing measurements. RSSI alone cannot answer all of these questions. Record adapter, driver, antenna arrangement, band, channel width, location, time and workload; compare measurements taken under equivalent conditions.

Use existing connection statistics first. A scan transmits or changes receiver behavior depending on hardware and mode, so remain within the user's authorized devices and scope. Preserve existing authorization. Do not change transmit power, regulatory settings, access-point channels or Bluetooth pairing merely to collect a baseline. Never use deauthentication or interference generation as an ordinary diagnostic shortcut.

Read [wifi-measurements](references/wifi-measurements.md) for signal units, channel utilization, link counters and repeatable Wi-Fi experiments. Read [bluetooth-and-spectrum](references/bluetooth-and-spectrum.md) for discovery behavior, RSSI uncertainty and spectrum-instrument limitations.

Keep measured signal strength, inferred link margin and application performance separate. Explain missing noise measurements and counter resets explicitly; avoid invented SNR values or universal good/bad thresholds. Bluetooth addresses may rotate, and RSSI cannot establish an exact distance or personal identity.

Deliver a time-aligned comparison, likely causes with supporting evidence, and the smallest reversible experiment that distinguishes them. Check the relevant hardware capabilities before prescribing monitor mode, spectrum capture or advanced channel features.
