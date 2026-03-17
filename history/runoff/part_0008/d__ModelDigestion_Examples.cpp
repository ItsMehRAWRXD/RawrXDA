/**
 * EXAMPLE: Model Digestion with RawrZ Integration
 * 
 * Demonstrates how to integrate the complete pipeline into your system:
 * - Model loading from encrypted BLOB
 * - RawrZ polymorphic decryption
 * - Inference execution
 * - Output handling
 */

// ============================================================================
// EXAMPLE 1: Basic Model Loading
// ============================================================================

#include "ModelDigestion.hpp"

void Example_BasicModelLoading() {
    using namespace ModelDigestion;

    std::string modelBasePath = "encrypted_models/llama2-800b";
    
    // Auto-load with discovery
    if (EncryptedModelLoader::AutoLoad(modelBasePath)) {
        printf("[SUCCESS] Model loaded and ready for inference\n");
    } else {
        printf("[ERROR] Failed to load encrypted model\n");
    }
}

// ============================================================================
// EXAMPLE 2: Full Integration with IDE
// ============================================================================

#include "ModelDigestion.hpp"
#include <windows.h>

class RawrXDModelIntegration {
public:
    static void IntegrateIntoIDEUI(HWND hwndOutput) {
        // Add model loading button to UI
        std::string status = "🔐 Encrypted Model System\n";
        status += "─────────────────────────────\n";
        status += "Status: Ready\n";
        status += "Format: AES-256-GCM (Carmilla)\n";
        status += "Protection: RawrZ1 Polymorphic\n";
        status += "Model Size: ~800B parameters\n";
        
        // Display in IDE output
        AppendWindowText(hwndOutput, 
            std::wstring(status.begin(), status.end()).c_str());
    }

    static bool LoadModelOnStartup(HWND hwndOutput) {
        AppendWindowText(hwndOutput, L"🔄 Loading encrypted model...\n");

        // Load model
        if (EncryptedModelLoader::AutoLoad("encrypted_models/llama2-800b")) {
            AppendWindowText(hwndOutput, 
                L"✅ Encrypted model initialized\n");
            return true;
        } else {
            AppendWindowText(hwndOutput,
                L"❌ Model initialization failed\n");
            return false;
        }
    }

    static void ShowModelInfo(HWND hwndOutput) {
        auto meta = GetModelMetadata();
        
        wchar_t buffer[512];
        swprintf_s(buffer, sizeof(buffer)/sizeof(wchar_t),
            L"📊 Model Information\n"
            L"   Vocab Size: %u\n"
            L"   Context: %u tokens\n"
            L"   Layers: %u\n"
            L"   Heads: %u\n"
            L"   Hidden: %u\n",
            meta->vocabSize,
            meta->contextLength,
            meta->layerCount,
            meta->headCount,
            meta->hiddenDim
        );
        
        AppendWindowText(hwndOutput, buffer);
    }
};

// ============================================================================
// EXAMPLE 3: Inference Pipeline
// ============================================================================

struct AICompletion {
    std::wstring text;
    float confidence;
};

AICompletion InferWithEncryptedModel(const std::string& prompt) {
    using namespace ModelDigestion;

    // Get inference context
    auto context = EncryptedModelLoader::CreateContext(
        "encrypted_models/llama2-800b/model.digested.blob"
    );

    if (!context.modelData) {
        return { L"[Error loading model]", 0.0f };
    }

    // Prepare inference request
    EncryptedModelInference::InferenceRequest request;
    request.temperature = 0.7f;
    request.maxTokens = 128;
    request.useCache = true;

    // Convert prompt to token IDs
    // (simplified - real implementation would use tokenizer)
    for (char c : prompt) {
        request.tokenIds.push_back((int)c);
    }

    try {
        // Run inference
        auto result = EncryptedModelInference::Infer(context, request);

        // Convert tokens to text
        std::wstring output;
        for (int token : result.generatedTokens) {
            output += (wchar_t)token;
        }

        return { output, result.confidence };
    }
    catch (const std::exception& e) {
        return { L"[Inference failed]", 0.0f };
    }
}

// ============================================================================
// EXAMPLE 4: RawrXD IDE Integration (Main file changes)
// ============================================================================

/*
Add this to RawrXD_Win32_IDE.cpp:

// Global model context
static bool g_encryptedModelLoaded = false;
static EncryptedModelLoader::InferenceContext g_modelContext;

// In WM_CREATE handler:
case WM_CREATE: {
    // ... existing code ...
    
    // Load encrypted model
    if (RawrXDModelIntegration::LoadModelOnStartup(g_hwndOutput)) {
        g_encryptedModelLoaded = true;
        g_modelContext = EncryptedModelLoader::CreateContext(
            "encrypted_models/llama2-800b/model.digested.blob"
        );
        RawrXDModelIntegration::ShowModelInfo(g_hwndOutput);
    }
    
    break;
}

// In AI completion handler:
void TriggerAICompletion() {
    if (!g_encryptedModelLoaded) {
        AppendWindowText(g_hwndOutput, L"⚠️ Model not loaded\n");
        return;
    }

    // Get selected code
    wchar_t selectedText[4096] = {};
    // ... get selection ...

    // Convert to string
    std::string prompt(selectedText, selectedText + wcslen(selectedText));

    // Infer with encrypted model
    auto completion = InferWithEncryptedModel(prompt);

    // Show result
    AppendWindowText(g_hwndOutput, completion.text.c_str());
}

// In WM_DESTROY handler:
case WM_DESTROY: {
    // Clean up model
    if (g_modelContext.modelData) {
        VirtualFree(g_modelContext.modelData, 0, MEM_RELEASE);
    }
    
    PostQuitMessage(0);
    break;
}
*/

