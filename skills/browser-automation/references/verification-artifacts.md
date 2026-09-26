# Verification, artifacts and replay

## Define the proof before collecting it

A screenshot proves visible pixels at one moment. A DOM assertion proves the queried state. A successful response proves only the server response actually inspected. None automatically proves durable persistence. For a saved document, verify its identifier and re-read the saved content; for a download, verify the completed local file rather than the presence of a download button. Match the evidence to the user's requested outcome.

Use a small deterministic test fixture when building automation. Test one representative success and one realistic disruption, such as a missing item, rejected upload, timeout after submit or navigation during a pending request. Avoid an exhaustive browser matrix unless compatibility is part of the assignment. A flaky assertion should lead to a better postcondition or diagnosis, not a longer arbitrary sleep.

## Trace deliberately

For Playwright Test, a trace on the first retry can capture actionable failures without storing every successful run. Inspect the local trace for the action timeline, DOM snapshots and network evidence. Tracing can expose page content and request information; keep artifacts scoped to the task and redact before external sharing. See [Trace Viewer](https://playwright.dev/docs/trace-viewer). If using a standalone script, check the current tracing API and stop tracing before closing the owned context so the artifact is written.

Name artifacts by case and attempt rather than overwriting a single failure.png. Record browser/version, viewport, relevant fixture revision and the last completed step. A screenshot attached to a report should have been visually inspected if the report makes visual claims. Otherwise describe it as captured evidence awaiting inspection.

Downloads require event ordering: establish the wait, perform the click, await the download, then save it to a task-owned path. Use a sanitized filename, inspect failure status, and confirm file type/size. Do not execute downloaded content to verify it. For generated reports, parse or preview the output with a suitable existing tool.

## Replay boundaries

A timeout does not establish that the server rejected an action. Before replaying a save, purchase, send or upload, inspect the destination for the intended effect and any duplicate. Use an application-provided idempotency mechanism when available. A browser restart cannot erase a server-side mutation.

Network mocking is useful for deterministic UI states, but mocked success is not production integration evidence. State which routes were mocked and which server behaviors remain untested. Service workers can intercept requests outside ordinary routing expectations; Playwright documents disabling them for tests that require request interception in [network guidance](https://playwright.dev/docs/network).

Finish with a compact evidence record: expected result, observed result, assertion or artifact, and any unresolved uncertainty. Keep raw traces recoverable without dumping their full contents into the agent's context. An incomplete workflow should identify its exact remaining action, not pretend that a prepared script has already run.
