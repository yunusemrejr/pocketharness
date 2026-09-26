# Workflow patterns

## Node CI with cache and matrix

```yaml
name: ci
on:
  push: {branches: [main]}
  pull_request:
concurrency:
  group: ${{ github.workflow }}-${{ github.ref }}
  cancel-in-progress: true
permissions:
  contents: read
jobs:
  test:
    runs-on: ubuntu-latest
    timeout-minutes: 20
    strategy:
      fail-fast: false
      matrix:
        node: [22, 24]
    steps:
      - uses: actions/checkout@v5
      - uses: actions/setup-node@v5
        with:
          node-version: ${{ matrix.node }}
          cache: npm
      - run: npm ci
      - run: npm run lint
      - run: npm test
```

Key the cache on the lockfile (built-in `cache:` does this); add `restore-keys` only when a stale partial cache is safe.

## Python CI

```yaml
      - uses: actions/setup-python@v6
        with: {python-version: '3.12', cache: pip}
      - run: pip install -e '.[test]'
      - run: pytest -q
```

Pin the interpreter to the versions you support; run the linter and type checker as separate steps so failures are attributed.

## Release with provenance

```yaml
name: release
on:
  push: {tags: ['v*']}
permissions:
  contents: write
  id-token: write
  attestations: write
jobs:
  publish:
    runs-on: ubuntu-latest
    environment: release
    steps:
      - uses: actions/checkout@v5
      - uses: actions/setup-node@v5
        with: {node-version: 22, registry-url: 'https://registry.npmjs.org'}
      - run: npm ci && npm test
      - run: npm publish --provenance --access public
        env: {NODE_AUTH_TOKEN: '${{ secrets.NPM_TOKEN }}'}
      - uses: actions/attest-build-provenance@v2
        with: {subject-path: dist/*}
```

Release jobs should not be cancelled by the PR concurrency group, and should require an environment approval when publishing is irreversible.

## Deploy with OIDC instead of stored keys

```yaml
permissions:
  contents: read
  id-token: write
steps:
  - uses: aws-actions/configure-aws-credentials@v4
    with:
      role-to-assume: arn:aws:iam::123456789012:role/deploy
      aws-region: eu-central-1
```

The role's trust policy should restrict the repository, branch and environment. No long-lived access keys in secrets.

## Hardening checklist

- [ ] Workflow-level `permissions` is read-only; write scopes are per job.
- [ ] Third-party actions pinned by SHA; Dependabot configured for actions.
- [ ] No `pull_request_target` with untrusted checkout.
- [ ] Untrusted `github.event.*` values go through `env:` and are quoted.
- [ ] Secrets never echoed, exported to artifacts, or used in fork builds.
- [ ] `timeout-minutes` set; concurrency group set and releases excluded from cancel.
- [ ] Required checks match existing job names; renaming tested against branch protection.
- [ ] Artifacts have retention limits and contain no credentials.
- [ ] Self-hosted runners (if any) never execute untrusted PRs.

## Common failure signatures

| Symptom | Likely cause |
| --- | --- |
| Job runs on the wrong version combinations | Matrix `include`/`exclude` mismatch or a stale required check name |
| Cache "hit" but install still slow | Key on lockfile hash; restored cache misses the lockfile change |
| Works locally, fails in CI | Different Node/Python version, missing service, or `NODE_ENV`/locale differences |
| Secrets empty in a fork PR | Expected: fork builds do not receive secrets — use `pull_request` and gate deploy jobs |
| Flaky network steps | Add bounded retries with backoff in the step, not `continue-on-error` |
| Required check waiting forever | Job renamed or conditionally skipped (`if:`), so the check never reports |
