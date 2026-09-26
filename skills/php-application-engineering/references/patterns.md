# PHP Application Engineering: patterns and examples

## Request and data boundaries
Use strict_types where consistent with the project, typed boundaries and explicit null handling. A type declaration does not validate an HTTP string's business meaning. Prefer Composer autoloading and dependency injection at actual boundaries over global mutable helpers. Use password_hash/password_verify, random_bytes for secrets, CSRF protection on cookie-authenticated mutations, and explicit session security configuration. Regenerate session IDs after privilege changes; safe regeneration under concurrent requests requires care.

```php
$stmt = $pdo->prepare('SELECT id, email FROM users WHERE id = :id');
$stmt->execute(['id' => $validatedId]);
$user = $stmt->fetch(PDO::FETCH_ASSOC);
```
Allowlist identifiers such as sort columns; placeholders bind values, not SQL grammar. Escape HTML text using htmlspecialchars with explicit UTF-8 and appropriate flags; HTML escaping is not URL or JavaScript sanitization. Avoid serializing untrusted objects, eval, shell interpolation and publicly executable upload directories. Validate upload size, actual media type and storage path; original filenames are untrusted.

## Hosting modes
Local: match production major/minor and extensions; `php -S` is a development server, not a production deployment plan. FPM: inspect pool user, OPcache, timeouts, environment injection, upload limits and reverse-proxy trust. Shared hosting/cPanel: verify selectable PHP version, document root, rewrite support, cron executable, symlinks, writable paths and whether long-lived workers are permitted. Keep secrets and vendor/application internals outside public_html where the host supports it; otherwise use tested deny rules and verify direct URL access.

Run Composer with the project's lockfile and intended production dependencies. Do not commit .env secrets or leave phpinfo/debug endpoints public. A migration needs backup and rollback strategy appropriate to its data changes; file rollback alone does not undo a schema migration. Protect concurrent jobs with an actual lock and define what happens after a crash.

## Verification
Use the project's test framework, static analyzer and formatter; add regression cases for authorization failure, malformed input and transaction rollback. Verify the web SAPI through a safe test endpoint or configured diagnostic, then remove it. Test case-sensitive paths on Linux, permissions as the runtime user, and unavailable mail/queue/storage services.

## Primary references

Check the documentation for the deployed version when an API, support matrix or policy matters. These are reference entry points, not permission to install or deploy.

- https://www.php.net/manual/en/
- https://www.php.net/manual/en/features.session.security.management.php
- https://getcomposer.org/doc/
- https://docs.cpanel.net/
