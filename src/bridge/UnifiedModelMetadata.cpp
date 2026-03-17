#include "UnifiedModelMetadata.h"
namespace RawrXD::Bridge {
    UnifiedModelMetadata MetadataRegistry::s_active{};
    uint64_t UnifiedModelMetadata::capability_hash() const {
        return std::hash<std::string>{}(family + quantization + std::to_string(context_length));
    }
}
