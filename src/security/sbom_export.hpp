#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace RawrXD {
namespace Security {

struct SbomComponent {
    std::string type;       // "file" or "library"
    std::string name;
    std::string version;
    std::string path;       // relative path or purl
    std::string purl;       // optional package URL
};

/** In-house SBOM export. Produces minimal CycloneDX 1.4 JSON. No external deps. */
class SbomExport {
public:
    SbomExport() = default;

    /** Collect components by scanning directory (src/ or given root). */
    size_t scanDirectory(const std::string& rootDir, std::vector<SbomComponent>& out);

    /** Write minimal CycloneDX 1.4 JSON to path. */
    bool writeCycloneDx(const std::string& outPath, const std::vector<SbomComponent>& components,
                        const std::string& projectName = "RawrXD", const std::string& projectVersion = "1.0.0");
};

} // namespace Security
} // namespace RawrXD
