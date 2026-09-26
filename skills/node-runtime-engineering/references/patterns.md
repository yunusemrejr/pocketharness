# Node Runtime Engineering: patterns and examples

## Modules and lifecycle
Understand package.json type, .mjs/.cjs, exports and the caller's loader. A successful dev transpile does not prove the published package resolves. Avoid accidental dual-package state duplication. Use the project's supported Node line and package manager; do not casually regenerate another manager's lockfile.

Handle promise rejection at the owning boundary; avoid async Promise executors and unawaited map/forEach callbacks. Promise.all is concurrency, not a limiter. Honor stream backpressure with pipeline rather than buffering arbitrary input. Bound JSON/body parsing before allocation when possible. Worker threads suit measured CPU bottlenecks; processes provide different isolation and memory behavior.

```js
import { spawn } from 'node:child_process';
const child = spawn('git', ['status', '--porcelain'], {
  cwd: projectDir, stdio: ['ignore', 'pipe', 'pipe'], shell: false
});
```
Consume or redirect both output streams, cap retained output, handle error and close distinctly, and terminate the owned process tree when the workflow requires it. A timeout Promise.race alone does not stop the process.

## Services and terminal applications
For HTTP, validate input, authenticate and authorize separately, limit uploads, set appropriate timeouts, and treat reverse-proxy headers as trusted only from configured proxies. SIGTERM should stop accepting work, drain bounded in-flight work and release resources. Avoid immediate process.exit before logs/data flush.

For TUI/CLI, stdout may be a machine protocol: send diagnostics to stderr. Restore terminal modes and cursor state on exit/cancellation; handle resize, Unicode display width and non-TTY mode. Color codes are not visible columns. A screen render must not become part of model context accidentally.

## Verification
Use the existing test runner and exercise real adapters with mock/local boundaries. Check listener/timer leaks, cancellation, backpressure, duplicate shutdown signals and a packaged smoke run. Measure event-loop delay and memory growth for performance claims; reducing line count is not evidence of lower latency.

## Primary references

Check the documentation for the deployed version when an API, support matrix or policy matters. These are reference entry points, not permission to install or deploy.

- https://nodejs.org/api/
- https://nodejs.org/en/learn/asynchronous-work/dont-block-the-event-loop
- https://docs.npmjs.com/cli/configuring-npm/package-json
