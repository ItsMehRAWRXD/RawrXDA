#pragma once

// Use the Qt app telemetry interface to avoid duplicate type definitions
#include "qtapp/telemetry.h"

// Simple accessor for a process-wide Telemetry instance.
// Ensures components across backend/middle/UI can record real events.
Telemetry& GetTelemetry();
