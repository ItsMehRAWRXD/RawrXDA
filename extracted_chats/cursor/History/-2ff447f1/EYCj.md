# Apply the 7 optional manual edits (t7, t9, t15, t17, t18, t19, t20)

Run these in repo root. Edits are one-line or minimal.

## t9 — README
Add next to the line that mentions IDE_LAUNCH.md (e.g. in "Open items" or "Which IDE exe"):
- **Mac/Linux:** **docs/MAC_LINUX_WRAPPER.md**


## t19 — IDE_LAUNCH.md
Add a line:
- **Mac/Linux:** `./scripts/rawrxd-space.sh` (see **docs/MAC_LINUX_WRAPPER.md**).

## t20 — .gitignore
Add:
```
.rawrxd/
```
Optional comment: `# Wine rawrxd-space prefix`

## t15 — UNFINISHED_FEATURES.md
Add a bullet (e.g. under a "Dork / scanner" or "Stubs" section):
- Pure-MASM dork engine is not implemented; see PHP dork audit.

## t17 — Ship/RawrXD_Universal_Dorker.asm
Line 1 (or top comment): add:
`; thunks only; logic in C++`

## t18 — TOP_50 (file with "FILES TO ADD")
Add a row:
| scripts/rawrxd-space.sh | docs/MAC_LINUX_WRAPPER.md |

## t7 — Build/Security menu IDs
In **Win32IDE.h** and **.rc**: define `ID_BUILD_SOLUTION`, `ID_SECURITY_*` (as needed). In message/command handlers, wire them if missing.
