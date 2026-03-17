#include "Win32IDE.h"
#include "../core/prompt_template_engine.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <cstdlib>
#include <sstream>
#include <iomanip>

namespace {

using RawrXD::Prompt::PromptTemplate;
using RawrXD::Prompt::PromptTemplateEngine;
using RawrXD::Prompt::TemplateType;

std::string wideToUtf8(const wchar_t* wide) {
    if (!wide || !*wide) return {};
    const int needed = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (needed <= 1) return {};
    std::string out(static_cast<size_t>(needed - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, out.data(), needed, nullptr, nullptr);
    return out;
}

bool loadJsonFromFile(const std::string& path, nlohmann::json& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return false;
    }
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (text.empty()) {
        return false;
    }
    out = nlohmann::json::parse(text, nullptr, false);
    return !out.is_discarded();
}

std::string getTemplateStorePath(const Win32IDE* ide) {
    const char* appData = std::getenv("APPDATA");
    std::filesystem::path root = appData ? std::filesystem::path(appData) : std::filesystem::current_path();
    root /= "RawrXD";
    root /= "workspaces";

    (void)ide;
    std::string workspaceKey = std::filesystem::current_path().string();

    // Stable workspace folder hash (FNV-1a 64-bit hex).
    uint64_t h = 14695981039346656037ull;
    for (unsigned char c : workspaceKey) {
        h ^= c;
        h *= 1099511628211ull;
    }
    std::ostringstream hs;
    hs << std::hex << std::setfill('0') << std::setw(16) << h;

    root /= hs.str();
    std::error_code ec;
    std::filesystem::create_directories(root, ec);
    return (root / "prompt_templates.json").string();
}

void loadUserTemplates(Win32IDE* ide, PromptTemplateEngine& engine) {
    const std::string path = getTemplateStorePath(ide);
    nlohmann::json j;
    if (!loadJsonFromFile(path, j)) {
        return;
    }

    if (!j.is_object() || !j.contains("templates") || !j["templates"].is_array()) {
        return;
    }

    for (const auto& entry : j["templates"]) {
        if (!entry.is_object()) continue;
        const std::string id = entry.value("id", "");
        const std::string body = entry.value("templateText", "");
        if (id.empty() || body.empty()) continue;

        PromptTemplate t{};
        t.id = id;
        t.name = entry.value("name", id);
        t.type = TemplateType::CUSTOM;
        t.templateText = body;
        t.systemPrompt = entry.value("systemPrompt", "");
        t.maxTokens = entry.value("maxTokens", 4096);
        t.temperature = entry.value("temperature", 0.4f);
        engine.registerTemplate(t);
    }
}

void saveUserTemplate(Win32IDE* ide, const PromptTemplate& tmpl) {
    const std::string path = getTemplateStorePath(ide);
    nlohmann::json root = nlohmann::json::object();
    (void)loadJsonFromFile(path, root);
    if (!root.is_object()) {
        root = nlohmann::json::object();
    }

    if (!root.contains("templates") || !root["templates"].is_array()) {
        root["templates"] = nlohmann::json::array();
    }

    bool replaced = false;
    for (auto& entry : root["templates"]) {
        if (!entry.is_object()) continue;
        if (entry.value("id", "") == tmpl.id) {
            entry["name"] = tmpl.name;
            entry["templateText"] = tmpl.templateText;
            entry["systemPrompt"] = tmpl.systemPrompt;
            entry["maxTokens"] = tmpl.maxTokens;
            entry["temperature"] = tmpl.temperature;
            replaced = true;
            break;
        }
    }

    if (!replaced) {
        root["templates"].push_back({
            {"id", tmpl.id},
            {"name", tmpl.name},
            {"templateText", tmpl.templateText},
            {"systemPrompt", tmpl.systemPrompt},
            {"maxTokens", tmpl.maxTokens},
            {"temperature", tmpl.temperature}
        });
    }

    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) {
        return;
    }
    out << root.dump(2);
}

