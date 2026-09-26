# Long community tasks and reliable submissions

Keep one root outcome in `todo` with the topic/product, audience, purpose, exact user authorization, excluded venues, authorized identity, requested quantity, deadlines/budget and completion criteria. Use child steps for discovery, eligible contributions and verification. `description`, `metadata`, `refs`, `acceptance` and `evidence` already persist through plan restoration; draft files can be referenced from them. Do not copy credentials, full pages or session cookies into task metadata. After resumption, re-read the todo plan's pending frontier before acting; recover missing exact instructions from saved task artifacts and metadata. A context reset is not a new authorization request.

For an open-ended request, choose a bounded initial batch of several diverse candidates and prioritize the best fit. Continue toward the stated outcome while useful authorized work remains. Don't stop after producing a plan or drafts when publishing was requested. Carry unresolved blockers and remaining work forward instead of replacing the goal after a follow-up. Reuse available background research, process and reminder tools for long waits; do not invent a scheduler or claim follow-up was scheduled without a returned receipt. Ordinary research and browser handles are process-local; persist findings before shutting down.

## Prepare for the actual contribution

- Question: inspect existing answers/search results, state the true context and attempted solutions, then ask a specific question in the appropriate category.
- Answer or comment: read the thread/article and recent replies, address the actual point, include enough substance locally, and support consequential claims.
- Resource entry: follow the submission schema, category and curator process; verify title, URL, summary, licensing/cost claims and affiliation as relevant.
- Marketing or guest article: tailor the useful contribution to that audience, disclose affiliation, respect link/editorial rules and verify the claims against source material.

Keep complete draft text in a local task artifact when it is long; reference its path and digest. Choose a native editor's supported plain-text/Markdown or rich-text mode and check the preview for links, paragraph breaks, escaping and length. If no account is specified, inspect available authorized identities; never invent one. Ask only for material missing facts/access or authorization not already granted, after completing independent preparation.

## Execute with available capabilities

Prefer a supported authenticated API/connector when it offers the required operation. Inspect its actual documentation and response contract; retain remote record IDs and supported idempotency keys. Web search and page-fetch tools are discovery/read tools, not submission tools.

Use an isolated browser for an eligible isolated-browser workflow. It starts with no imported login, personal profile or credentials. A login-required destination needs a supported authorized authentication surface; an isolated browser launch is not evidence of account access. Preserve a browser explicitly selected by the user. Stop at an actual login, MFA, CAPTCHA, automation prohibition or moderator restriction; save the precise blocker, continue other eligible destinations, and request human intervention only when needed. Do not bypass access challenges or create replacement identities to evade a restriction.

Within the browser, observe the current form, account, thread/category and buttons. Use exact observed role/name or selector, including an observed iframe when appropriate. Fill text inputs, textareas and native contenteditable editors with observed supported controls; for selects use the exact observed option label, and set checkboxes with an explicit boolean. Compare a visible field or preview against known text without dumping its contents, and inspect the comparison result separately: a comparison executing successfully does not prove the content matches. A comparison or a successful click does not prove publication. If a custom editor does not support these controls, use its observed supported mode or another available authorized surface; do not repeatedly force input.

Track the temporary browser's remaining time and action budget from the available tool's receipts or status, and save task progress before either runs out; refreshing or extending a session preserves the temporary profile/page but does not submit, retry or extend task authorization. Expiry, cancellation, close, reload and process exit destroy the temporary browser; reopening requires reacquiring identity and reconciling every pending effect. Parallelize independent reads only; serialize submissions for the same account and venue.

## Persist before submitting; reconcile before retrying

On the existing contribution step, store metadata such as:

```json
{
  "submission": {
    "destination": "https://community.example.org/thread/123",
    "account": "authorized account label",
    "kind": "answer",
    "draftPath": "task-artifacts/answer.md",
    "contentDigest": "sha256 of exact outgoing content",
    "authorizationRef": "reference to the user's publishing instruction",
    "state": "prepared",
    "attemptedAt": null,
    "remoteId": null,
    "receiptUrl": null,
    "evidence": null,
    "nextAction": "verify preview and submit"
  }
}
```

Keep a stable identity for destination/thread plus account plus contribution purpose, as well as the exact content digest. Edited wording must not create a second contribution to an already submitted thread. Before sending, check prior plan steps and the destination/account history for an existing or pending version. Use genuine canonical thread/post URLs, preserving identity-bearing query parameters when needed; browser diagnostic URLs intentionally omit parameters, so they may be insufficient as a permalink. Never store access tokens or tracking secrets as evidence URLs.

Persist `state:"submitting"` and the attempt timestamp **before** clicking Submit or calling the write API. If saving that checkpoint fails, repair the local checkpoint before sending. Recheck the account, target, exact draft, disclosures and links, then submit once. Observe one of:

| State | Evidence and next action |
| --- | --- |
| `published` | Actual remote record/permalink with matching account and content. Record evidence; finish this contribution. |
| `pending_moderation` | Explicit pending notice or submission receipt. Save it; don't submit again. Schedule a check only within authorized follow-up. |
| `uncertain` | Timeout, disconnect, expired browser or missing receipt after sending. Read account history, target thread or supported API status; don't replay automatically. |
| `rejected` | Explicit validation/moderator rejection. Fix a validation error only after confirming no record was created; honor moderation decisions. |
| `blocked` | Specific access/rule blocker before sending. Save the reason and move to other eligible work. |

On resume, treat `submitting` as `uncertain`, even if the browser disappeared. Absence from a public thread is insufficient to exclude a moderation queue. If the outcome cannot be reconciled, keep it uncertain and continue independent work. A local plan/digest is a recovery record, not a server-side guarantee against duplicates. Do not claim exactly-once publishing on an API without such a contract.

## Finish against the requested outcome

Record verified publication links, receipts awaiting moderation, unresolved effects and concrete access blockers separately. A `published` claim requires durable remote evidence. If the user requested live posts, pending moderation leaves publication verification open; if submission alone was requested, a confirmed submission receipt may satisfy that step. Complete todo items only with evidence matching their acceptance criterion. Report achieved quantity and remaining gap without presenting drafts, clicks or estimates as published work. Measure replies or qualified visits only when the task includes that follow-up, and keep observed attribution separate from causation.
