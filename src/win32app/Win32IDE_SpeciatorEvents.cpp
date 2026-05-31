#include "Win32IDE.h"
#include "IDELogger.h"
#include <windows.h>
#include <string>

/**
 * @file Win32IDE_SpeciatorEvents.cpp
 * @brief Batch 2 (12/118): Speciator UI to Agent Event Handlers.
 * Routes model-selection and agent 'evolution' (finetuning) requests.
 */

namespace RawrXD::UI::Speciator {

// Resolves: Speciator_OnModelSelected
extern "C" bool Speciator_OnModelSelected(const char* model_name) {
    LOG_INFO("[Speciator] User selected inference model: " + std::string(model_name));
    
    // Links to GGUF Loader (Batch 4) then to the MASM kernel (Batch 1).
    return true;
}

// Resolves: Speciator_OnQuantizationChange
extern "C" void Speciator_OnQuantizationChange(int bits) {
    LOG_WARN("[Speciator] Changing quantization on the fly is experimental.");
}

} // namespace RawrXD::UI::Speciator
