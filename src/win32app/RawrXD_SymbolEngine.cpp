#include "RawrXD_SymbolEngine.h"

#include <cstdio>
#include <cwctype>
#include <functional>
#include <sstream>
#include <thread>

namespace RawrXD::SymbolEngine
{

namespace
{

std::string WideToUtf8(std::wstring_view w)
{
    if (w.empty())
        return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    if (n <= 0)
        return {};
    std::string out(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), out.data(), n, nullptr, nullptr);
    return out;
}

void AssignSelectionRange(SymbolPtr sym)
{
    if (!sym)
        return;
    sym->selectionRange = sym->range;
}

}  // namespace

SymbolPtr SymbolDatabase::getDocumentRootLocked(std::wstring_view uri) const
{
    std::wstring key(uri);
    auto it = documentRoots_.find(key);
    return (it != documentRoots_.end()) ? it->second : nullptr;
}

void SymbolDatabase::removeDocumentLocked(std::wstring_view uri)
{
    std::wstring key(uri);
    auto it = documentRoots_.find(key);
    if (it == documentRoots_.end())
        return;
    removeFromWorkspaceIndex(it->second);
    documentRoots_.erase(it);
}

void SymbolDatabase::IndexDocument(std::wstring_view uri, SymbolPtr root)
{
    if (!root)
        return;
    std::wstring doc(uri);
    root->sourceUri = doc;
    std::unique_lock lock(mutex_);
    removeDocumentLocked(uri);
    documentRoots_[doc] = root;
    indexSymbolRecursive(root, doc);
}

void SymbolDatabase::RemoveDocument(std::wstring_view uri)
{
    std::unique_lock lock(mutex_);
    removeDocumentLocked(uri);
}

SymbolPtr SymbolDatabase::GetDocumentRoot(std::wstring_view uri) const
{
    std::shared_lock lock(mutex_);
    return getDocumentRootLocked(uri);
}

void SymbolDatabase::indexSymbolRecursive(const SymbolPtr& sym, const std::wstring& docUri)
{
    sym->id = nextId_++;
    sym->sourceUri = docUri;
    idIndex_[sym->id] = sym;
    workspaceIndex_[sym->name].push_back(sym);
    for (auto& child : sym->children)
    {
        child->parent = sym;
        indexSymbolRecursive(child, docUri);
    }
}

void SymbolDatabase::removeFromWorkspaceIndex(const SymbolPtr& sym)
{
    auto it = workspaceIndex_.find(sym->name);
    if (it != workspaceIndex_.end())
    {
        auto& vec = it->second;
        vec.erase(std::remove_if(vec.begin(), vec.end(), [&](const SymbolPtr& p) { return p && p->id == sym->id; }),
                  vec.end());
        if (vec.empty())
            workspaceIndex_.erase(it);
    }
    idIndex_.erase(sym->id);
    for (auto& child : sym->children)
        removeFromWorkspaceIndex(child);
}

std::vector<SymbolPtr> SymbolDatabase::SearchWorkspace(std::wstring_view query, int maxResults) const
{
    std::shared_lock lock(mutex_);
    std::vector<SymbolPtr> results;
    if (maxResults <= 0)
        return results;

    std::wstring q(query);
    std::transform(q.begin(), q.end(), q.begin(), ::towlower);

    for (const auto& kv : workspaceIndex_)
    {
        if (static_cast<int>(results.size()) >= maxResults)
            break;
        std::wstring lowerName = kv.first;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::towlower);
        if (q.empty() || lowerName.find(q) != std::wstring::npos)
        {
            for (const auto& s : kv.second)
            {
                if (static_cast<int>(results.size()) >= maxResults)
                    break;
                results.push_back(s);
            }
        }
    }
    return results;
}

SymbolPtr SymbolDatabase::FindById(UINT64 id) const
{
    std::shared_lock lock(mutex_);
    auto it = idIndex_.find(id);
    return (it != idIndex_.end()) ? it->second : nullptr;
}

