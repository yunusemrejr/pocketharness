---
name: shared-hosting-deployment
description: Discover, validate and deliver production websites through Git or SSH on Namecheap, GoDaddy and other shared hosts, including existing GitHub CI/deployment integration and rollback.
---

# Shared hosting deployment

Use the actual account, plan and server configuration. Namecheap and GoDaddy branding does not establish SSH access, Node support, PHP version, cPanel Git availability or a deploy hook. Inspect the existing repository, remotes, deploy scripts, GitHub Actions, host configuration and current authorized destination. Check installed CLI help and current official provider documentation when a provider feature is uncertain.

Before remote mutation, establish the source revision, public document root, runtime, protected persistent paths (uploads, databases, secrets), current deployed revision and available rollback. User authorization for deployment persists; preparing a concrete result does not require repeated approval. Never use provider guesses, copied SSH flags or disabled host verification as a substitute for configuration. Read existing credentials only within scope and keep values out of logs/artifacts.

Read [Git/SSH delivery](references/git-ssh.md) for discovery, staging, release switching, GitHub integration and production verification. Reuse an existing working path; do not introduce CI and SSH deployment side by side without a reason. Build and validate locally first, then publish the authorized change through the chosen path and check the site itself. A successful upload is not proof of the correct production revision or behavior.
