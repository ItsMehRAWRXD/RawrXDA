#pragma once
// =============================================================================
// RawrXD IDE — Authoritative Version Constants (Phase 33: Gold Master)
// =============================================================================
// This file is the SINGLE SOURCE OF TRUTH for version strings.
// All surfaces (Win32IDE About dialog, CLI banner, HeadlessIDE, HTTP headers,
// packaging scripts) must reference these values.
//
// Bump RAWRXD_VERSION_PATCH for hotfixes.
// Bump RAWRXD_VERSION_MINOR for feature releases.
// Bump RAWRXD_VERSION_MAJOR for breaking API changes.
// =============================================================================

#define RAWRXD_VERSION_MAJOR  14
#define RAWRXD_VERSION_MINOR  5
#define RAWRXD_VERSION_PATCH  0

// Pre-computed string forms
#define RAWRXD_VERSION_STR    "14.5.0"
#define RAWRXD_VERSION_FULL   "RawrXD IDE v" RAWRXD_VERSION_STR " (SOVEREIGN)"

// Build metadata (set by CMake or compiler)
#define RAWRXD_BUILD_DATE     __DATE__
#define RAWRXD_BUILD_TIME     __TIME__

// Release channel
#define RAWRXD_CHANNEL        "SOVEREIGN-GOLD"

// Compilation unit count (updated each gold-master build)
#define RAWRXD_COMPILE_UNITS  314

// Binary target size (bytes, approximate — used by gauntlet sanity check)
#define RAWRXD_EXPECTED_SIZE_MIN  3500000
#define RAWRXD_EXPECTED_SIZE_MAX  5000000

// MASM kernel count
#define RAWRXD_MASM_KERNELS   9

// Phase count
#define RAWRXD_PHASE_COUNT    33

// Sovereign baseline (finality attestation: 31-file audit, 183+ procs, zero stubs)
#define RAWRXD_SOVEREIGN_BASELINE  "v14.5"
#define RAWRXD_FINALITY_HASH       "ARCH-SEALED-2026-03-13-145-SOV"
#define RAWRXD_SITUATIONAL_AWARE   1

// Copyright
#define RAWRXD_COPYRIGHT      "Copyright (c) 2023-2026 ItsMehRAWRXD"
#define RAWRXD_LICENSE         "MIT License"
#define RAWRXD_GITHUB          "https://github.com/ItsMehRAWRXD/RawrXD"
