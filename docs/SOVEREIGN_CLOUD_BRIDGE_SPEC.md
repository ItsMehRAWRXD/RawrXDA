# Sovereign Cloud Bridge Spec (v1.3.0)

**Status:** Draft  
**Scope:** Optional cloud-model access with strict local sovereignty defaults.

## 1. Product intent

RawrXD remains local-first and telemetry-free by default. Cloud use is an opt-in bridge for users who provide their own keys and explicitly approve outbound context.

## 2. Hard guarantees

- `Default deny`: no outbound model traffic unless Cloud Bridge is enabled by user action.
- `BYOK only`: keys are user-provided; no platform-managed shared key path in v1.3.0.
- `Explicit consent`: every request with local context requires user approval policy.
- `No silent sync`: no background code upload, indexing upload, or history upload.
- `Local parity`: all core workflows continue to function with Cloud Bridge disabled.

## 3. Non-goals (v1.3.0)

- Team-wide centralized telemetry.
- Automatic cloud fallback without user-visible policy.
- Server-side project memory managed by RawrXD.

## 4. Data classes and egress policy

- `D0 Public`: prompts with no workspace content; may be sent if Cloud Bridge enabled.
- `D1 Metadata`: language, file extension, diagnostics codes; allowed by policy toggle.
- `D2 Snippets`: selected text or bounded window around caret; requires consent policy.
- `D3 Sensitive`: secrets, keys, private certs, env values, full files; blocked by default.
- `D3 Hard-deny default categories`: debugger memory and dump bytes, full workspace index export, clipboard history bulk, and raw log archives.

`D3` can only be transmitted via one-time explicit override with red warning and request preview.

## 5. Consent model

- `Per request`: default mode, user confirms each outbound request with payload preview.
- `Per session`: user approves a bounded session for one provider/model/workspace.
- `Policy allowlist`: advanced mode with explicit rules (`provider`, `workspace`, `data class`, `ttl`).
- `Scope rules`: one-shot is default; session/workspace scopes are optional and must be revocable.

All consent actions are revocable immediately from status bar and settings.

## 6. Request pipeline

1. Build local context.
2. Classify payload into `D0-D3`.
3. Run local redaction pass (secret scanner + path scrubbing).
4. Show preview and policy decision.
5. Send only approved payload subset.
6. Record local audit event.

If classification or redaction fails, request is blocked.

`No hidden bytes` guarantee: preview hash must equal payload hash at send time. If hashes differ, request is blocked and logged.

## 7. Key and transport security

- Store API keys in OS credential store (DPAPI on Windows), never plaintext in repo/workspace.
- Mask keys in UI and logs.
- TLS required; cert validation on by default.
- Optional provider endpoint pinning for enterprise deployments.

## 8. Logging and auditability

- Local-only audit log: timestamp, provider, model, data class summary, bytes sent, decision source.
- No prompt-body logging by default.
- Export formats: JSON and CSV.
- Audit controls map to existing proof workflow in `docs/PARITY_VALIDATION_PROOF_PACK.md`.
- Receipts are local-only and never uploaded by the bridge path.
- Receipt manifest includes itemization: source fragments, redaction actions, hashes, and decision metadata.

## 9. UX contract

- Clear mode badge: `Local Only` or `Cloud Bridge: <provider>`.
- Preflight panel shows exactly what leaves machine.
- One-click `Panic Off` disables cloud egress instantly, cancels active/queued cloud requests, and blocks new cloud egress until explicitly re-enabled.
- Cloud errors never degrade editor responsiveness or local completion path.

## 10. API and command surface

- `cloud_bridge.enable(provider, model)`
- `cloud_bridge.disable()`
- `cloud_bridge.preview(payload_id)`
- `cloud_bridge.consent(mode, ttl, workspace_scope)`
- `cloud_bridge.audit.export(format)`
- `cloud_bridge.panic_off()`

All commands must be wired and self-testable.

Adapter isolation rule: provider adapters cannot access workspace APIs directly. Adapters receive only finalized manifest plus approved bytes from the policy pipeline.

## 11. Acceptance criteria (ship gate)

- `CB-01`: Disabled-by-default verified on clean install.
- `CB-02`: Per-request consent preview blocks egress until approved.
- `CB-03`: D3 payload blocked unless one-time override used.
- `CB-04`: Secret scanner redacts known key/token formats before send.
- `CB-05`: Panic Off cuts active and queued cloud requests.
- `CB-06`: Audit export contains decision trail without raw prompt leakage.
- `CB-07`: Local-only path performance unchanged when bridge is disabled.

## 12. Validation hooks

Add to `self_test_gate`:

- `cloud_bridge_default_off`
- `cloud_bridge_consent_required`
- `cloud_bridge_sensitive_block`
- `cloud_bridge_redaction`
- `cloud_bridge_panic_off`
- `cloud_bridge_audit_export`
- `cloud_bridge_local_parity`

Record results in a v1.3.0 proof artifact:

- `build_validation_cloud_bridge_run1.txt`
