#include "sbom_export.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

namespace RawrXD {
namespace Security {

static std::string escapeJson(const std::string& s) {
    std::ostringstream o;
    for (char c : s) {
        switch (c) {
            case '"':  o << "\\\""; break;
            case '\\': o << "\\\\"; break;
            case '\b': o << "\\b"; break;
            case '\f': o << "\\f"; break;
            case '\n': o << "\\n"; break;
            case '\r': o << "\\r"; break;
            case '\t': o << "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20)
                    o << "\\u" << std::hex << std::setfill('0') << std::setw(4) << static_cast<unsigned>(c);
                else
                    o << c;
        }
    }
    return o.str();
}

size_t SbomExport::scanDirectory(const std::string& rootDir, std::vector<SbomComponent>& out) {
    std::string root = rootDir.empty() ? "." : rootDir;
    fs::path rootPath(root);
    if (!fs::exists(rootPath) || !fs::is_directory(rootPath)) return 0;
    const std::string ext[] = { ".cpp", ".c", ".h", ".hpp", ".asm", ".rc" };
    size_t added = 0;
    try {
        for (auto it = fs::recursive_directory_iterator(rootPath, fs::directory_options::skip_permission_denied); it != fs::recursive_directory_iterator(); ++it) {
            if (!it->is_regular_file()) continue;
            std::string path = it->path().string();
            std::string rel = fs::relative(path, rootPath).string();
            for (char& c : rel) if (c == '\\') c = '/';
            bool skip = (rel.find("node_modules") != std::string::npos || rel.find(".git") != std::string::npos ||
                         rel.find("build") != std::string::npos || rel.find("3rdparty") != std::string::npos);
            if (skip) continue;
            std::string e = it->path().extension().string();
            if (std::find(std::begin(ext), std::end(ext), e) == std::end(ext)) continue;
            SbomComponent comp;
            comp.type = "file";
            comp.name = it->path().filename().string();
            comp.version = "";
            comp.path = rel;
            comp.purl = "pkg:rawrxd/" + rel + "#" + comp.name;
            out.push_back(comp);
            added++;
        }
    } catch (...) {}
    return added;
}

bool SbomExport::writeCycloneDx(const std::string& outPath, const std::vector<SbomComponent>& components,
                                 const std::string& projectName, const std::string& projectVersion) {
    std::ofstream f(outPath, std::ios::binary);
    if (!f) return false;
    f << "{\n  \"bomFormat\": \"CycloneDX\",\n  \"specVersion\": \"1.4\",\n  \"version\": 1,\n";
    f << "  \"metadata\": {\n    \"component\": {\n      \"type\": \"application\",\n      \"name\": \"" << escapeJson(projectName) << "\",\n      \"version\": \"" << escapeJson(projectVersion) << "\"\n    }\n  },\n";
    f << "  \"components\": [\n";
    for (size_t i = 0; i < components.size(); i++) {
        const auto& c = components[i];
        f << "    {\n      \"type\": \"" << (c.type == "library" ? "library" : "file") << "\",\n";
        f << "      \"name\": \"" << escapeJson(c.name) << "\",\n";
        if (!c.version.empty()) f << "      \"version\": \"" << escapeJson(c.version) << "\",\n";
        f << "      \"purl\": \"" << escapeJson(c.purl.empty() ? "pkg:rawrxd/" + c.path : c.purl) << "\"\n";
        f << "    }";
        if (i + 1 < components.size()) f << ",";
        f << "\n";
    }
    f << "  ]\n}\n";
    return f.good();
}

} // namespace Security
} // namespace RawrXD
