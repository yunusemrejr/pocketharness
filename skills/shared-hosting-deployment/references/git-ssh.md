# Git and SSH delivery

## Discover the real destination

Inspect `git remote -v`, branch policy, deployment scripts, CI workflows, SSH host aliases and the hosting control panel/configuration without printing private keys or secret values. For an authorized connection, confirm the host identity through the existing trusted configuration. Use a bounded read-only remote probe to establish document root, PHP CLI/web runtime differences, permitted commands, disk space, permissions and current revision. Node/Flask/Java services require an actual supported process manager or server plan; otherwise deliver a compatible static frontend/PHP backend or report that constraint.

Identify the deployment mode: a control-panel Git pull plus hook, a GitHub Actions SSH job, or a local Git/SSH release copy. Reconcile existing production changes and protected data before updating. Never run an unqualified sync with `--delete` over uploads, secrets or a database directory.

## Implement and stage

Use the project's lockfile and production build. PHP 8+ deploys must match installed extensions and SAPI behavior; static React/vanilla assets must use the correct base URL and nested-route fallback. Set the web root to public build output, not the repository root. Keep `.git`, environment files, private keys, source maps containing secrets and server-only modules inaccessible to HTTP.

Prefer a staging/release directory with a revision marker and a previous release when the host supports it. Transfer only the validated artifact with preserved permissions and a dry-run/file manifest when available. Keep persistent uploads/config outside replaceable release files. Verify staged dependencies/config, then use the host-supported atomic symlink/rename or its existing deploy hook. If atomic switching is unavailable, follow the project's maintenance/backup procedure and verify immediately after the copy.

Database migrations require compatible application/data ordering and an explicit recovery plan. A filesystem rollback does not undo a destructive schema change; do not bundle one into a routine deploy by accident.

## GitHub integration

Inspect existing workflow triggers, environments, concurrency, secrets and protection rules. Build/test before the deploy job, deploy the verified commit/artifact, and serialize writes to the same destination. Prefer the established SSH credential with verified host keys; do not print secrets or broaden token permissions. A pushed commit is not a completed deployment: observe the actual CI/job result and identify its deployed revision. Do not create a second release/deploy path that repeats completed work.

## Verify and recover

Request the real production URL and a changed route/asset, inspect status, content/revision marker, HTTPS, redirects, cache behavior and errors. Exercise a relevant read/write/error path using authorized test data; clean up the test data owned by this run. Verify that storage permissions, background jobs and sessions match the actual host. Keep production data out of public checks.

On failure, inspect the deployed state before retrying. Switch to the previous compatible release or the existing recovery procedure, then verify recovery. End with source/deployed revision, local/CI/production checks, URL, rollback reference and unresolved limitations. Do not claim a live deployment from an unconnected SSH command or a prepared workflow file.
