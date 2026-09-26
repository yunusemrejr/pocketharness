# Google Identity Integration: patterns and examples

## Choose the flow
Use Google Identity Services or a maintained OIDC library appropriate to the actual client. A Gmail address is not proof of account ownership. Sign-in may need only identity scopes; accessing Drive, Calendar or Gmail requires separate justified OAuth consent. Browser public clients cannot keep a client secret. Native apps require the supported redirect and system-browser flow rather than an invented embedded password screen.

## Validate the boundary
For an ID token verify signature with current provider keys, allowed algorithm, expected issuer, audience and expiry; validate nonce when used by the chosen flow. Handle key rotation using the library's bounded cache. Token decoding is not verification. Authorization-code flows need redirect URI matching, transaction correlation/state and PKCE as supported/required by the selected client flow. State and nonce have different roles; use the library's documented CSRF protections for GIS credential POST flows rather than forcing a mismatched recipe.

Store an identity key such as `(issuer, sub)`, not mutable email. Check required hosted-domain policy from validated claims and documented semantics, not a user-entered email suffix. `email_verified` does not authorize access to an existing local account automatically. Linking identities should require an authenticated user and reauthentication/confirmation appropriate to the application; do not silently merge on email collision.

## Application session
After verification create your own session with secure cookie settings and rotation. An ID token is not an access token for arbitrary Google APIs. Keep refresh tokens encrypted server-side where used, request minimal scopes and handle revocation/expiry. Logging out of the app and revoking Google consent are different actions; implement the requested behavior accurately.

Test wrong audience/issuer, expired token, replayed login state, denied consent, duplicate email, account linking and redirect mismatch with controlled fixtures. Mock verification proves local decisions only; label provider configuration and real consent-screen behavior as unverified until tested. Never print tokens or client secrets in diagnostics.

## Primary references

Check the documentation for the deployed version when an API, support matrix or policy matters. These are reference entry points, not permission to install or deploy.

- https://developers.google.com/identity/openid-connect/openid-connect
- https://developers.google.com/identity/gsi/web
- https://developers.google.com/identity/protocols/oauth2