std::vector<SymbolPtr> SymbolDatabase::GetSymbolsAtLine(std::wstring_view uri, UINT line) const
{
    std::shared_lock lock(mutex_);
    std::vector<SymbolPtr> result;
    auto root = getDocumentRootLocked(uri);
    if (!root)
        return result;

    std::function<void(const SymbolPtr&)> check = [&](const SymbolPtr& s)
    {
        if (!s)
            return;
        if (line >= s->range.lineStart && line <= s->range.lineEnd)
            result.push_back(s);
        for (auto& c : s->children)
            check(c);
    };
    check(root);
    return result;
}

void SymbolDatabase::Clear()
{
    std::unique_lock lock(mutex_);
    documentRoots_.clear();
    workspaceIndex_.clear();
    idIndex_.clear();
    nextId_ = 1;
}

std::vector<SymbolPtr> SymbolDatabase::GetContextSymbols(std::wstring_view uri, const Range& range, int maxDepth) const
{
    std::shared_lock lock(mutex_);
    std::vector<SymbolPtr> ctx;
    auto root = getDocumentRootLocked(uri);
    if (!root || maxDepth <= 0)
        return ctx;

    SymbolPtr current = root;
    bool progressed = true;
    while (progressed)
    {
        progressed = false;
        for (auto& child : current->children)
        {
            if (!child)
                continue;
            if (range.lineStart >= child->range.lineStart && range.lineEnd <= child->range.lineEnd)
            {
                current = child;
                progressed = true;
                break;
            }
        }
    }

    int depth = 0;
    while (current && depth < maxDepth)
    {
        ctx.push_back(current);
        current = current->parent.lock();
        depth++;
    }
    std::reverse(ctx.begin(), ctx.end());
    return ctx;
}

bool GenericCodeParser::CanParse(std::wstring_view filename) const
{
    const auto dot = filename.find_last_of(L'.');
    if (dot == std::wstring_view::npos || dot + 1 >= filename.size())
        return false;
    std::wstring_view ext = filename.substr(dot + 1);
    return ext == L"cpp" || ext == L"c" || ext == L"h" || ext == L"hpp" || ext == L"rs" || ext == L"go" ||
           ext == L"js" || ext == L"ts" || ext == L"java" || ext == L"cs" || ext == L"py" || ext == L"m" ||
           ext == L"asm";
}

