---
name: linux
description: Linux system understanding — /proc mental model, systematic debugging (strace/perf/journalctl/ss/iostat), systemd, permissions/capabilities, filesystem & disk, networking, and common gotchas. Use when diagnosing system-level problems, writing services, or reasoning about what a Linux box is actually doing.
---

# Linux Understanding

## Start with a bounded snapshot

Use `pocket kit sys` for kernel, load, available memory/swap, CPU/memory/I/O pressure and workspace disk/inodes; `pocket kit sys PID` shows process state, threads, RSS/swap, wait channel and accessible I/O counters. These native snapshots read no environment or command-line secrets. Missing procfs/permissions remain unavailable, not proof of a healthy resource. Compare repeated bounded snapshots before claiming growth or a bottleneck, then use a targeted installed diagnostic. Preserve already verified state and use one focused failure hypothesis per check.

## Mental model

Everything is a process in a namespace with a cgroup, every observable is a file, every limit is a number in sysctl/cgroup/rlimit. When something is "mysterious," there's a file describing it.

**`/proc` first reflex:**
- `cat /proc/<pid>/status` (state, threads, VmRSS, uid), `/proc/<pid>/stack` (kernel wait), `/proc/<pid>/wchan` (what it's blocked in), `ls /proc/<pid>/fd` (what's open), `readlink /proc/<pid>/fd/*` (where they point), `/proc/<pid>/io` (block read/written)
- `cat /proc/meminfo` (MemAvailable is the number, not "free"), `/proc/pressure/*` (PSI: CPU/memory/IO stall % — the fastest "who's hurting" signal)
- `/proc/net/tcp*` + `ss -tnp` (connections, states, owning process), `/proc/diskstats`, `/proc/loadavg` (load = runnable+uninterruptible, so high load ≠ CPU busy — check D-state: `ps -eo stat | grep -c '^D'`)

## Symptom → tool

| Symptom | First tools |
|---|---|
| Process stuck / D state | `pocket kit sys PID`, bounded `strace -p PID`, `findmnt -T /affected/path`, `iostat -xz 1 3` |
| CPU high | `top -H -p PID` (which thread) → `perf top -t TID` or `perf record -p PID` on the hot function |
| Latency spikes | `perf record` + flamegraph; `bpftrace` for syscalls if kernel-side; check `cat /proc/pressure/cpu` (PSI) |
| Memory growth | `smem -s rss -k -t P<pid>`, pmap, cgroup `memory.peak`; `journalctl -k` (OOM kills: "Killed process … (oom_kill)") |
| Disk full | `df -h /affected/path`, targeted `du -xsh /affected/path`, deleted-open files with available `lsof +L1`, and `df -i /affected/path` |
| Slow disk | `iostat -xz 1` (util, await), `iotop`; NVMe `smartctl` |
| Networking | `pocket kit reach HOST:PORT 3`, `ss -tnp`, `ip route get DEST`, bounded authorized capture/path checks, available NIC counters |
| "Who opened/changed file X" | Existing audit records with `ausearch -f X --start recent`; adding a watch uses `auditctl`, changes policy and needs an authorized scope |
| Service misbehavior | `journalctl -u svc -n 200 --since -1h`, `systemctl status svc` (full: ` systemctl status --lines=200`) |
| Startup slow | `systemd-analyze blame` (service times), `systemd-analyze critical-chain` |

Text reflexes: `strace -f -e trace=openat,write` to see a process's IO story; `ltrace` for library calls; `ls -l --time-style=full-iso`; `watch -n1`; `journalctl --follow`.

## Systemd literacy

- Units are declarative; **drop-ins** override without editing the unit: `systemctl edit svc` → override.conf (`[Service] CPUQuota=50%` etc.).
- `ExecStart` must be the service's main process; wrap in a shell only when you must.
- `Restart=on-failure` + `RestartSec` + `StartLimitIntervalSec` — know why a unit is in `failed: start-limit-hit`.
- Timers = cron replacement (calendar/every, `Persistent=true` catches up on downtime). Sockets: `Socket=/unit.socket` for on-demand services.
- `journalctl` is the log: `-u unit`, `-t tag`, `-p err` (priority), `--since/-n`, `_SYSTEMD_UNIT=`, `+CONTAINERS`, `--disk-usage`, `vacuum`.
- Resource limits: `MemoryMax=`, `CPUQuota=`, `TasksMax=` on the unit (cgroup v2) — set them; "the box ran out" is almost always "a unit had no limit."

## Permissions & identity

- Unix perms (rwx owner/group/other, sticky bit on /tmp), **ACLs** (`getfacl`/`setfacl`) for the case where one user needs one exception, **capabilities** (`getcap -r /`, `capsh --print`) for granting one privilege instead of setuid-root.
- Inspect the actual `umask`, mode, ACL and mount state. Creating/removing a directory entry needs write and execute permission on its parent; path traversal needs execute permission on ancestors. Writing an existing file follows that file's permissions and applicable ACL/security policy. A default umask is not guaranteed in a service.
- selinux/apparmor: when "it works as root, fails as the user" with no obvious perms, `getenforce` / `dmesg | grep -i denial` first.
- `chattr +i` (immutable) files are the silent killer of "permission denied" on root — `lsattr` to check, `chattr -i` to clear.

## Disk & filesystem

- Mount: `findmnt -T /path` (what fs, which options — `noatime`, `ro`…), fstab entries survive reboots, `mount` doesn't.
- XFS/ext4: `xfs_info` / `tune2fs -l`; snapshots are LVM/zfs/btrfs features, not ext4.
- **Sparse files & `du`**: apparent size (`du -h --apparent-size`) vs allocated — report the right one; `df` counts blocks, `du` counts inodes' blocks (deleted-but-open discrepancy: same as above).
- Filesystem repair needs the target unmounted and the filesystem-specific recovery procedure; a read-only mount alone does not make concurrent `e2fsck` safe. Inspect first and preserve a recovery path before an authorized repair.

## Networking

- `ip` over ifconfig: `ip a`, `ip r`, `ip s` (errors), `ip -s link` (drops).
- Routing is the first suspect for "worked yesterday": `ip route get <dest>` shows the actual path decision (policy routing, source-based).
- Firewall: `nft list ruleset` (or `iptables -L -n -v` on old boxes): the **counter column tells you what's being hit** — "no firewall rule matches" shows up as 0 hits + DROP default.
- DNS: `resolvectl status`, `getent hosts x` (NSS order), `rdig`/`dig +short` per resolution step — split-brain DNS (hosts file vs. resolver) is a classic.
- sysctl tuning: `net.core.somaxconn` (accept queue overflows show as "SYN cookies"/drop counters), `net.ipv4.tcp_tw_reuse` for outbound-heavy clients — change with a comment on why.

## Gotchas checklist

`df`/`du` mismatch (deleted-open) · inode exhaustion (`df -i`) · `ETXTBSY` (editing a binary while it runs — copy/sync/replace) · `PATH` pollution from `.profile`/nvm switching shells · `LD_PRELOAD` set by a profile (kill it before you trust a binary) · timezones in logs (system vs. app) · `hostname` resolution loops (`/etc/hosts` consistency) · zombie reaping (parent must `wait()`) · NFS stall = client in D state, server-side `nfsd` logs · "works in my shell" = the shell's environment (check `env -i bash -c` for the service's view).
