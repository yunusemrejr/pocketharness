---
name: email
description: "Work with email, emails and mail: AgentMail API sends, full-text inbox search and message triage; local mailbox and mbox search across mailboxes including Thunderbird, Gmail-over-IMAP and SMTP/IMAP servers; careful outreach and newsletter conduct. Use when the task involves emails, mail, inboxes, mailboxes, messages or mailing lists."
---

# Email

Use when the task involves email: sending, searching, or triaging mail, whether through the AgentMail API, a local mailbox, or a mail server. Decide the surface first: AgentMail tools reach only AgentMail inboxes, never a local Thunderbird profile or another provider.

## Working method

- AgentMail API: start with agentmail_status to confirm the key and sender inbox, then agentmail_search or agentmail_messages to find mail, agentmail_message to read one body, agentmail_send to send. Follow agentmail_messages nextPageToken through every page before concluding; a single page is not the inbox.
- Local mailbox files (Thunderbird, mbox, Maildir): snapshot first, parse the copies, never the live store while the client runs. Prefer full-text search over scrolling; check spam, trash, sent, and archive scopes deliberately; an empty result is incomplete coverage, not absence.
- IMAP/SMTP servers: connect only with explicit user authorization and least-privilege credentials, and stop at the authorized scope.
- Keep credentials in the environment. Never write keys or passwords to files, commands, logs, or tool output; never decrypt or exfiltrate stored client credentials without explicit authorization.
- Verify before sending: the recipient from a real source, the sender identity, the suppression list, and the opt-out line. Never pad a campaign to a count.
- For Google sign-in or OAuth flows use google-identity-integration; for message words and voice use natural-editorial-writing.

Read [mailbox search details](references/mailbox-search.md) when working with Thunderbird, mbox, or IMAP; do not load it for AgentMail-only work. User instructions take precedence; this skill adds no authority to access accounts or send mail.

## Evidence and completion

Report which surface was used, what was searched or sent, and what remains unverified. Quote message dates, subjects, and senders for findings; do not present search snippets as verified addresses.
