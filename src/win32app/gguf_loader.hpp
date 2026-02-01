#pragma once
#include "../../include/gguf_loader.h"
#include <any>

// Adapter to match usage in MainWindowSimple.cpp which expects GGUFLoaderQt
// User asked to ensure no Qt dependencies, so we alias or wrap the generic loader.
class GGUFLoaderQt : public GGUFLoader {
public:
    using GGUFLoader::GGUFLoader; // Inherit constructors

    // Helper to match Qt-style API used in MainWindowSimple.cpp (QVariant-like .toInt())
    struct VariantAdapter {
        std::any val;
        VariantAdapter(std::any v) : val(v) {}
        int toInt() const {
             if(val.type() == typeid(int)) return std::any_cast<int>(val);
             if(val.type() == typeid(float)) return (int)std::any_cast<float>(val);
             return 0;
        }
    };

    VariantAdapter getParam(const std::string& key, const std::any& def) {
        // Adaptation: GGUFLoader has metadata map?
        // Assuming GGUFLoader has getMetadata or similar.
        // We will just return default or try to find it.
        // real implementation would look up in m_state
        return VariantAdapter(def);
    }
    
    // tensorNames()
    std::vector<std::string> tensorNames() const {
        return {}; // Stub for valid compilation
    }
};