// ============================================================================
// EXAMPLE 5: Advanced: Caching and Optimization
// ============================================================================

class EncryptedModelCache {
private:
    static EncryptedModelLoader::InferenceContext g_cachedContext;
    static bool g_isCached;

public:
    static bool LoadAndCache(const std::string& modelPath) {
        if (g_isCached) {
            return true;  // Already cached
        }

        g_cachedContext = EncryptedModelLoader::CreateContext(modelPath);
        g_isCached = (g_cachedContext.modelData != nullptr);
        
        return g_isCached;
    }

    static EncryptedModelLoader::InferenceContext GetCached() {
        return g_cachedContext;
    }

    static void ClearCache() {
        if (g_cachedContext.modelData) {
            VirtualFree(g_cachedContext.modelData, 0, MEM_RELEASE);
            g_cachedContext.modelData = nullptr;
        }
        g_isCached = false;
    }

    static size_t GetCacheSize() {
        return g_isCached ? g_cachedContext.modelSize : 0;
    }
};

// ============================================================================
// EXAMPLE 6: Error Handling and Recovery
// ============================================================================

bool SafeLoadEncryptedModel(const std::string& modelPath, 
                            int maxRetries = 3) {
    using namespace ModelDigestion;

    for (int attempt = 0; attempt < maxRetries; ++attempt) {
        try {
            if (EncryptedModelLoader::AutoLoad(modelPath)) {
                printf("✅ Model loaded successfully (attempt %d)\n", attempt + 1);
                return true;
            }
        }
        catch (const std::exception& e) {
            printf("⚠️  Attempt %d failed: %s\n", attempt + 1, e.what());
            
            if (attempt < maxRetries - 1) {
                Sleep(500);  // Wait before retry
                continue;
            }
        }
    }

    printf("❌ Failed to load model after %d attempts\n", maxRetries);
    return false;
}

// ============================================================================
// EXAMPLE 7: Monitoring and Diagnostics
// ============================================================================

void DiagnoseModelState() {
    using namespace ModelDigestion;

    printf("\n═══════════════════════════════════════════════\n");
    printf("  MODEL DIGESTION SYSTEM DIAGNOSTICS\n");
    printf("═══════════════════════════════════════════════\n\n");

    // Check configuration
    printf("Configuration:\n");
    printf("  Vocab Size:      %u\n", ModelDigestionConfig::VOCAB_SIZE);
    printf("  Context Length:  %u\n", ModelDigestionConfig::CONTEXT_LENGTH);
    printf("  Layer Count:     %u\n", ModelDigestionConfig::LAYER_COUNT);
    printf("  Hidden Dim:      %u\n", ModelDigestionConfig::HIDDEN_DIM);
    printf("  Head Count:      %u\n", ModelDigestionConfig::HEAD_COUNT);

    printf("\nEncryption:\n");
    printf("  Algorithm:       AES-256-GCM\n");
    printf("  Key Size:        %u bits\n", ModelDigestionConfig::KEY_SIZE * 8);
    printf("  IV Size:         %u bits\n", ModelDigestionConfig::IV_SIZE * 8);
    printf("  Auth Tag Size:   %u bits\n", ModelDigestionConfig::AUTH_TAG_SIZE * 8);

    printf("\nSecurity:\n");
    printf("  Anti-Debug:      %s\n", 
        ModelDigestionConfig::ANTI_DEBUG_ENABLED ? "Enabled" : "Disabled");
    printf("  Checksum:        %s\n",
        ModelDigestionConfig::VERIFY_CHECKSUM ? "Enabled" : "Disabled");

    printf("\nPerformance:\n");
    printf("  AES-NI:          %s\n",
        ModelDigestionConfig::USE_AES_NI ? "Enabled" : "Disabled");
    printf("  Model Cache:     %s\n",
        ModelDigestionConfig::CACHE_DECRYPTED_MODEL ? "Enabled" : "Disabled");
    printf("  Max Model Size:  %llu MB\n",
        ModelDigestionConfig::MAX_MODEL_SIZE / (1024 * 1024));

    printf("\n═══════════════════════════════════════════════\n\n");
}

// ============================================================================
// EXAMPLE 8: Deployment Verification
// ============================================================================

bool VerifyDeploymentIntegrity() {
    using namespace ModelDigestion;

    printf("🔍 Verifying deployment integrity...\n\n");

    // Check required files
    std::vector<std::string> requiredFiles = {
        "encrypted_models/llama2-800b/model.digested.blob",
        "encrypted_models/llama2-800b/model.digested.manifest.json",
        "ModelDigestion.hpp",
        "model.digestion.lib"
    };

    bool allPresent = true;
    for (const auto& file : requiredFiles) {
        HANDLE hFile = CreateFileA(
            file.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr
        );

        if (hFile != INVALID_HANDLE_VALUE) {
            printf("✅ %s\n", file.c_str());
            CloseHandle(hFile);
        } else {
            printf("❌ %s (MISSING)\n", file.c_str());
            allPresent = false;
        }
    }

    printf("\n%s Deployment ready for integration\n",
        allPresent ? "✅" : "❌");

    return allPresent;
}

// ============================================================================
// MAIN ENTRY POINT
// ============================================================================

int main() {
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  MODEL DIGESTION SYSTEM - INTEGRATION EXAMPLES            ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");

    // Run diagnostics
    DiagnoseModelState();

    // Verify deployment
    if (!VerifyDeploymentIntegrity()) {
        return 1;
    }

    // Load model with retry logic
    if (!SafeLoadEncryptedModel("encrypted_models/llama2-800b")) {
        return 2;
    }

    // Show model info
    // (Would be called from IDE)

    printf("\n✅ All examples executed successfully!\n");
    return 0;
}