void GenericCodeParser::Tokenize(std::string_view content, std::vector<std::pair<std::string, Range>>& tokens)
{
    tokens.clear();
    size_t line = 0;
    size_t col = 0;
    size_t i = 0;
    const size_t n = content.size();

    auto advanceLine = [&]()
    {
        ++line;
        col = 0;
    };

    while (i < n)
    {
        char c = content[i];

        if (c == '\r')
        {
            ++i;
            if (i < n && content[i] == '\n')
                ++i;
            advanceLine();
            continue;
        }
        if (c == '\n')
        {
            ++i;
            advanceLine();
            continue;
        }

        if (i + 1 < n && c == '/' && content[i + 1] == '/')
        {
            i += 2;
            col += 2;
            while (i < n && content[i] != '\n' && content[i] != '\r')
            {
                ++i;
                ++col;
            }
            continue;
        }

        bool inStr = (c == '"' || c == '\'');
        if (inStr)
        {
            char q = c;
            ++i;
            ++col;
            while (i < n)
            {
                if (content[i] == '\\' && i + 1 < n)
                {
                    i += 2;
                    col += 2;
                    continue;
                }
                if (content[i] == '\r' || content[i] == '\n')
                    break;
                if (content[i] == q)
                {
                    ++i;
                    ++col;
                    break;
                }
                ++i;
                ++col;
            }
            continue;
        }

        bool isIdentStart = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || (c >= '0' && c <= '9');
        if (isIdentStart && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'))
        {
            size_t start = i;
            size_t startCol = col;
            ++i;
            ++col;
            while (i < n)
            {
                char d = content[i];
                if ((d >= 'a' && d <= 'z') || (d >= 'A' && d <= 'Z') || (d >= '0' && d <= '9') || d == '_')
                {
                    ++i;
                    ++col;
                }
                else
                    break;
            }
            Range r{};
            r.lineStart = static_cast<UINT>(line);
            r.colStart = static_cast<UINT>(startCol);
            r.lineEnd = static_cast<UINT>(line);
            r.colEnd = static_cast<UINT>(col);
            tokens.emplace_back(std::string(content.substr(start, i - start)), r);
            continue;
        }

        if (c == '{' || c == '}' || c == '(' || c == ')' || c == ';')
        {
            Range r{};
            r.lineStart = r.lineEnd = static_cast<UINT>(line);
            r.colStart = static_cast<UINT>(col);
            r.colEnd = static_cast<UINT>(col + 1);
            tokens.emplace_back(std::string(1, c), r);
            ++i;
            ++col;
            continue;
        }

        ++i;
        ++col;
    }
}

SymbolPtr GenericCodeParser::ParseDocument(std::wstring_view uri, std::string_view content)
{
    auto root = std::make_shared<Symbol>();
    const auto slash = uri.find_last_of(L"/\\");
    root->name = (slash == std::wstring_view::npos) ? std::wstring(uri) : std::wstring(uri.substr(slash + 1));
    root->kind = SymbolKind::File;
    root->sourceUri = std::wstring(uri);
    root->range = Range{0, 0, 0, 0};

    std::vector<std::pair<std::string, Range>> tokens;
    Tokenize(content, tokens);

    SymbolPtr currentContainer = root;
    int braceDepth = 0;

    for (size_t i = 0; i < tokens.size(); ++i)
    {
        auto& tok = tokens[i].first;
        auto& range = tokens[i].second;
        std::string_view nextTok = (i + 1 < tokens.size()) ? std::string_view(tokens[i + 1].first) : "";

        if (tok == "class" || tok == "struct" || tok == "namespace")
        {
            if (!nextTok.empty() && nextTok != "{" && nextTok != ";")
            {
                auto sym = std::make_shared<Symbol>();
                sym->name.assign(nextTok.begin(), nextTok.end());
                sym->kind = (tok == "class")    ? SymbolKind::Class
                            : (tok == "struct") ? SymbolKind::Struct
                                                : SymbolKind::Namespace;
                sym->range = range;
                sym->parent = currentContainer;
                AssignSelectionRange(sym);
                currentContainer->children.push_back(sym);
                currentContainer = sym;
                braceDepth = 0;
            }
        }
        else if (tok == "{")
        {
            braceDepth++;
        }
        else if (tok == "}")
        {
            braceDepth--;
            if (braceDepth <= 0 && currentContainer != root && currentContainer->parent.lock())
            {
                auto par = currentContainer->parent.lock();
                currentContainer->range.lineEnd = range.lineEnd;
                currentContainer->range.colEnd = range.colEnd;
                currentContainer = par ? par : root;
                braceDepth = 0;
            }
        }
        else if (tok == "def" || tok == "fn" || tok == "func" || tok == "function")
        {
            if (!nextTok.empty())
            {
                auto sym = std::make_shared<Symbol>();
                sym->name.assign(nextTok.begin(), nextTok.end());
                sym->kind = SymbolKind::Function;
                sym->range = range;
                sym->parent = currentContainer;
                AssignSelectionRange(sym);
                currentContainer->children.push_back(sym);
            }
        }
        else if ((tok == "int" || tok == "void" || tok == "bool" || tok == "string" || tok == "auto" || tok == "let" ||
                  tok == "var" || tok == "const") &&
                 !nextTok.empty() && nextTok != "(" && nextTok != "=")
        {
            auto sym = std::make_shared<Symbol>();
            sym->name.assign(nextTok.begin(), nextTok.end());
            sym->kind = SymbolKind::Variable;
            sym->range = range;
            sym->parent = currentContainer;
            AssignSelectionRange(sym);
            currentContainer->children.push_back(sym);
        }
    }

    return root;
}

SymbolIndexer::SymbolIndexer(SymbolDatabase& db) : db_(db)
{
    parsers_.push_back(std::make_unique<GenericCodeParser>());
}

void SymbolIndexer::RegisterParser(std::unique_ptr<ILanguageParser> parser)
{
    if (parser)
        parsers_.push_back(std::move(parser));
}

void SymbolIndexer::IndexFile(std::wstring_view uri, std::string_view content)
{
    std::lock_guard<std::mutex> lock(parseMutex_);
    for (auto& parser : parsers_)
    {
        if (parser->CanParse(uri))
        {
            SymbolPtr root = parser->ParseDocument(uri, content);
            if (root)
                db_.IndexDocument(uri, root);
            return;
        }
    }
}

void SymbolIndexer::IndexWorkspace(std::wstring_view rootPath)
{
    std::wstring search(rootPath);
    if (search.empty())
        return;
    if (search.back() != L'\\' && search.back() != L'/')
        search += L'\\';
    search += L"*.*";

    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW(search.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE)
        return;

    do
    {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (wcscmp(fd.cFileName, L".") != 0 && wcscmp(fd.cFileName, L"..") != 0)
            {
                std::wstring sub = std::wstring(rootPath);
                if (!sub.empty() && sub.back() != L'\\' && sub.back() != L'/')
                    sub += L'\\';
                sub += fd.cFileName;
                IndexWorkspace(sub);
            }
        }
        else
        {
            std::wstring path = std::wstring(rootPath);
            if (!path.empty() && path.back() != L'\\' && path.back() != L'/')
                path += L'\\';
            path += fd.cFileName;

            HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
            if (hFile == INVALID_HANDLE_VALUE)
                continue;
            DWORD size = GetFileSize(hFile, nullptr);
            if (size > 0 && size < 10u * 1024u * 1024u)
            {
                std::vector<char> buf(size);
                DWORD read = 0;
                if (ReadFile(hFile, buf.data(), size, &read, nullptr) && read > 0)
                    IndexFile(path, std::string_view(buf.data(), read));
            }
            CloseHandle(hFile);
        }
    } while (FindNextFileW(h, &fd));

    FindClose(h);
}

