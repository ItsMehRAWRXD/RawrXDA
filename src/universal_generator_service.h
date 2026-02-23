#pragma once
#include <string>

// The "GenerateAnything" function - the core entry point for the Universal Generator Service.
// This replaces the old HTTP server model with a direct C++ API call.
std::string GenerateAnything(const std::string& intent, const std::string& parameters);

