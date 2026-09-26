# Deployment and rollback

## Environment promotion

Promote the same artifact through environments (build once, deploy many times) rather than rebuilding per stage — a rebuild is a different binary with different evidence. Typical stages: ephemeral review or preview environments per change, a persistent staging mirror for integration, then production. Each stage defines entry criteria (which checks passed), data posture (synthetic, scrubbed, or production-like — never raw production data where avoidable), and who approves exit.

Keep environment configuration (feature states, connection targets, limits) separate from the artifact and reviewable as code. Credentials for every environment live in the OS environment or the secret store — never in deployment manifests, chat, or CI logs. A promotion that cannot name its artifact digest and config revision is not auditable.

## Feature flags and progressive rollout

Flags separate deploy from release: code ships dark, then activates per cohort. Keep flags short-lived with named owners and removal tasks — a flag inventory that only grows becomes a second, untested codebase. Distinguish release flags (temporary, remove after rollout) from operational flags (kill switches, quotas) and permission flags (entitlements); each class has different lifetime and review rules.

Roll out progressively: internal users, small percentage, growing cohorts, full release — with success metrics and automatic halt conditions defined before starting. A rollout without halt criteria is just a slow big-bang. Log flag evaluations with enough context to reconstruct who saw what during an incident.

## Hotfix flow

Hotfixes follow the same pipeline at higher priority, not a bypass around it: branch from the release point (or tag), minimal change, expedited review by a second engineer, full check suite, staged promotion, then forward-merge to main so the fix is not lost in the next release. Document who can declare a hotfix and what evidence the declaration needs; routine work labeled "hotfix" to skip review destroys the category.

Time-box the expedite: if the hotfix path takes longer than the rollback path, roll back first and fix forward calmly. This decision must be pre-agreed, not invented during the incident.

## Rollback discipline

Every deployment plan names its rollback: previous artifact digest, config revision, data-migration compatibility (migrations must be backward-compatible or paired with a forward fix — a migration that blocks rollback is a deployment defect), and the exact commands or pipeline actions. Practice rollbacks on staging; an untested rollback procedure fails exactly when needed.

Prefer rollback mechanisms that are fast and boring: redeploy the previous digest, flip the flag, shift the traffic weight. Novel recovery procedures during incidents add risk on top of risk. After any rollback, record what triggered it, what the forward fix needs, and what gate would have caught it earlier.

## Pipeline observability

Instrument the pipeline itself: lead time (commit to production), change-failure rate, rollback frequency, check flakiness, and time-to-restore. Flaky checks deserve the same urgency as production bugs — a suite the team does not trust stops gating merges in practice, whatever the settings say. Quarantine flakes explicitly with owner and expiry rather than letting retries normalize them.

Keep deployment records queryable: who promoted what digest to which environment when, with which approvals and which flag states. Incident review starts from this record; reconstructing it from chat history wastes the first hour.

## Primary references

- https://cloud.google.com/architecture/devops/devops-tech-deployment-automation
- https://martinfowler.com/articles/feature-toggles.html
- https://docs.github.com/en/actions/deployment/about-deployments/about-continuous-deployment
