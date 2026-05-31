// ============================================================================
// missing_features.cpp - Implemented integration subset for IDEFeatures facade
// ============================================================================

#include "missing_features.hpp"

namespace rawrxd {

void MultiCursorManager::addCursor(int line, int column)
{
    CursorPosition cp{line, column};
    for (const auto& c : cursors)
    {
        if (c == cp)
            return;
    }
    cursors.push_back(cp);
    std::sort(cursors.begin(), cursors.end());
    updatePrimaryIndex();
}

void MultiCursorManager::removeCursor(size_t index)
{
    if (index < cursors.size())
        cursors.erase(cursors.begin() + static_cast<long long>(index));
    updatePrimaryIndex();
}

void MultiCursorManager::clearCursors()
{
    cursors.clear();
    cursors.push_back({0, 0});
    primaryCursorIndex = 0;
}

void MultiCursorManager::setPrimaryCursor(size_t index)
{
    if (index < cursors.size())
        primaryCursorIndex = index;
}

CursorPosition& MultiCursorManager::primary()
{
    static CursorPosition dummy;
    return cursors.empty() ? dummy : cursors[primaryCursorIndex];
}

const CursorPosition& MultiCursorManager::primary() const
{
    static CursorPosition dummy;
    return cursors.empty() ? dummy : cursors[primaryCursorIndex];
}

void MultiCursorManager::moveAll(int deltaLine, int deltaColumn, const std::vector<std::string>& lines)
{
    const int maxLine = static_cast<int>(lines.empty() ? 0 : lines.size() - 1);
    for (auto& c : cursors)
    {
        c.line = std::max(0, std::min(maxLine, c.line + deltaLine));
        const int maxCol = (c.line >= 0 && c.line < static_cast<int>(lines.size())) ? static_cast<int>(lines[c.line].size()) : 0;
        c.column = std::max(0, std::min(maxCol, c.column + deltaColumn));
    }
    removeDuplicates();
}

std::vector<std::pair<int, int>> MultiCursorManager::insertAtAll(const std::string& text, std::vector<std::string>& lines)
{
    std::vector<std::pair<int, int>> out;
    if (text.empty())
        return out;

    std::vector<size_t> idx(cursors.size());
    for (size_t i = 0; i < idx.size(); ++i)
        idx[i] = i;

    std::sort(idx.begin(), idx.end(), [this](size_t a, size_t b) { return cursors[a] < cursors[b]; });

    for (auto it = idx.rbegin(); it != idx.rend(); ++it)
    {
        auto& c = cursors[*it];
        if (c.line < 0 || c.line >= static_cast<int>(lines.size()))
            continue;
        auto& line = lines[c.line];
        const int col = std::max(0, std::min(static_cast<int>(line.size()), c.column));
        line.insert(static_cast<size_t>(col), text);
        c.column = col + static_cast<int>(text.size());
        out.push_back({c.line, c.column});
    }
    return out;
}

void MultiCursorManager::deleteAtAll(std::vector<std::string>& lines, bool forward)
{
    std::vector<size_t> idx(cursors.size());
    for (size_t i = 0; i < idx.size(); ++i)
        idx[i] = i;

    std::sort(idx.begin(), idx.end(), [this](size_t a, size_t b) { return cursors[a] < cursors[b]; });

    for (auto it = idx.rbegin(); it != idx.rend(); ++it)
    {
        auto& c = cursors[*it];
        if (c.line < 0 || c.line >= static_cast<int>(lines.size()))
            continue;
        auto& line = lines[c.line];
        if (forward)
        {
            if (c.column >= 0 && c.column < static_cast<int>(line.size()))
                line.erase(static_cast<size_t>(c.column), 1);
        }
        else
        {
            if (c.column > 0 && c.column <= static_cast<int>(line.size()))
            {
                line.erase(static_cast<size_t>(c.column - 1), 1);
                --c.column;
            }
        }
    }
    removeDuplicates();
}

void MultiCursorManager::updatePrimaryIndex()
{
    if (cursors.empty())
        primaryCursorIndex = 0;
    else if (primaryCursorIndex >= cursors.size())
        primaryCursorIndex = cursors.size() - 1;
}

void MultiCursorManager::removeDuplicates()
{
    std::set<CursorPosition> seen;
    std::vector<CursorPosition> uniq;
    for (const auto& c : cursors)
    {
        if (!seen.count(c))
        {
            seen.insert(c);
            uniq.push_back(c);
        }
    }
    cursors = std::move(uniq);
    std::sort(cursors.begin(), cursors.end());
    updatePrimaryIndex();
}

void BreadcrumbProvider::updateFromPath(const std::string& filePath)
{
    items.clear();
    std::string token;
    for (char ch : filePath)
    {
        if (ch == '/' || ch == '\\')
        {
            if (!token.empty())
            {
                BreadcrumbItem b;
                b.label = token;
                b.kind = "folder";
                b.icon = "folder";
                items.push_back(std::move(b));
                token.clear();
            }
            continue;
        }
        token.push_back(ch);
    }
    if (!token.empty())
    {
        BreadcrumbItem b;
        b.label = token;
        b.kind = token.find('.') == std::string::npos ? "folder" : "file";
        b.icon = b.kind == "file" ? "file" : "folder";
        items.push_back(std::move(b));
    }
}

void BreadcrumbProvider::addSymbolBreadcrumb(const std::string& name, const std::string& kind, int line, int column)
{
    BreadcrumbItem b;
    b.label = name;
    b.kind = kind;
    b.line = line;
    b.column = column;
    b.icon = "symbol";
    items.push_back(std::move(b));
}

std::string BreadcrumbProvider::render() const
{
    std::string out;
    for (size_t i = 0; i < items.size(); ++i)
    {
        out += items[i].label;
        if (i + 1 < items.size())
            out += " > ";
    }
    return out;
}

void Snippet::parse(const std::string& snippetBody)
{
    body = snippetBody;
    bodyLines.clear();
    std::stringstream ss(snippetBody);
    std::string line;
    while (std::getline(ss, line))
        bodyLines.push_back(line);
}

std::vector<std::string> Snippet::expand() const
{
    return bodyLines;
}

void SnippetManager::loadSnippets(const std::string& language, const std::map<std::string, std::string>& snippetDefs)
{
    auto& target = snippetsByLanguage[language];
    for (const auto& kv : snippetDefs)
    {
        Snippet s;
        s.prefix = kv.first;
        s.name = kv.first;
        s.body = kv.second;
        s.parse(kv.second);
        target.push_back(s);
        snippetsByPrefix[s.prefix] = s;
    }
}

std::vector<Snippet*> SnippetManager::getCompletions(const std::string& prefix, const std::string& language)
{
    std::vector<Snippet*> out;
    auto it = snippetsByLanguage.find(language);
    if (it != snippetsByLanguage.end())
    {
        for (auto& s : it->second)
        {
            if (s.prefix.rfind(prefix, 0) == 0)
                out.push_back(&s);
        }
    }
    return out;
}

void SnippetManager::loadDefaults()
{
    loadSnippets("cpp", {{"main", "int main() {\n\t$0\n}\n"}, {"fori", "for (int i = 0; i < n; ++i) {\n\t$0\n}\n"}});
    loadSnippets("python", {{"def", "def func():\n\t$0\n"}, {"ifmain", "if __name__ == '__main__':\n\t$0\n"}});
}

std::shared_ptr<EditorTab> TabGroup::addTab(const std::string& path, const std::string& name)
{
    auto tab = std::make_shared<EditorTab>();
    tab->path = path;
    tab->name = name.empty() ? path : name;
    tab->id = tab->name + "_" + std::to_string(tabs.size());
    tabs.push_back(tab);
    return tab;
}

void TabGroup::removeTab(const std::string& tabId)
{
    tabs.erase(std::remove_if(tabs.begin(), tabs.end(), [&](const std::shared_ptr<EditorTab>& t) { return t->id == tabId; }), tabs.end());
    if (activeTabIndex >= tabs.size())
        activeTabIndex = tabs.empty() ? 0 : (tabs.size() - 1);
}

std::shared_ptr<EditorTab> TabGroup::activeTab() { return activeTabIndex < tabs.size() ? tabs[activeTabIndex] : nullptr; }

void TabGroup::activateTab(const std::string& tabId)
{
    for (size_t i = 0; i < tabs.size(); ++i)
    {
        if (tabs[i]->id == tabId)
        {
            activeTabIndex = i;
            break;
        }
    }
}

void TabGroup::moveTab(size_t from, size_t to)
{
    if (from >= tabs.size() || to >= tabs.size() || from == to)
        return;
    auto t = tabs[from];
    tabs.erase(tabs.begin() + static_cast<long long>(from));
    tabs.insert(tabs.begin() + static_cast<long long>(to), t);
    activeTabIndex = to;
}

void LayoutNode::split(SplitDirection dir, bool secondGroup)
{
    direction = dir;
    auto oldGroup = tabGroup;
    tabGroup.reset();
    first = std::make_shared<LayoutNode>();
    second = std::make_shared<LayoutNode>();
    first->parent = this;
    second->parent = this;
    first->tabGroup = secondGroup ? std::make_shared<TabGroup>() : oldGroup;
    second->tabGroup = secondGroup ? oldGroup : std::make_shared<TabGroup>();
}

void WorkspaceLayoutManager::init()
{
    root = std::make_shared<LayoutNode>();
    root->id = "root";
    root->tabGroup = std::make_shared<TabGroup>();
    root->tabGroup->id = "primary";
    root->tabGroup->isPrimary = true;
    allGroups.clear();
    allGroups.push_back(root->tabGroup);
}

TabGroup* WorkspaceLayoutManager::primaryGroup() { return root && root->tabGroup ? root->tabGroup.get() : nullptr; }

std::shared_ptr<TabGroup> WorkspaceLayoutManager::addGroup(TabGroup*, SplitDirection)
{
    auto g = std::make_shared<TabGroup>();
    g->id = "group_" + std::to_string(allGroups.size());
    g->isPrimary = false;
    allGroups.push_back(g);
    return g;
}

void WorkspaceLayoutManager::removeGroup(TabGroup* group)
{
    if (!group || allGroups.size() <= 1)
        return;
    allGroups.erase(std::remove_if(allGroups.begin(), allGroups.end(), [&](const std::shared_ptr<TabGroup>& g) { return g.get() == group; }),
                    allGroups.end());
}

void WorkspaceLayoutManager::setSplitRatio(LayoutNode* node, float ratio)
{
    if (node)
        node->splitRatio = std::max(0.1f, std::min(0.9f, ratio));
}

LayoutNode* WorkspaceLayoutManager::findNode(LayoutNode* node, TabGroup* group)
{
    if (!node)
        return nullptr;
    if (node->isLeaf() && node->tabGroup.get() == group)
        return node;
    if (node->first)
    {
        if (auto* f = findNode(node->first.get(), group))
            return f;
    }
    if (node->second)
    {
        if (auto* s = findNode(node->second.get(), group))
            return s;
    }
    return nullptr;
}

void MultiRootWorkspace::addFolder(const std::string& path, const std::string& name)
{
    WorkspaceFolder f;
    f.path = path;
    f.uri = "file://" + path;
    f.name = name.empty() ? path : name;
    f.index = static_cast<int>(folders.size());
    folders.push_back(std::move(f));
}

bool MultiRootWorkspace::load(const std::string& path)
{
    workspaceFile = path;
    std::ifstream in(path);
    if (!in)
        return false;
    folders.clear();
    std::string line;
    while (std::getline(in, line))
    {
        auto key = line.find("\"path\"");
        if (key == std::string::npos)
            continue;
        auto q1 = line.find('"', key + 6);
        auto q2 = (q1 == std::string::npos) ? std::string::npos : line.find('"', q1 + 1);
        if (q1 != std::string::npos && q2 != std::string::npos)
            addFolder(line.substr(q1 + 1, q2 - q1 - 1));
    }
    return true;
}

void MultiRootWorkspace::save(const std::string& path)
{
    workspaceFile = path;
    std::ofstream out(path);
    if (!out)
        return;
    out << "{\n  \"folders\": [\n";
    for (size_t i = 0; i < folders.size(); ++i)
    {
        out << "    { \"path\": \"" << folders[i].path << "\" }" << (i + 1 < folders.size() ? "," : "") << "\n";
    }
    out << "  ]\n}\n";
}

void TerminalManager::loadProfiles()
{
    if (!profiles.empty())
        return;
#ifdef _WIN32
    profiles.push_back({"powershell", "powershell.exe", {}, "", {}, "terminal"});
    profiles.push_back({"cmd", "cmd.exe", {}, "", {}, "terminal"});
#else
    profiles.push_back({"bash", "/bin/bash", {}, "", {}, "terminal"});
#endif
}

std::shared_ptr<TerminalInstance> TerminalManager::createTerminal(const std::string& profileName, const std::string& cwd)
{
    auto term = std::make_shared<TerminalInstance>();
    term->id = nextId++;
    term->cwd = cwd.empty() ? "." : cwd;
    if (!profileName.empty())
    {
        for (const auto& p : profiles)
        {
            if (p.name == profileName)
            {
                term->profile = p;
                break;
            }
        }
    }
    if (term->profile.name.empty() && !profiles.empty())
        term->profile = profiles.front();
    term->name = term->profile.name.empty() ? ("terminal-" + std::to_string(term->id)) : term->profile.name;
    term->isRunning = true;
    terminals.push_back(term);
    return term;
}

std::shared_ptr<TerminalInstance> TerminalManager::getTerminal(int id)
{
    for (auto& t : terminals)
    {
        if (t->id == id)
            return t;
    }
    return nullptr;
}

void SettingsManager::setDefaultPath(const std::string& path) { settingsPath = path; }
void SettingsManager::addRecentFile(const std::string& path)
{
    recentFiles.erase(std::remove(recentFiles.begin(), recentFiles.end(), path), recentFiles.end());
    recentFiles.insert(recentFiles.begin(), path);
    if (recentFiles.size() > 50)
        recentFiles.resize(50);
}

bool SettingsManager::save()
{
    if (settingsPath.empty())
        return false;
    std::ofstream out(settingsPath);
    if (!out)
        return false;
    out << "{\n  \"recentFiles\": [\n";
    for (size_t i = 0; i < recentFiles.size(); ++i)
        out << "    \"" << recentFiles[i] << "\"" << (i + 1 < recentFiles.size() ? "," : "") << "\n";
    out << "  ]\n}\n";
    return true;
}

bool SettingsManager::load() { return !settingsPath.empty(); }

void IconTheme::buildMaps()
{
    extensionMap.clear();
    nameMap.clear();
    languageMap.clear();
    for (const auto& icon : icons)
    {
        for (const auto& e : icon.fileExtensions)
            extensionMap[e] = icon.name;
        for (const auto& n : icon.fileNames)
            nameMap[n] = icon.name;
        if (!icon.languageId.empty())
            languageMap[icon.languageId] = icon.name;
    }
}

void IconThemeManager::loadTheme(const std::string& id, const std::string& name, const std::vector<IconThemeIcon>& icons)
{
    IconTheme t;
    t.id = id;
    t.name = name;
    t.icons = icons;
    t.defaultIcon = "file";
    t.folderIcon = "folder";
    t.buildMaps();
    themes[id] = std::move(t);
}

void IconThemeManager::setActiveTheme(const std::string& id)
{
    auto it = themes.find(id);
    activeTheme = (it == themes.end()) ? nullptr : &it->second;
}

void IconThemeManager::loadDefaultThemes()
{
    loadTheme("default", "Default", {});
    setActiveTheme("default");
}

bool ExtensionHost::loadExtension(const std::string& path)
{
    Extension e;
    e.id = path;
    e.path = path;
    extensions[e.id] = e;
    for (const auto& cb : onLoadCallbacks)
        cb(e);
    return true;
}

void IDEFeatures::initialize()
{
    snippets.loadDefaults();
    terminals.loadProfiles();
    icons.loadDefaultThemes();
    layout.init();
}

void IDEFeatures::loadWorkspace(const std::string& path)
{
    if (path.empty())
        return;
    if (path.rfind(".code-workspace") != std::string::npos)
        workspace.load(path);
    else
        workspace.addFolder(path);
}

void IDEFeatures::indexWorkspace() { symbolsIndex.clear(); }

void IDEFeatures::openFile(const std::string& path)
{
    if (path.empty())
        return;
    settings.addRecentFile(path);
    breadcrumb.updateFromPath(path);
}

std::string IDEFeatures::detectLanguage(const std::string& path)
{
    const auto dot = path.find_last_of('.');
    if (dot == std::string::npos)
        return "plaintext";
    const std::string ext = path.substr(dot);
    if (ext == ".cpp" || ext == ".cc" || ext == ".cxx" || ext == ".hpp" || ext == ".h")
        return "cpp";
    if (ext == ".py")
        return "python";
    if (ext == ".js")
        return "javascript";
    if (ext == ".ts" || ext == ".tsx")
        return "typescript";
    if (ext == ".rs")
        return "rust";
    if (ext == ".go")
        return "go";
    return "plaintext";
}

void IDEFeatures::saveSettings() { settings.save(); }
void IDEFeatures::loadSettings() { settings.load(); }

} // namespace rawrxd
