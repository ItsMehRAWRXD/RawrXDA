# Enterprise license production cryptography (E14)

Last updated: 2026-03-21.

## Code anchors

- **`src/core/enterprise_license_v2.cpp`** — validation / parsing entry points for v2 payloads.
- **`src/core/enterprise_licensev2_impl.cpp`** — signing, offline cache, and enforcement implementation details.

## Production contract (must-haves)

1. **No dev placeholder keys** in release builds — signing material must be supplied via secure build pipelines, not checked into the repo.
2. **Online endpoint** — document the HTTPS contract (challenge/response, clock skew, grace period) next to the HTTP client that calls it (see license enforcer initialization in Win32 startup).
3. **Offline path** — cache file location, TTL, and revocation/blacklist handling must be covered by automated tests (see PR05 in `docs/ENHANCEMENT_15_PRODUCT_READY_7.md`).
4. **HWID stability** — machine fingerprint algorithm must be versioned so support can interpret exported bundles.

## Operator bundle (pairs with E15)

Electron **`enterprise:export-compliance-bundle`** and Win32 exports should include tier, last validation timestamp, cache state, and HWID (when available) for support tickets.
