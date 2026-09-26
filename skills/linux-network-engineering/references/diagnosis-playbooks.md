# Diagnosis playbooks

All examples use documentation addresses (`192.0.2.0/24`, `198.51.100.0/24`, `203.0.113.0/24`) and `example.com`; substitute the authorized target. Commands shown are read-only unless marked CHANGE.

## Rung 1: link and addresses

`ip -brief link` and `ip -brief addr` show interface state and assigned addresses. A `DOWN` or `NO-CARRIER` interface is a physical or driver problem, not a routing problem. `ethtool <iface>` (when available) reports negotiated speed, duplex, and link detection; a mismatch against the switch port explains loss and slowness that higher layers cannot fix.

Missing addresses point at DHCP or static misconfiguration: check `networkctl status <iface>` for systemd-networkd or `nmcli device show <iface>` for NetworkManager, then the DHCP client logs. An address with a wrong prefix length breaks same-subnet reachability while leaving the default route intact — verify the `/N` suffix, not just the address.

## Rung 2: routes

`ip route get <destination>` answers which route, interface, and source address the kernel would use — this single command replaces most route-table reading. `ip route show` and `ip -6 route show` reveal competing defaults, stale entries, and policy routing (`ip rule show`) that overrides the main table.

Asymmetry matters: return-path routing on the far side can break TCP while ICMP appears fine. When a connection stalls after SYN, suspect the return path, MTU blackholes (`ping -M do -s <size>` probes MTU), or a middlebox dropping specific flags — not the local default route.

## Rung 3: DNS

`resolvectl status` (systemd-resolved) or the contents of `/etc/resolv.conf` plus `/etc/nsswitch.conf` define the actual resolver path. `getent hosts example.com` exercises the system resolver including NSS plugins; `dig @<server> example.com` bypasses them and tests one server directly. When the two disagree, the defect is in NSS, search domains, or `ndots` handling — not in DNS itself.

Check negative caching and TTL behavior with `dig +trace` sparingly (it walks from the roots and is slow), and verify DNSSEC or split-horizon views when internal and external answers differ. Do not change resolver configuration as a diagnosis step without authorization; record the broken state first.

## Rung 4: TCP reachability and path

`ss -tlnp` shows what the host actually listens on; a refused connection to a port nothing listens on is a service problem. `ss -tnp` during a failure shows socket state — `SYN-SENT` stuck means no reply, `ESTABLISHED` with no progress means an application or window stall.

`mtr --report-wide <target>` combines traceroute with per-hop loss over time; run it long enough to separate persistent loss from single-probe noise. Compare forward-path hops against the reverse direction when possible. Treat ICMP-filtered hops (`???`) as unknown, not as faulty — many routers deprioritize or drop ICMP while forwarding TCP normally.

## Rung 5: TLS and application

`curl -v` (or `openssl s_client -connect host:port -servername host` for TLS detail) separates TCP success from TLS failure: certificate expiry, hostname mismatch, missing intermediates, and protocol or cipher refusal each have distinct messages. Verify the system trust store and clock — an expired-looking certificate on a host with a wrong date is a clock problem.

Only after TLS succeeds does application debugging begin. Carry the verified lower rungs as evidence so application work does not re-litigate the network.

## NetworkManager versus systemd-networkd

Identify which manager owns the interface before interpreting state: `nmcli general status` and `networkctl list` show their respective claims. Conflicting managers (both trying to configure one interface) produce flapping addresses and routes. Configuration lives in profiles or `.network` units depending on the manager; read the active profile, not just the files on disk, since pending changes may not be applied.

## Primary references

Check the documentation for the installed version when behavior matters. These are reference entry points, not permission to change configuration.

- https://man7.org/linux/man-pages/man8/ip.8.html
- https://man7.org/linux/man-pages/man8/ss.8.html
- https://www.freedesktop.org/software/systemd/man/latest/resolvectl.html
- https://networkmanager.dev/docs/
