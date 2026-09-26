# Mailbox search details

Local-first playbook for Thunderbird profiles, mbox/Maildir stores, and IMAP
servers. Read-only unless the user authorized a connection or a send.

## Thunderbird profile layout (Linux)

- Native install: `~/.thunderbird/<profile>/`; Snap: `~/snap/thunderbird/common/.thunderbird/<profile>/`.
- `prefs.js` holds connection facts (hostnames, usernames, folder URIs). Read
  it for server identity only; passwords live in `logins.json` (NSS-encrypted)
  and are never an input to transcribe, decrypt, or reuse without explicit
  authorization.
- Mail stores: `ImapMail/<host>/` for IMAP accounts, `Mail/` for local folders,
  `webaccountMail/` for Exchange/Outlook bridges. Large extension-less files
  (`INBOX`, `Sent`, `Archive`) are mbox stores; `.msf` files are indexes, not
  mail. `global-messages-db.sqlite` (Gloda) is a content index that can reveal
  messages no longer present in the folders.

## Searching local stores

1. Snapshot first: copy the mbox files (or the whole profile) to a temp
   directory and parse the copies. Thunderbird may hold the live store open.
2. Parse with `mailbox` from the Python standard library, not ad-hoc line
   splits: it handles multipart bodies, encoded headers, and `From ` escapes.
3. Narrow by correspondent domain first (for example `@customer.example`), then
   by subject terms (renewal, grace period, invoice, license), then read full
   bodies for the surviving threads in chronological order.
4. Check every scope the question touches: inbox, sent, drafts, trash, archive,
   and each configured account. Note the date coverage of each store (oldest
   and newest message) before claiming anything about older mail: a local sync
   window is not the account history.

## IMAP servers

- Connect only when the user said to, with credentials they supplied for this
  purpose (prefer app passwords over master passwords). Use `imaplib` over
  TLS, `SEARCH` server-side, and `FETCH` bodies for the surviving ids.
- Verify host, port, and encryption from the client's own settings
  (`prefs.js` socket type and port) rather than guessing.
- Gmail over IMAP needs an app password and honors labels as folders; expect
  `All Mail` to duplicate folder contents.

## Sources

- AgentMail API: https://docs.agentmail.to
- Thunderbird profiles: https://support.mozilla.org/en-US/kb/profiles-where-thunderbird-stores-user-data
- Python `mailbox`: https://docs.python.org/3/library/mailbox.html
- Python `imaplib`: https://docs.python.org/3/library/imaplib.html
