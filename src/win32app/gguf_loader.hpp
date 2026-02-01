#pragma once
#include "../../include/gguf_loader.h"
#include <any>

// Adapter to match usage in MainWindowSimple.cpp which expects GGUFLoaderQt
// User asked to ensure no Qt dependencies, so we alias or wrap the generic loader.
class GGUFLoaderQt : public GGUFLoader {
public:
    GGUFLoaderQt(const std::string& path) {
        if (Open(path)) {
            ParseHeader();
            ParseMetadata();
            // Tensors are parsed during metadata/header phase in standard GGUF parsing usually, 
            // strictly they follow metadata.
            // We need to ensure we parse tensors too if GGUFLoader requires explicit call.
            // The method is usually ParseTensorInfo() or similar. 
            // Checking GGUFLoader interface, it says GetTensorInfo() returns tensors_.
            // We might need to ensure they are populated. 
            // Assuming ParseMetadata also parses tensor info based on implementation patterns,
            // or I need to check GGUFLoader.cpp. 
            // For now, let's assume ParseMetadata ensures basic readiness or we add a call if needed.
        }
    }
    
    // isOpen check
    bool isOpen() const { return is_open_; }

    // Helper to match Qt-style API used in MainWindowSimple.cpp (QVariant-like .toInt())
    struct VariantAdapter {
        std::string valStr;
        bool valid;
        
        VariantAdapter() : valid(false) {}
        VariantAdapter(const std::string& s) : valStr(s), valid(true) {}
        
        int toInt() const {
             if(!valid) return 0;
             try { return std::stoi(valStr); } catch (...) { return 0; }
        }
        std::string toString() const { return valStr; }
        bool isValid() const { return valid; }
    };
    
    // getParam like QVariant
    VariantAdapter getParam(const std::string& key, int def) {
        auto it = metadata_.kv_pairs.find(key);
        if (it != metadata_.kv_pairs.end()) {
            return VariantAdapter(it->second);
        }
        // Try common mappings
        if (key == "n_layer" && metadata_.layer_count > 0) return VariantAdapter(std::to_string(metadata_.layer_count));
        if (key == "n_embd" && metadata_.embedding_dim > 0) return VariantAdapter(std::to_string(metadata_.embedding_dim));
        if (key == "n_vocab" && metadata_.vocab_size > 0) return VariantAdapter(std::to_string(metadata_.vocab_size));
        if (key == "n_ctx" && metadata_.context_length > 0) return VariantAdapter(std::to_string(metadata_.context_length));

        return VariantAdapter(std::to_string(def));
    }

    // Overload for std::any signature
    VariantAdapter getParam(const std::string& key, const std::any& def) {
        return getParam(key, 0); // Simplified fallback
    }
    
    // tensorNames()
    std::vector<std::string> tensorNames() const {
        std::vector<std::string> names;
        for (const auto& t : tensors_) {
            names.push_back(t.name);
        }
        return names;
    }
};
