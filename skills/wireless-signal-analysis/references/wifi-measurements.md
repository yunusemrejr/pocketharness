# Wi-Fi measurements and controlled experiments

## Measure the correct quantity

RSSI is commonly reported in dBm, a logarithmic received-power scale. A change from -70 dBm to -60 dBm corresponds to ten times the received power, not ten percent improvement. If signal and noise refer to comparable bandwidth, calibration and time, `SNR_dB = signal_dBm - noise_dBm`. For example -62 dBm signal and -92 dBm noise yield 30 dB SNR. When the driver omits noise, report SNR as unknown instead of substituting a typical noise floor. A driver-reported signal average and a per-packet value are not interchangeable.

Prefer medians and lower-tail observations over a single attractive reading. State the sampling period, number of samples and packet activity. For an arithmetic mean of physical power, convert dBm to milliwatts, average, then convert back; an average in dB answers a different statistical question. Neither aggregate establishes throughput without measurements of airtime, interference, retries and application behavior.

## Read connection and channel evidence

On a supported Linux adapter, `iw dev` identifies interfaces, `iw dev wlan0 link` reports the associated link and `iw dev wlan0 station dump` exposes peer counters. Replace the example interface with the discovered one. `iw dev wlan0 survey dump` may expose channel-active and busy time where the driver supports them. Use differences between two samples, not lifetime totals: utilization is `delta_busy / delta_active` only when both counters cover the same interval and channel. Reject a zero denominator or counter reset. The [Linux Wireless iw documentation](https://wireless.docs.kernel.org/en/latest/en/users/documentation/iw.html) describes the supported management interface; local `iw help` and driver capabilities resolve version-specific commands.

Read transmit bitrate as a negotiated or recent PHY estimate, not application goodput. Compare retry and failed-transmission deltas with traffic volume. Channel width, spatial streams, modulation, power-saving and client/AP capabilities can change independently. A crowded neighboring channel can matter even if the strongest access point appears healthy. A survey observation sees what this receiver measures, not the full RF environment at every client.

## Design an experiment that isolates a cause

Choose one variable such as client position, band or competing load. Keep server endpoint, test duration, file size and measurement path constant. A local authorized wired endpoint separates WAN variability from the wireless link. Bound any generated test traffic and record background demand; a throughput test itself consumes airtime and may distort the symptom. Retest with the same adapter orientation and power state. Compare latency distribution, packet loss, retransmissions and goodput alongside RSSI.

Report uncertainty rather than universal thresholds. Different PHY modes require different margins, and implementation differences make values across adapters imperfectly comparable. Recommend configuration changes only when a measured comparison supports them, with a reversible plan and the user's existing operational scope. Keep regulatory-domain and transmit-power changes out of routine diagnosis.
