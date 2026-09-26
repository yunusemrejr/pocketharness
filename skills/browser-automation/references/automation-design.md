# Automation design

## Choose the execution surface

For a task in an existing browser, discover the actual tab and supported tool methods; do not substitute an unrelated Playwright process and assume it has the same authentication. CUA APIs must come from the active tool documentation. For a repository-owned automation script, inspect its installed Playwright version, test configuration and existing fixtures. Distinguish attaching to a browser from launching one. Record who owns its lifecycle: closing a context created by this task is different from closing the user's browser.

Headful mode helps inspect interactive behavior; headless mode is useful for repeatable unattended runs. Neither mode alone demonstrates parity. Use the same viewport, locale, timezone, browser version and fixtures when comparing failures. Fonts, graphics availability, permissions and authentication can differ. Reproduce an observed discrepancy before changing launch flags. A CAPTCHA or account-consent screen is a workflow boundary, not a reason to invent a bypass.

## Implement stable interaction

In Playwright, prefer role/name, label or explicit test-ID locators over position-based CSS chains. Locators resolve when used; narrowing them to a meaningful container makes duplicate labels manageable. `getByRole('button', { name: 'Save', exact: true })` is suitable only when current page evidence establishes that target. Do not add `force: true` merely to suppress an actionability failure; diagnose the overlay or disabled state. See [official locator guidance](https://playwright.dev/docs/locators).

Wait for a meaningful condition: expected text, URL, visible result, download event or specific network response. A quiet network does not necessarily mean a streamed application is ready. Register an event wait before the action that can emit the event. Treat action, observation and reconciliation as separate steps when an external mutation is possible. Repeating a read is generally simpler than replaying a submit whose response was lost.

For authenticated tests, use isolated contexts and designated test identities where the project supports them. Storage-state files can contain reusable credentials; keep them outside tracked artifacts. Model authentication setup as a prerequisite, not a brittle login sequence copied into every test. Do not export session cookies from a user browser just to make automation convenient.

## Bound work and resources

Set per-operation and whole-workflow deadlines. Cap pagination and downloads using explicit task limits; an infinite scroll is not an instruction to collect everything. Keep parallel read-only pages independent. Serialize actions that share the same cart, account, document or server-side object. In cleanup, close owned contexts even on failure, preserve required evidence first, and do not delete a downloaded deliverable before reporting it.

The final script should expose the input URL or record identifier, expected postcondition, bounded retry policy and artifact directory. Check the installed API before using launch or context options; [Playwright configuration](https://playwright.dev/docs/test-use-options) describes the supported choices.
