// ============================================================================
// extension_command_bridge.cpp — Extension Command Bridge Implementation
// ============================================================================
#include "extension_command_bridge.h"
#include "../ui/command_registry.h"
#include <sstream>
#include <mutex>
#include <algorithm>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

// ============================================================================
// vscode::commands
// ============================================================================
namespace vscode {
namespace commands {

Disposable registerCommand(const std::string& commandId,
                           std::function<CommandResult(const CommandArgs&)> handler,
                           const std::string& ownerExtId)
{
    auto& reg = CommandRegistry::instance();

    // If already registered by native code, push an override; else add new.
    if (reg.hasCommand(commandId)) {
        reg.overrideCommand(commandId, handler, ownerExtId);
        return { [commandId, ownerExtId]() {
            CommandRegistry::instance().releaseOverride(commandId, ownerExtId);
        }};
    }

    // Brand new command contributed by extension
    CommandDescriptor desc;
    desc.id          = commandId;
    desc.displayName = commandId;   // Extension may set better label later
    desc.category    = "Extension";
    desc.accessModes = CMD_ACCESS_PALETTE | CMD_ACCESS_API |
                       CMD_ACCESS_EXTENSION | CMD_ACCESS_AGENTIC;
    desc.handler     = handler;
    reg.registerCommand(std::move(desc));

    return { [commandId]() {
        CommandRegistry::instance().setEnabled(commandId, false);
    }};
}

CommandResult executeCommand(const std::string& commandId, const CommandArgs& args) {
    return CommandRegistry::instance().execute(commandId, args);
}

std::vector<std::string> getCommands(bool /*includeInternal*/) {
    auto descs = CommandRegistry::instance().enumerate(CMD_ACCESS_ALL);
    std::vector<std::string> ids;
    ids.reserve(descs.size());
    for (auto& d : descs) ids.push_back(d.id);
    return ids;
}

} // namespace commands

// ============================================================================
// vscode::window
// ============================================================================
namespace window {

static HWND s_mainHwnd = nullptr;

void setMainHwnd(HWND hwnd) { s_mainHwnd = hwnd; }

void showInformationMessage(const std::string& message) {
    HWND parent = s_mainHwnd ? s_mainHwnd : nullptr;
#ifdef _WIN32
    MessageBoxA(parent, message.c_str(), "RawrXD", MB_OK | MB_ICONINFORMATION);
#endif
}

void showWarningMessage(const std::string& message) {
    HWND parent = s_mainHwnd ? s_mainHwnd : nullptr;
#ifdef _WIN32
    MessageBoxA(parent, message.c_str(), "RawrXD", MB_OK | MB_ICONWARNING);
#endif
}

void showErrorMessage(const std::string& message) {
    HWND parent = s_mainHwnd ? s_mainHwnd : nullptr;
#ifdef _WIN32
    MessageBoxA(parent, message.c_str(), "RawrXD", MB_OK | MB_ICONERROR);
#endif
}

std::optional<std::string> showInputBox(
    const std::string& prompt,
    const std::string& placeholder,
    const std::string& defaultValue,
    bool               password)
{
    (void)placeholder;
#ifdef _WIN32
    // Create a simple Win32 input dialog
    std::wstring wPrompt = prompt.empty() ? L"Input" : std::wstring(prompt.begin(), prompt.end());
    std::wstring wTitle = L"RawrXD - Input";

    // Use DialogBoxIndirect or simple approach: MessageBox + clipboard for MVP
    // For production: create custom WS_POPUP dialog with EDITTEXT
    char buf[2048] = {};
    if (!defaultValue.empty()) {
        strncpy_s(buf, defaultValue.c_str(), sizeof(buf) - 1);
    }

    // Simple input via dialog: show prompt, return default if available
    if (!defaultValue.empty()) {
        return defaultValue;
    }

    // No default — return nullopt (caller should handle)
    return std::nullopt;
#else
    (void)prompt; (void)password;
    return std::nullopt;
#endif
}

std::optional<std::string> showQuickPick(
    const std::vector<std::string>& items,
    const std::string& placeHolder,
    bool canPickMany)
{
    (void)placeHolder; (void)canPickMany;
    if (items.empty()) return std::nullopt;

    // Build a simple dialog message with numbered items
    std::string msg = "Select an item:\n\n";
    for (size_t i = 0; i < items.size(); ++i) {
        msg += std::to_string(i + 1) + ". " + items[i] + "\n";
    }

    std::wstring wMsg(msg.begin(), msg.end());
    std::wstring wTitle = std::wstring(placeHolder.begin(), placeHolder.end());
    if (wTitle.empty()) wTitle = L"RawrXD - Select";

    // Show message box and return first item (MVP)
    // Production: custom listbox dialog with selection
    MessageBoxW(nullptr, wMsg.c_str(), wTitle.c_str(), MB_OK | MB_ICONINFORMATION);
    return items.front();
}

std::optional<std::string> getActiveEditorText() {
    // Return active editor buffer content via Win32IDE message dispatch
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return std::nullopt;
    
    // Query IDE for active editor text via WM_USER + message ID
    constexpr UINT MSG_GET_EDITOR_TEXT = WM_USER + 0x2001;
    COPYDATASTRUCT cds;
    cds.dwData = MSG_GET_EDITOR_TEXT;
    cds.cbData = 0;
    cds.lpData = nullptr;
    
    LRESULT result = SendMessageA(hwnd, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&cds));
    if (result && cds.lpData && cds.cbData > 0) {
        return std::string(static_cast<const char*>(cds.lpData), cds.cbData);
    }
    return std::nullopt;
}

Disposable setStatusBarMessage(const std::string& text, int timeoutMs) {
    // Post status bar update to Win32IDE main window
    HWND hwnd = GetForegroundWindow();
    if (hwnd) {
        constexpr UINT MSG_STATUS_BAR = WM_USER + 0x2002;
        // Copy text to heap for async consumption
        char* buf = new char[text.size() + 1];
        std::memcpy(buf, text.c_str(), text.size() + 1);
        PostMessageA(hwnd, MSG_STATUS_BAR, reinterpret_cast<WPARAM>(buf), timeoutMs);
    }
    return { [text](){} };
}

} // namespace window

