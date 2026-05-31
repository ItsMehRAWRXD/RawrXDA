#pragma once

// Heuristic symbol index, UTF-8 LSP-shaped JSON helpers, AI context. Win32 + STL.
// Full semantic symbols: use LSP (Win32IDE_LSPClient); this backs offline / fallback indexing.

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace RawrXD::SymbolEngine
{

enum class SymbolKind : int
{
    File = 1,
    Module = 2,
    Namespace = 3,
    Package = 4,
    Class = 5,
    Method = 6,
    Property = 7,
    Field = 8,
    Constructor = 9,
    Enum = 10,
    Interface = 11,
    Function = 12,
    Variable = 13,
    Constant = 14,
    String = 15,
    Number = 16,
    Boolean = 17,
    Array = 18,
    Object = 19,
    Key = 20,
    Null = 21,
    EnumMember = 22,
    Struct = 23,
    Event = 24,
    Operator = 25,
    TypeParameter = 26
};

struct Range
{
    UINT lineStart = 0, colStart = 0;
    UINT lineEnd = 0, colEnd = 0;

    bool Contains(UINT line, UINT col) const noexcept
    {
        if (line < lineStart || line > lineEnd)
            return false;
        if (line == lineStart && col < colStart)
            return false;
        if (line == lineEnd && col > colEnd)
            return false;
        return true;
    }
};

struct Symbol;
using SymbolPtr = std::shared_ptr<Symbol>;
using SymbolWeak = std::weak_ptr<Symbol>;

struct Symbol
{
    std::wstring name;
    std::wstring detail;
    std::wstring sourceUri;
    SymbolKind kind = SymbolKind::Variable;
    Range range{};
    Range selectionRange{};
    SymbolWeak parent;
    std::vector<SymbolPtr> children;
    bool deprecated = false;
    UINT64 id = 0;

    bool IsContainer() const noexcept
    {
        return kind == SymbolKind::Class || kind == SymbolKind::Struct || kind == SymbolKind::Namespace ||
               kind == SymbolKind::Interface || kind == SymbolKind::Enum || kind == SymbolKind::Object;
    }

    std::wstring FullQualifiedName() const
    {
        std::wstring fq = name;
        auto p = parent.lock();
        while (p)
        {
            fq = p->name + L"::" + fq;
            p = p->parent.lock();
        }
        return fq;
    }

    std::wstring ToAIContext() const
    {
        std::wstring ctx = FullQualifiedName();
        if (!detail.empty())
            ctx += L" " + detail;
        return ctx;
    }
};

class SymbolDatabase
{
  public:
    SymbolDatabase() = default;
    ~SymbolDatabase() = default;

    void IndexDocument(std::wstring_view uri, SymbolPtr root);
    void RemoveDocument(std::wstring_view uri);
    SymbolPtr GetDocumentRoot(std::wstring_view uri) const;

    std::vector<SymbolPtr> SearchWorkspace(std::wstring_view query, int maxResults = 100) const;
    SymbolPtr FindById(UINT64 id) const;

    std::vector<SymbolPtr> GetSymbolsAtLine(std::wstring_view uri, UINT line) const;
    std::vector<SymbolPtr> GetContextSymbols(std::wstring_view uri, const Range& range, int maxDepth = 3) const;

    void Clear();

  private:
    using DocumentMap = std::unordered_map<std::wstring, SymbolPtr>;

    SymbolPtr getDocumentRootLocked(std::wstring_view uri) const;
    void removeDocumentLocked(std::wstring_view uri);
    void indexSymbolRecursive(const SymbolPtr& sym, const std::wstring& docUri);
    void removeFromWorkspaceIndex(const SymbolPtr& sym);

    mutable std::shared_mutex mutex_;
    DocumentMap documentRoots_;
    std::unordered_map<std::wstring, std::vector<SymbolPtr>> workspaceIndex_;
    std::unordered_map<UINT64, SymbolPtr> idIndex_;
    UINT64 nextId_ = 1;
};

class ILanguageParser
{
  public:
    virtual ~ILanguageParser() = default;
    virtual SymbolPtr ParseDocument(std::wstring_view uri, std::string_view content) = 0;
    virtual bool CanParse(std::wstring_view filename) const = 0;
};

class GenericCodeParser final : public ILanguageParser
{
  public:
    bool CanParse(std::wstring_view filename) const override;
    SymbolPtr ParseDocument(std::wstring_view uri, std::string_view content) override;

  private:
    static void Tokenize(std::string_view content, std::vector<std::pair<std::string, Range>>& tokens);
};

class SymbolIndexer
{
  public:
    explicit SymbolIndexer(SymbolDatabase& db);
    void RegisterParser(std::unique_ptr<ILanguageParser> parser);
    void IndexFile(std::wstring_view uri, std::string_view content);
    void IndexWorkspace(std::wstring_view rootPath);

  private:
    SymbolDatabase& db_;
    std::vector<std::unique_ptr<ILanguageParser>> parsers_;
    mutable std::mutex parseMutex_;
};

namespace lsp
{
std::string PathToFileUri(std::wstring_view path);
std::string EscapeJsonUtf8(std::string_view s);
std::string SymbolToJsonUtf8(const Symbol& sym, bool includeChildren, std::wstring_view locationUri);
std::string DocumentSymbolResultArrayUtf8(const std::vector<SymbolPtr>& roots);
std::string WorkspaceSymbolResultArrayUtf8(const std::vector<SymbolPtr>& symbols, std::wstring_view defaultUri);
std::wstring ParseWorkspaceSymbolQueryUtf8(std::string_view json);
}  // namespace lsp

class AISymbolContext
{
  public:
    explicit AISymbolContext(SymbolDatabase& db);
    std::wstring BuildPromptContext(std::wstring_view uri, UINT line, UINT col, int maxSymbols = 8);
    std::wstring ResolveAtReference(std::wstring_view query);
    std::vector<UINT64> ExtractSymbolRefs(std::wstring_view aiResponse);

  private:
    SymbolDatabase& db_;
};

class StickyScrollController
{
  public:
    explicit StickyScrollController(SymbolDatabase& db);

    struct StickyLine
    {
        std::wstring text;
        SymbolKind kind = SymbolKind::Variable;
        int indentLevel = 0;
        UINT originalLine = 0;
    };

    std::vector<StickyLine> ComputeStickyLines(std::wstring_view uri, UINT topVisibleLine, int maxLines = 5);

  private:
    SymbolDatabase& db_;
};

class RawrXDSymbolHost
{
  public:
    RawrXDSymbolHost();
    void IndexFile(std::wstring_view uri, std::string_view content);
    void IndexWorkspace(std::wstring_view rootPath);
    void Clear();

    std::string DocumentSymbolLspResultUtf8(std::wstring_view uri) const;
    std::string WorkspaceSymbolLspResultUtf8(std::string_view queryUtf8) const;

    AISymbolContext& AiContext() { return ai_; }
    StickyScrollController& Sticky() { return sticky_; }
    SymbolDatabase& Db() { return db_; }
    const SymbolDatabase& Db() const { return db_; }

  private:
    SymbolDatabase db_;
    SymbolIndexer indexer_;
    AISymbolContext ai_;
    StickyScrollController sticky_;
};

  // Shared process-wide symbol host used by Win32IDE feature wiring.
  RawrXDSymbolHost& GlobalSymbolHost();
  SymbolDatabase& GlobalSymbolDatabase();
  AISymbolContext& GlobalAISymbolContext();
  void IndexFileAsync(std::wstring uri, std::string content);

}  // namespace RawrXD::SymbolEngine
