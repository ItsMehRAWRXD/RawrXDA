# Native Cross-Platform Launch (No PowerShell Required)

## Quick Start
```bash
npm install
npm run start
```
Opens `IDEre2.html` in your default browser.

## Scripts
- `start` – Launch IDE
- `audit-local` – Scan for unsafe dynamic code patterns (eval/new Function)
- `strip-comments` – Generate stripped copies of JS/d.ts into `stripped/`
- `lint` – Run ESLint with security-focused rules

## Security Notes
Sandboxed extension marketplace (no direct eval). Remaining dynamic code sites listed by `audit-local`.

## Converting Old PowerShell Flow
Legacy scripts: see `powershell-entrypoints.md`. Replace with `npm run start`.

## Next Hardening Steps (Optional)
1. Remove/patch each file flagged by audit.
2. Integrate real tests instead of placeholder.
3. Lock dependency versions and perform external vulnerability audit.
