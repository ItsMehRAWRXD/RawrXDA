# Canonical repository root

**Single source of truth:** the RawrXD tree lives at **`D:\RawrXD`** on the primary Windows build host.

- Do not rely on Cursor worktrees, `%USERPROFILE%\.cursor\worktrees\…`, or copies under other drives for production builds, scripts, or documentation examples.
- Clone or sync the repo to **`D:\RawrXD`** (case-insensitive on NTFS, but use this spelling in docs and defaults).
- Optional override: set environment variable **`RAWRXD_REPO_ROOT`** to an absolute path if you must build from another location; tools and C++ search paths honor it where implemented.

CI and other machines should pass **`RAWRXD_REPO_ROOT`** explicitly rather than hardcoding `D:\RawrXD`.