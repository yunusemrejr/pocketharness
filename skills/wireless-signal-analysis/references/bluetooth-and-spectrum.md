# Bluetooth and spectrum evidence

## Discovery is a measurement process

Record the controller, transport, discovery interval, duplicate policy and filters before comparing Bluetooth observations. Advertising intervals and receiver scheduling determine whether a device is seen. An absent advertisement during a short window does not prove a device is powered off. Classic discovery and Bluetooth Low Energy scanning have different behavior; do not treat their records as one uniform sample stream. Existing authorized controller APIs are preferable to changing pairing or connecting to arbitrary discovered devices.

The [BlueZ Adapter API](https://bluez.readthedocs.io/en/latest/adapter-api/) documents discovery filters and RSSI handling. Filters and duplicate settings can affect which updates reach a client, so record them in the experiment manifest. A client may share a controller with other applications. Start and stop only the discovery session owned by the task, and do not reset the controller to clean up another application's state. Manufacturer data and service UUIDs are hints, not verified physical identity. Randomized addresses and changing advertisements can create multiple records for one device.

## Interpret RSSI conservatively

A log-distance model can express a rough relationship: `RSSI(d) = RSSI(d0) - 10*n*log10(d/d0)`, with a calibrated reference distance `d0` and environment-dependent exponent `n`. It is not an accurate indoor ruler. Body blockage, orientation, multipath, antenna gain and device transmit-power behavior can dominate distance effects. Do not infer an exact person location from signal strength. Use the model only when a task calls for calibration, retain repeated measurements and report a broad uncertainty interval.

The Bluetooth SIG's [distance and RSSI discussion](https://www.bluetooth.com/blog/proximity-and-rssi/) explains why RSSI-based ranging is difficult. A 6 dB drop does not universally mean doubled distance: that relationship depends on the selected propagation model. For proximity comparisons, keep device model and orientation fixed, collect multiple windows and evaluate false near/far classifications on held-out placements. Missing data, censored low-signal samples and changing advertising rates should remain visible.

## Spectrum observations require suitable hardware

An ordinary Wi-Fi adapter usually reports decoded traffic and driver counters; it is not a calibrated spectrum analyzer. A monitor-mode packet capture may omit non-Wi-Fi interference completely. A real spectrum instrument requires frequency range, resolution bandwidth, sweep time, detector mode and gain settings in the report. Distinguish occupancy, peak power and integrated channel power. Check for receiver overload before calling a broad rise in energy genuine interference.

Use passive observations for initial troubleshooting. Compare radio measurements with link failures at matching timestamps and locations. A correlation suggests a hypothesis; a controlled reversible change is needed to test it. Do not inject interference, jam channels or force disconnections as a diagnostic convenience. Conclude with supported explanations and the next targeted observation, not a confident device identity or distance estimate derived from one signal sample.