namespace lsp
{

std::string PathToFileUri(std::wstring_view path)
{
    if (path.empty())
        return "file:///";
    std::string uri = "file:///";
    std::string p = WideToUtf8(path);
    for (char& c : p)
    {
        if (c == '\\')
            c = '/';
    }
    if (p.size() >= 2 && p[1] == ':')
    {
        uri += p;
    }
    else
    {
        for (char c : p)
        {
            if (c == ' ')
                uri += "%20";
            else
                uri += c;
        }
    }
    return uri;
}

std::string EscapeJsonUtf8(std::string_view s)
{
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s)
    {
        switch (c)
        {
            case '\\':
                out += "\\\\";
                break;
            case '"':
                out += "\\\"";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if (c < 0x20)
                {
                    char buf[7];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                }
                else
                    out += static_cast<char>(c);
        }
    }
    return out;
}

std::string SymbolToJsonUtf8(const Symbol& sym, bool includeChildren, std::wstring_view locationUri)
{
    std::ostringstream js;
    std::wstring uriForLoc = sym.sourceUri.empty() ? std::wstring(locationUri) : sym.sourceUri;
    std::string nameU = WideToUtf8(sym.name);
    std::string detailU = WideToUtf8(sym.detail);

    js << '{';
    js << "\"name\":\"" << EscapeJsonUtf8(nameU) << "\",";
    js << "\"kind\":" << static_cast<int>(sym.kind) << ",";
    if (sym.deprecated)
        js << "\"deprecated\":true,";
    js << "\"range\":{\"start\":{\"line\":" << sym.range.lineStart << ",\"character\":" << sym.range.colStart
       << "},\"end\":{\"line\":" << sym.range.lineEnd << ",\"character\":" << sym.range.colEnd << "}},";
    js << "\"selectionRange\":{\"start\":{\"line\":" << sym.selectionRange.lineStart
       << ",\"character\":" << sym.selectionRange.colStart << "},\"end\":{\"line\":" << sym.selectionRange.lineEnd
       << ",\"character\":" << sym.selectionRange.colEnd << "}}";
    if (!detailU.empty())
        js << ",\"detail\":\"" << EscapeJsonUtf8(detailU) << "\"";
    if (includeChildren && !sym.children.empty())
    {
        js << ",\"children\":[";
        for (size_t i = 0; i < sym.children.size(); ++i)
        {
            if (i > 0)
                js << ',';
            if (sym.children[i])
                js << SymbolToJsonUtf8(*sym.children[i], true, uriForLoc);
        }
        js << ']';
    }
    js << '}';
    return js.str();
}

