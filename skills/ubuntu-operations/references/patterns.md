# Ubuntu Operations: patterns and examples

## Establish the execution environment
Use `pwd`, `id`, `cat /etc/os-release`, `uname -m`, `command -v tool`, `findmnt -T path` and narrowly scoped `systemctl status` or `journalctl -u unit --since ...`. A systemd command may not apply inside a minimal container or WSL. A path in `/home` may be remote-mounted. Check disk and inode exhaustion separately (`df -h`, `df -i`). Determine whether a process is managed by systemd, a user session, cron, a container or a foreground terminal before restarting it.

## Shell patterns
```bash
if [ -f "$source_path" ]; then
    cp -- "$source_path" "$destination_path"
else
    printf '%s
' 'Source file is missing' >&2
    exit 1
fi
```
Use `--` for utility operands where supported, NUL-delimited records for arbitrary filenames, arrays for argument lists, and `mktemp` with a cleanup trap for temporary work. Do not parse `ls`. `set -e` has exceptions and can exit at surprising points; explicitly handle expected failures and check pipeline semantics before adding `pipefail`. Test destructive selectors by listing exact targets first. Symlinks and races mean a lexical path prefix is not a secure containment check.

## Packages and services
Inspect apt candidates and repository origins before install/upgrade. Respect dpkg locks; identify the owner instead of deleting lock files. Keep Python project dependencies in a virtual environment and Node project dependencies under the lockfile; do not repair an application by replacing the distro's Python. Distinguish system/user systemd units, environment files, working directories and resource limits. `daemon-reload` reloads unit definitions, not application code. Diagnose permission issues with ownership and traversal bits (`namei -l`), not `chmod -R 777`.

## Desktop, server and remote work
GUI applications need the correct user/session environment; sudo often loses display/audio context. Before changing network or SSH services remotely, preserve an access path. Use bounded logs and process inspection; a PID can be recycled, so verify identity before signaling. For backups, distinguish copied bytes, filesystem consistency and application consistency. A restore rehearsal is the completion test.

## Primary references

Check the documentation for the deployed version when an API, support matrix or policy matters. These are reference entry points, not permission to install or deploy.

- https://documentation.ubuntu.com/server/
- https://www.gnu.org/software/bash/manual/bash.html
- https://manpages.ubuntu.com/
