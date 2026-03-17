/**
 * @file moc_wrapper.cpp
 * @brief Explicit MOC generation wrapper for headers in include/ directory
 * @details Forces Qt MOC to process headers that AUTOMOC might miss due to path issues
 * 
 * CRITICAL: Do not remove - these includes force CMake AUTOMOC to generate
 * meta-object code for CheckpointManager, TokenizerSelector, and CICDSettings
 */

#include "../../include/checkpoint_manager.h"
#include "../../include/tokenizer_selector.h"
#include "../../include/ci_cd_settings.h"

// This file intentionally has no implementations - its sole purpose is to ensure
// MOC files are generated for the headers above before linking occurs.
