# Linux Host Defense: patterns and examples

## Assess before mutating
Read `id`, `/etc/os-release`, `ss -lntup`, relevant service unit configuration and package policy with appropriate access. Do not dump process environments, private keys or credentials into logs. Distinguish laptop, shared hosting, container, VM and internet-facing host: container root and host root have different exposure; a container is not automatic isolation from mounted sockets or directories.

Untrusted repository hooks, Makefiles, npm scripts, editor tasks and downloaded archives are executable inputs. Inspect before running elevated. Avoid `curl ... | sudo bash`; verify source, digest/signature where available, unpack to a controlled directory and inspect traversal/symlink behavior. Treat PATH, dynamic linker paths, writable parent directories and shell expansion as trust boundaries. Quote variables and use argument arrays; quoting does not make an untrusted command safe.

## Hardening with recovery
Before SSH/firewall changes, establish a tested alternative access path and a rollback. Validate SSH configuration (`sshd -t` where installed), allow the actual management path and test a second connection before closing the first. IPv6 and container-published ports need their own verification; an enabled UFW screen alone does not prove exposure is blocked.

For a systemd service, evaluate a dedicated user, restrictive writable paths, NoNewPrivileges, capability bounding and filesystem protection against its actual requirements. Test capability and device access; do not paste a universal hardening unit that silently breaks backups or networking. Keep AppArmor/SELinux confinement enabled where supported, diagnose denials rather than disabling the mechanism globally.

Reduce credential scope, permission and lifetime. Separate deployment identity from runtime identity; secrets are not command-line arguments. Protect backup integrity and test restoration. Patch through the distribution's supported channels; verify unattended update/reboot policy fits the workload. Log authentication and relevant service failures with retention and access controls.

If compromise is suspected, preserve evidence and contain the affected system. An antivirus scan or deleting one suspicious file does not establish that the host is trustworthy again; determine credential rotation and rebuild scope from evidence.

## Primary references

Check the documentation for the deployed version when an API, support matrix or policy matters. These are reference entry points, not permission to install or deploy.

- https://documentation.ubuntu.com/security/security-features/security-features-overview/
- https://documentation.ubuntu.com/server/how-to/security/firewalls/index.html
- https://www.freedesktop.org/software/systemd/man/latest/systemd.exec.html
