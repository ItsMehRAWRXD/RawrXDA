#pragma once

#include <cstdint>
#include <string>

// Forward declaration
struct AppState;

namespace BaselineProfile {

/**
 * Detect CPU baseline frequency and stable overclock offset
 * @param state AppState to populate with detection results
 * @return true if detection successful
 */
bool DetectBaseline(AppState& state);

/**
 * Load saved baseline profile from disk
 * @param state AppState to populate
 * @return true if profile loaded successfully
 */
bool LoadProfile(AppState& state);

/**
 * Save current baseline to disk
 * @param state AppState containing baseline data
 * @return true if save successful
 */
bool SaveProfile(const AppState& state);

} // namespace BaselineProfile
