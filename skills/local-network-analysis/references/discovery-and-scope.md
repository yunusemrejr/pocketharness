# Discovery with explicit boundaries

## Establish the observation boundary

Represent the scope as interfaces, address ranges, explicit exclusions, time window, allowed probe types and a question to answer. A route through a VPN may make a private subnet reachable without making it part of the requested site. Check routing and interface membership before selecting a target. Honor existing authorization; ask only for a missing boundary that affects actual traffic. Save the resolved scope beside results so later agents do not broaden it accidentally.

On Linux, existing configuration can be read with `ip -j address show`, `ip -j route show` and `ip -j neigh show`. Preserve address family and interface index. Read DHCP leases or switch inventory only through an already available authorized interface. Neighbor caches contain recently resolved peers, not all connected devices. A stale entry is historical evidence; an empty cache is not an empty network. IPv6 link-local addresses require interface scope and must not be merged across links. [IPv6 Neighbor Discovery](https://www.rfc-editor.org/rfc/rfc4861) describes neighbor and router discovery; it does not provide a complete asset census.

## Progress from records to bounded probes

For authorized discovery, build a reviewed target list first. Nmap `-sL -n -iL targets.txt` lists the supplied targets without host probing; disabling name resolution matters because otherwise a planning step can emit DNS requests. Then a bounded `-sn -n -iL targets.txt --max-retries 1 --host-timeout 10s -oA discovery` discovers responding hosts without the ordinary port-scan phase. These are examples, not safe universal timing settings. Match probe rate and retry policy to device tolerance and network conditions. Do not append scripts, operating-system detection or version probing by habit. Nmap's [host discovery documentation](https://nmap.org/book/man-host-discovery.html) explains local ARP behavior, discovery options and why filtered hosts may not respond.

When service inventory is in scope, test a small explicit service-port list on discovered hosts. Preserve Nmap XML rather than scraping its human table; keep the executable version and complete argument vector. A TCP connection probe still reaches applications and can be inappropriate on old controllers. UDP silence is ambiguous and often rate-limited. Do not reinterpret `open|filtered` as confirmed service availability. Nmap's [port-state documentation](https://nmap.org/book/port-scanning.html) defines these observation states.

## Coverage and stopping conditions

Stop when the authorized window closes, error rates rise, device health changes or the inventory question is answered. A retry should address a known cause such as an incorrect interface or dropped replies, not repeatedly sweep the same range. Report excluded addresses, unreachable segments, IPv6 omissions and the sample duration. Compare later runs using stable device evidence and timestamps; DHCP reassignment can create apparent additions and deletions. Keep discovery artifacts local with retention suitable for addresses, hostnames and device identifiers. A clean scan is evidence of what responded from this vantage point, not proof that no other device exists.