std::string DocumentSymbolResultArrayUtf8(const std::vector<SymbolPtr>& roots)
{
    std::ostringstream js;
    js << '[';
    for (size_t i = 0; i < roots.size(); ++i)
    {
        if (i > 0)
            js << ',';
        if (roots[i])
            js << SymbolToJsonUtf8(*roots[i], true, roots[i]->sourceUri);
    }
    js << ']';
    return js.str();
}

std::string WorkspaceSymbolResultArrayUtf8(const std::vector<SymbolPtr>& symbols, std::wstring_view defaultUri)
{
    std::ostringstream js;
    js << '[';
    bool first = true;
    for (const auto& symPtr : symbols)
    {
        const Symbol* s = symPtr.get();
        if (!s)
            continue;
        if (!first)
            js << ',';
        first = false;
        std::wstring locUri = s->sourceUri.empty() ? std::wstring(defaultUri) : s->sourceUri;
        std::string uri = PathToFileUri(locUri);
        std::string nameU = WideToUtf8(s->name);
        js << '{';
        js << "\"name\":\"" << EscapeJsonUtf8(nameU) << "\",";
        js << "\"kind\":" << static_cast<int>(s->kind) << ",";
        if (!s->detail.empty())
            js << "\"detail\":\"" << EscapeJsonUtf8(WideToUtf8(s->detail)) << "\",";
        js << "\"location\":{";
        js << "\"uri\":\"" << EscapeJsonUtf8(uri) << "\",";
        js << "\"range\":{\"start\":{\"line\":" << s->range.lineStart << ",\"character\":" << s->range.colStart
           << "},\"end\":{\"line\":" << s->range.lineEnd << ",\"character\":" << s->range.colEnd << "}}";
        js << "}}";
    }
    js << ']';
    return js.str();
}

std::wstring ParseWorkspaceSymbolQueryUtf8(std::string_view json)
{
    auto pos = json.find("\"query\"");
    if (pos == std::string_view::npos)
        return L"";
    pos = json.find(':', pos);
    if (pos == std::string_view::npos)
        return L"";
    pos = json.find('"', pos);
    if (pos == std::string_view::npos)
        return L"";
    ++pos;
    auto end = json.find('"', pos);
    if (end == std::string_view::npos)
        return L"";
    std::string sub(json.substr(pos, end - pos));
    int n = MultiByteToWideChar(CP_UTF8, 0, sub.data(), static_cast<int>(sub.size()), nullptr, 0);
    if (n <= 0)
        return std::wstring(sub.begin(), sub.end());
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, sub.data(), static_cast<int>(sub.size()), w.data(), n);
    return w;
}

}  // namespace lsp

AISymbolContext::AISymbolContext(SymbolDatabase& db) : db_(db) {}

std::wstring AISymbolContext::BuildPromptContext(std::wstring_view uri, UINT line, UINT col, int maxSymbols)
{
    Range r{line, col, line, col};
    auto symbols = db_.GetContextSymbols(uri, r, maxSymbols);
    std::wstring ctx = L"Symbols:\n";
    for (auto& s : symbols)
    {
        if (s)
            ctx += L"- " + s->ToAIContext() + L"\n";
    }
    return ctx;
}

std::wstring AISymbolContext::ResolveAtReference(std::wstring_view query)
{
    auto results = db_.SearchWorkspace(query, 1);
    if (results.empty() || !results[0])
        return L"Symbol not found: " + std::wstring(query);
    return results[0]->ToAIContext();
}