// ============================================================================
// vscode::workspace
// ============================================================================
namespace workspace {

std::optional<std::string> getWorkspaceRoot() {
#ifdef _WIN32
    char buf[MAX_PATH] = {};
    GetCurrentDirectoryA(MAX_PATH, buf);
    return std::string(buf);
#else
    return std::nullopt;
#endif
}

std::vector<std::string> findFiles(const std::string& include,
                                   const std::string& exclude) {
    std::vector<std::string> results;
    std::filesystem::path root = getWorkspaceRoot().value_or(".");

    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root, std::filesystem::directory_options::skip_permission_denied)) {
            if (!entry.is_regular_file()) continue;
            std::string path = entry.path().string();
            // Simple include filter (substring match)
            if (!include.empty() && path.find(include) == std::string::npos) continue;
            // Simple exclude filter (substring match)
            if (!exclude.empty() && path.find(exclude) != std::string::npos) continue;
            results.push_back(path);
        }
    } catch (const std::filesystem::filesystem_error&) {
        // Permission denied or inaccessible path — return partial results
    }
    return results;
}

std::optional<std::string> readFile(const std::string& path) {
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "rb") != 0 || !f) return std::nullopt;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string out(sz, '\0');
    fread(out.data(), 1, sz, f);
    fclose(f);
    return out;
}

bool writeFile(const std::string& path, const std::string& content) {
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "wb") != 0 || !f) return false;
    fwrite(content.data(), 1, content.size(), f);
    fclose(f);
    return true;
}

} // namespace workspace

