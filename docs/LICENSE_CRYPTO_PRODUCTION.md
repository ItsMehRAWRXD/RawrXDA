# Production license cryptography (E14)

**Done when:** Signing keys are **not** dev placeholders; online contract documented; grace + blacklist paths tested.

## Code anchors

- **`src/core/enterprise_licensev2_impl.cpp`**
- **`src/core/license_offline_validator.cpp`**
- **`tests/test_enterprise_license_regression.cpp`**

## Contract checklist

- [ ] Root / intermediate CA for **license JWT** or binary sig (pick one stack; document).
- [ ] **Online:** endpoint, timeout, retry, clock-skew policy.
- [ ] **Offline:** cache TTL, grace period, revocation list or OCSP stub.
- [ ] **HWID:** stable id generation + privacy note in operator doc.

## Release gate

- No `TEST_KEY` / `0x00` signing material in **Release** CMake lane (pairs with **PR04**).