std::vector<UINT64> AISymbolContext::ExtractSymbolRefs(std::wstring_view aiResponse)
{
    std::vector<UINT64> refs;
    size_t pos = 0;
    while (pos < aiResponse.size())
    {
        pos = aiResponse.find(L'@', pos);
        if (pos == std::wstring_view::npos)
            break;
        ++pos;
        size_t end = pos;
        while (end < aiResponse.size() &&
               (iswalnum(static_cast<wint_t>(aiResponse[end])) || aiResponse[end] == L'_' || aiResponse[end] == L':'))
        {
            ++end;
        }
        if (end > pos)
        {
            std::wstring name(aiResponse.substr(pos, end - pos));
            auto results = db_.SearchWorkspace(name, 1);
            if (!results.empty() && results[0])
                refs.push_back(results[0]->id);
        }
        pos = end;
    }
    return refs;
}

StickyScrollController::StickyScrollController(SymbolDatabase& db) : db_(db) {}

std::vector<StickyScrollController::StickyLine> StickyScrollController::ComputeStickyLines(std::wstring_view uri,
                                                                                           UINT topVisibleLine,
                                                                                           int maxLines)
{
    std::vector<StickyLine> lines;
    auto syms = db_.GetSymbolsAtLine(uri, topVisibleLine);

    for (auto& s : syms)
    {
        if (!s)
            continue;
        if (s->range.lineStart < topVisibleLine && s->IsContainer())
        {
            StickyLine sl;
            sl.text = s->name;
            sl.kind = s->kind;
            sl.originalLine = s->range.lineStart;
            int depth = 0;
            auto p = s->parent.lock();
            while (p)
            {
                ++depth;
                p = p->parent.lock();
            }
            sl.indentLevel = depth;
            lines.push_back(sl);
        }
    }

    std::sort(lines.begin(), lines.end(),
              [](const StickyLine& a, const StickyLine& b) { return a.originalLine < b.originalLine; });
    if (static_cast<int>(lines.size()) > maxLines)
        lines.resize(static_cast<size_t>(maxLines));
    return lines;
}

RawrXDSymbolHost::RawrXDSymbolHost() : indexer_(db_), ai_(db_), sticky_(db_) {}

void RawrXDSymbolHost::IndexFile(std::wstring_view uri, std::string_view content)
{
    indexer_.IndexFile(uri, content);
}

void RawrXDSymbolHost::IndexWorkspace(std::wstring_view rootPath)
{
    indexer_.IndexWorkspace(rootPath);
}

void RawrXDSymbolHost::Clear()
{
    db_.Clear();
}

std::string RawrXDSymbolHost::DocumentSymbolLspResultUtf8(std::wstring_view uri) const
{
    auto root = db_.GetDocumentRoot(uri);
    std::vector<SymbolPtr> v;
    if (root)
        v.push_back(root);
    return lsp::DocumentSymbolResultArrayUtf8(v);
}

std::string RawrXDSymbolHost::WorkspaceSymbolLspResultUtf8(std::string_view queryUtf8) const
{
    std::wstring q;
    int n = MultiByteToWideChar(CP_UTF8, 0, queryUtf8.data(), static_cast<int>(queryUtf8.size()), nullptr, 0);
    if (n > 0)
    {
        q.resize(static_cast<size_t>(n));
        MultiByteToWideChar(CP_UTF8, 0, queryUtf8.data(), static_cast<int>(queryUtf8.size()), q.data(), n);
    }
    auto syms = db_.SearchWorkspace(q, 100);
    return lsp::WorkspaceSymbolResultArrayUtf8(syms, std::wstring_view{});
}

RawrXDSymbolHost& GlobalSymbolHost()
{
    static RawrXDSymbolHost s_host;
    return s_host;
}

SymbolDatabase& GlobalSymbolDatabase()
{
    return GlobalSymbolHost().Db();
}

AISymbolContext& GlobalAISymbolContext()
{
    return GlobalSymbolHost().AiContext();
}

void IndexFileAsync(std::wstring uri, std::string content)
{
    std::thread([uri = std::move(uri), content = std::move(content)]() { GlobalSymbolHost().IndexFile(uri, content); })
        .detach();
}

}  // namespace RawrXD::SymbolEngine