// ============================================================================
// vscode::languages
// ============================================================================
namespace languages {

struct InlineProvider {
    std::string languageId;
    std::function<std::vector<std::string>(const std::string&, size_t)> fn;
};

static std::vector<InlineProvider> s_inlineProviders;
static std::mutex                  s_providerMutex;

Disposable registerInlineCompletionItemProvider(
    const std::string& languageId,
    std::function<std::vector<std::string>(const std::string&, size_t)> provider)
{
    std::lock_guard<std::mutex> lk(s_providerMutex);
    s_inlineProviders.push_back({ languageId, provider });
    size_t idx = s_inlineProviders.size() - 1;
    return { [idx]() {
        std::lock_guard<std::mutex> lk2(s_providerMutex);
        if (idx < s_inlineProviders.size())
            s_inlineProviders[idx].fn = nullptr;
    }};
}

Disposable registerHoverProvider(
    const std::string& languageId,
    std::function<std::string(const std::string&, size_t)> provider)
{
    std::lock_guard<std::mutex> lk(s_providerMutex);
    // Store hover provider in a static registry keyed by languageId
    static std::unordered_map<std::string, std::function<std::string(const std::string&, size_t)>> s_hoverProviders;
    s_hoverProviders[languageId] = provider;
    return { [languageId]() {
        std::lock_guard<std::mutex> lk2(s_providerMutex);
        s_hoverProviders.erase(languageId);
    }};
}

} // namespace languages

} // namespace vscode

// ============================================================================
// ExtensionCommandBridge
// ============================================================================
ExtensionCommandBridge& ExtensionCommandBridge::instance() {
    static ExtensionCommandBridge s;
    return s;
}

void ExtensionCommandBridge::onExtensionActivate(const ExtensionContext& ctx) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_extensions[ctx.extensionId] = { ctx, {}, {} };
}

void ExtensionCommandBridge::onExtensionDeactivate(const std::string& extensionId) {
    std::lock_guard<std::mutex> lk(m_mutex);
    auto it = m_extensions.find(extensionId);
    if (it == m_extensions.end()) return;

    // Release all command overrides owned by this extension
    for (const auto& cmdId : it->second.registeredCommandIds)
        CommandRegistry::instance().releaseOverride(cmdId, extensionId);

    // Delete disposables
    for (auto* d : it->second.disposables) delete d;
    m_extensions.erase(it);
}

bool ExtensionCommandBridge::isActive(const std::string& extensionId) const {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_extensions.count(extensionId) > 0;
}

std::vector<std::string> ExtensionCommandBridge::activeExtensions() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    std::vector<std::string> out;
    out.reserve(m_extensions.size());
    for (const auto& [k, _] : m_extensions)
        out.push_back(k);
    return out;
}

std::vector<std::string> ExtensionCommandBridge::commandsForExtension(
    const std::string& extId) const
{
    std::lock_guard<std::mutex> lk(m_mutex);
    auto it = m_extensions.find(extId);
    if (it == m_extensions.end()) return {};
    return it->second.registeredCommandIds;
}

std::string ExtensionCommandBridge::dumpCommandSurfaceJson() const {
    auto cmds = CommandRegistry::instance().enumerate(CMD_ACCESS_ALL);
    std::ostringstream oss;
    oss << "{\n  \"commands\": [\n";
    bool first = true;
    for (const auto& c : cmds) {
        if (!first) oss << ",\n";
        first = false;
        oss << "    {\"id\":\"" << c.id
            << "\",\"name\":\"" << c.displayName
            << "\",\"category\":\"" << c.category
            << "\",\"keybinding\":\"" << c.keybinding
            << "\",\"accessModes\":" << c.accessModes
            << ",\"enabled\":" << (c.enabled ? "true" : "false")
            << "}";
    }
    oss << "\n  ],\n  \"extensionCount\":" << m_extensions.size() << "\n}";
    return oss.str();
}
