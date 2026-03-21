#pragma once
// Canonical definition: include/rawrxd_telemetry_exports.h
// This file exists so TUs that pick up "rawrxd_telemetry_exports.h" from -Isrc
// do not duplicate symbols when another header includes the same API via
// include/ (MSVC treats the two paths as different headers if both are full copies).
#include "../include/rawrxd_telemetry_exports.h"
