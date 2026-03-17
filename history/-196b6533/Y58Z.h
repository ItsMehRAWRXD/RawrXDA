#pragma once

#include "telemetry.h"

// Simple accessor for a process-wide Telemetry instance.
// Ensures components across backend/middle/UI can record real events.
Telemetry& GetTelemetry();
