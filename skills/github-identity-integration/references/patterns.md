# GitHub Identity Integration: patterns and examples

## Login transaction
Generate unpredictable, single-use state, bind it to the initiating browser session and expire it. Use PKCE with the documented S256 mechanism where supported by the flow; GitHub supports PKCE, so do not repeat old guidance claiming otherwise. Keep the verifier bound to that login attempt. Validate the callback and exchange code only at the intended token endpoint. A public client cannot protect an embedded secret.

With the resulting access token call the authenticated user API and validate its response. GitHub's OAuth token is not a generic Google-style ID token to decode. Use GitHub's stable user ID as the external account key; login names and emails can change. A missing/private email must not break identity. Request email access only when the application actually needs it, and handle its verified/primary semantics deliberately.

## Account and permission boundaries
Do not auto-link an existing local account solely by matching email. Require an authenticated linking flow or an explicit recovery policy. GitHub App installation access and user authorization are different credentials and lifecycles. For repository operations prefer the least permissions and resource scope that satisfy the feature; do not request broad repository access for basic signup. Enterprise hosts require the appropriate configured endpoints and trust boundary, not hardcoded github.com assumptions.

## Sessions and resilience
Create an application session after resolving identity. Protect cookies and rotate session identifiers; never expose server tokens through URLs, browser storage or logs without a justified flow. Handle denied consent, rate limits, token expiration/revocation and unavailable email. A failed profile request must not silently authenticate a guessed user. Distinguish logout from disconnect/revocation.

Test wrong/missing state, reused codes, provider errors, changed username, absent email and existing-account conflicts. Use fixtures for local security decisions and document which registered callback, app permissions and real provider exchanges were actually tested. Keep retries bounded and reconcile uncertain exchanges through the documented flow.

## Primary references

Check the documentation for the deployed version when an API, support matrix or policy matters. These are reference entry points, not permission to install or deploy.

- https://docs.github.com/en/apps/oauth-apps/building-oauth-apps/authorizing-oauth-apps
- https://docs.github.com/en/apps/creating-github-apps/about-creating-github-apps/deciding-when-to-build-a-github-app