void deleteUserTemplate(Win32IDE* ide, const std::string& id) {
    const std::string path = getTemplateStorePath(ide);
    nlohmann::json root = nlohmann::json::object();
    if (!loadJsonFromFile(path, root) || !root.is_object()) {
        return;
    }
    if (!root.contains("templates") || !root["templates"].is_array()) {
        return;
    }

    auto& arr = root["templates"];
    arr.erase(std::remove_if(arr.begin(), arr.end(),
        [&](const nlohmann::json& v) { return v.is_object() && v.value("id", "") == id; }),
        arr.end());

    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) return;
    out << root.dump(2);
}

} // namespace

void Win32IDE::ShowPromptTemplateManager() {
    auto& engine = PromptTemplateEngine::Global();
    static bool s_promptTemplatesLoaded = false;
    if (!s_promptTemplatesLoaded) {
        engine.registerBuiltins();
        loadUserTemplates(this, engine);
        s_promptTemplatesLoaded = true;
    }

    const auto templates = engine.listTemplates();
    std::string msg = "Prompt templates available: " + std::to_string(templates.size()) + "\n\n";
    for (const auto& id : templates) {
        msg += " - " + id + "\n";
    }
    msg += "\nCreate or update a custom template?";

    const int choice = MessageBoxA(
        m_hwndMain,
        (msg + "\nYes = Create/Update, No = Delete, Cancel = Close").c_str(),
        "Prompt Template Library",
        MB_YESNOCANCEL | MB_ICONINFORMATION);
    if (choice == IDCANCEL) {
        appendToOutput("[PromptLibrary] Listed " + std::to_string(templates.size()) + " template(s).\n",
                       "Output", OutputSeverity::Info);
        return;
    }

    wchar_t idBuf[128] = L"custom_project_template";
    if (!DialogBoxWithInput(L"Prompt Template ID",
                            L"Template ID (letters/numbers/_ recommended):",
                            idBuf, sizeof(idBuf) / sizeof(idBuf[0]))) {
        return;
    }

    const std::string id = wideToUtf8(idBuf);
    if (id.empty()) {
        MessageBoxA(m_hwndMain, "Template ID is required.", "Prompt Template", MB_OK | MB_ICONWARNING);
        return;
    }

    if (choice == IDNO) {
        engine.removeTemplate(id);
        deleteUserTemplate(this, id);
        appendToOutput("[PromptLibrary] Deleted template: " + id + "\n", "Output", OutputSeverity::Info);
        MessageBoxA(m_hwndMain, ("Deleted template: " + id).c_str(), "Prompt Template", MB_OK | MB_ICONINFORMATION);
        return;
    }

    wchar_t bodyBuf[4096] = L"{{ user_message }}";
    if (!DialogBoxWithInput(L"Prompt Template Body",
                            L"Template text (Jinja-style variables, e.g. {{ user_message }}):",
                            bodyBuf, sizeof(bodyBuf) / sizeof(bodyBuf[0]))) {
        return;
    }

    const std::string body = wideToUtf8(bodyBuf);
    if (body.empty()) {
        MessageBoxA(m_hwndMain, "Template body is required.", "Prompt Template", MB_OK | MB_ICONWARNING);
        return;
    }

    PromptTemplate userTemplate{};
    userTemplate.id = id;
    userTemplate.name = id;
    userTemplate.type = TemplateType::CUSTOM;
    userTemplate.templateText = body;
    userTemplate.maxTokens = 4096;
    userTemplate.temperature = 0.4f;
    userTemplate.requiredVars = {"user_message"};

    engine.registerTemplate(userTemplate);
    saveUserTemplate(this, userTemplate);

    appendToOutput("[PromptLibrary] Saved custom template: " + id + "\n", "Output", OutputSeverity::Info);
    MessageBoxA(m_hwndMain, ("Saved template: " + id).c_str(), "Prompt Template", MB_OK | MB_ICONINFORMATION);
}
