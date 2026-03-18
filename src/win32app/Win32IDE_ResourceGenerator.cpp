#include "Win32IDE.h"
#include "ide/resource_generator.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

class ResourceGeneratorManager {
public:
    ~ResourceGeneratorManager();
};

ResourceGeneratorManager::~ResourceGeneratorManager() = default;

namespace {

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string pickTemplateId(const std::string& type,
                           const std::vector<RawrXD::Resource::ResourceTemplate>& all) {
    if (all.empty()) {
        return {};
    }

    const std::string key = toLower(type);

    for (const auto& t : all) {
        if (toLower(t.id) == key || toLower(t.name) == key) {
            return t.id;
        }
    }

    for (const auto& t : all) {
        if (toLower(t.id).find(key) != std::string::npos || toLower(t.name).find(key) != std::string::npos) {
            return t.id;
        }
    }

    return all.front().id;
}

}  // namespace

std::vector<std::string> Win32IDE::getAvailableResourceGenerators() const {
    auto& gen = RawrXD::Resource::ResourceGeneratorEngine::Instance();
    const auto templates = gen.GetAllTemplates();

    std::vector<std::string> out;
    out.reserve(templates.size());
    for (const auto& t : templates) {
        out.push_back(t.id + " - " + t.name);
    }
    return out;
}

bool Win32IDE::generateResource(const std::string& type,
                                const std::string& name,
                                const std::string& outputPath,
                                const std::unordered_map<std::string, std::string>& parameters) {
    initResourceGenerator();

    auto& gen = RawrXD::Resource::ResourceGeneratorEngine::Instance();
    const auto templates = gen.GetAllTemplates();
    if (templates.empty()) {
        appendToOutput("[ResourceGen] No templates are registered.\n", "ResourceGen", OutputSeverity::Error);
        return false;
    }

    const std::string templateId = pickTemplateId(type, templates);
    std::map<std::string, std::string> params(parameters.begin(), parameters.end());
    if (params.find("projectName") == params.end()) {
        params["projectName"] = "RawrXD";
    }
    if (params.find("appName") == params.end()) {
        params["appName"] = params["projectName"];
    }

    const auto result = gen.Generate(templateId, params);
    if (!result.success) {
        appendToOutput("[ResourceGen] Generation failed: " + result.error + "\n", "ResourceGen", OutputSeverity::Error);
        return false;
    }

    std::filesystem::path path(outputPath);
    if (outputPath.empty()) {
        path = std::filesystem::current_path() / (name.empty() ? result.suggestedFilename : name);
    } else if (std::filesystem::is_directory(path)) {
        std::string file = name.empty() ? result.suggestedFilename : name;
        path /= file;
    }

    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        appendToOutput("[ResourceGen] Unable to open output file: " + path.string() + "\n", "ResourceGen", OutputSeverity::Error);
        return false;
    }
    out << result.content;
    out.close();

    appendToOutput("[ResourceGen] Generated " + templateId + " -> " + path.string() + "\n", "ResourceGen", OutputSeverity::Success);
    return true;
}

void Win32IDE::showResourceGeneratorDialog() {
    initResourceGenerator();

    auto& gen = RawrXD::Resource::ResourceGeneratorEngine::Instance();
    const auto templates = gen.GetAllTemplates();

    if (templates.empty()) {
        MessageBoxA(getMainWindow(), "No resource templates are available.", "Resource Generator", MB_OK | MB_ICONWARNING);
        return;
    }

    std::ostringstream msg;
    msg << "Available resource templates (" << templates.size() << "):\n\n";
    const size_t maxToShow = std::min<size_t>(templates.size(), 20);
    for (size_t i = 0; i < maxToShow; ++i) {
        msg << "- " << templates[i].id << " : " << templates[i].name << "\n";
    }
    if (templates.size() > maxToShow) {
        msg << "... " << (templates.size() - maxToShow) << " more\n";
    }
    msg << "\nUse Resource commands to generate files from templates.";

    appendToOutput("[ResourceGen] Listed templates in dialog.\n", "ResourceGen", OutputSeverity::Info);
    MessageBoxA(getMainWindow(), msg.str().c_str(), "Resource Generator", MB_OK | MB_ICONINFORMATION);
}
