# Firewall and routing

## Read the policy before theorizing

Dump the effective ruleset first: `nft list ruleset` for nftables, or `iptables -S` and `iptables -t nat -S` (plus `ip6tables`) where iptables remains. Read chain policies (`ACCEPT` versus `DROP`), then rules in order — first match wins, and a logging rule's position changes what it observes. `conntrack -L` (when available) shows tracked connections and their states; a missing conntrack entry for an allegedly established flow points at asymmetric routing or a bypass, not at the firewall.

Distinguish host firewall from path filtering: test loopback, same-subnet, and routed destinations separately. Loopback success with routed failure implicates forwarding or the path; same-subnet failure implicates the host ruleset or the peer.

## Common drop signatures

- TCP SYN with no reply and no RST: filtered (dropped) rather than refused. Check `INPUT`/`FORWARD` drops and upstream ACLs.
- ICMP port-unreachable or TCP RST: reached a host or a rejecting rule. A reject rule answers; a drop rule stays silent — the firewall's own behavior distinguishes them.
- Established connections dying after idle time: conntrack timeout or a stateful middlebox expiring the flow. Compare `ESTABLISHED`-rule placement and timeout settings against the application's keepalive behavior.
- DNS works but TCP fails, or small packets pass while large ones stall: MTU or fragmentation policy, not port rules. Verify with size-sweep ping before touching rules.
- One direction works: missing or asymmetric return rule, NAT misconfiguration, or reverse-path filtering (`rp_filter`) dropping asymmetric replies. Check `sysctl net.ipv4.conf.<iface>.rp_filter` and the NAT table together.

## NAT, masquerade, and forwarding

Forwarding requires both the sysctl (`net.ipv4.ip_forward`, `net.ipv6.conf.all.forwarding`) and `FORWARD`-chain permission; either one missing stops routed traffic silently. For masquerade, verify the `POSTROUTING` rule matches the intended source and egress interface, and confirm the egress address is the one replies return to — multi-homed hosts pick surprising source addresses.

Hairpin (NAT loopback) failures look like external outages from inside: a client behind NAT reaching the public address of a local service needs an explicit hairpin rule or split DNS. Test from outside the NAT boundary before declaring the service down.

## Change discipline

Firewall and routing changes need explicit authorization, a recorded before-state (full ruleset dump), a single minimal change, and an after-test from an affected client — plus a rollback command prepared before applying. Prefer atomic replaces (`nft -f` with a complete file, `iptables-restore`) over incremental appends that drift from the documented policy.

Never disable the firewall as a diagnosis step on a reachable host; instead add a narrow temporary logging rule, observe, then remove it. Log rules with rate limits to avoid disk exhaustion during a scan or flood.

Credentials for managed firewalls, VPN concentrators, or cloud security groups live in the OS environment or the existing secret mechanism — never in commands, saved ruleset dumps shared externally, or transcripts. Sanitize public IPs and internal topology from any artifact that leaves the authorized scope.

## Primary references

- https://wiki.nftables.org/
- https://man7.org/linux/man-pages/man8/iptables.8.html
- https://man7.org/linux/man-pages/man8/sysctl.8.html
