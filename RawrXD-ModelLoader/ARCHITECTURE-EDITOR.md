# RawrXD Custom Code Editor Architecture

## 1. Goals
- 1000 simultaneous tabs (practical upper bound) with acceptable memory footprint.
- Syntax highlighting for ANY language via plugin API (initial built-ins: PowerShell, C++, JSON, Markdown, Python).
- Fast editing: sub-10ms keystroke latency for typical files (<1MB) and graceful degradation for very large files.
- Incremental syntax highlighting and repaint (dirty region based).
- Extensible for completions, diagnostics, refactoring (future LSP bridge).
- Robust undo/redo with coalescing.
- Session persistence and crash resilience.

## 2. Core Components Overview
| Component | Responsibility | Key Interfaces |
|-----------|----------------|----------------|
| BufferModel | Owns text storage + line index; efficient edits | `insert`, `erase`, `getLine`, `snapshot` |
| GapBuffer / Rope | Underlying storage strategies | `moveGap`, `split`, `merge` |
| TabManager | Owns collection of `EditorTab` objects | `openTab`, `closeTab`, `activateTab` |
| EditorTab | Aggregates BufferModel + SyntaxState + UndoStack | `render`, `handleInput` |
| SyntaxEngine | Coordinates lexing + token caching + invalidation | `tokenizeRange`, `invalidate(start,end)` |
| LanguagePlugin | Provides lexer + optional semantic hooks | `registerLanguage`, `lex`, `semanticPass` |
| RenderPipeline | Draw text, gutter, highlights, selections | `drawLine`, `present` |
| ThemeManager | Color + style lookup for token types | `loadTheme`, `getStyle(tokenType)` |
| UndoStack | Command objects + coalescing window | `push`, `undo`, `redo` |
| SearchIndex | Optional per-buffer term index | `findNext`, `findAll`, `replaceAll` |
| SessionStore | Persistence of layout + buffers metadata | `save`, `restore` |
| PluginLoader | DLL discovery & sandbox bridge | `loadAll`, `resolveSymbols` |
| PerfMonitor | Metrics & instrumentation | `record(event)`, `report()` |

## 3. Text Storage Strategy
### Gap Buffer (Default)
- Single contiguous array with a movable gap (fast insertion near cursor).
- Operations:
  - `moveGap(targetIndex)` only relocates when cursor jumps far.
  - `insert(chars)` writes into gap and shrinks gap size.
  - `erase(count)` expands gap.
- Amortized O(1) for local edits; O(n) worst-case when relocating gap.

### Rope (Fallback for Large Files > 10MB)
- Balanced binary tree of leaf chunks (e.g., 4KB each).
- Maintains cumulative sizes to support O(log n) random access.
- Activated automatically when file size threshold exceeded or memory fragmentation risk detected.

### Line Index
- Maintained separately as vector of offsets (recomputed incrementally on edits affecting newlines).
- Enables O(1) line fetch.

## 4. Syntax Highlighting Engine
- Tokenization is incremental:
  - File segmented into logical blocks (e.g. 512 lines or ~32KB chunks).
  - Each block caches tokens + a simple hash of its content.
  - Edit invalidates only affected block(s) plus possible cascading for multi-line constructs (e.g., multiline string).
- Multi-pass architecture:
  1. Lexical (fast regex / state machine).
  2. Optional semantic (identifiers, keywords sets, symbol roles) — performed asynchronously.
- Token Types canonical set: `Keyword`, `Identifier`, `String`, `Number`, `Comment`, `Operator`, `Punctuation`, `Whitespace`, `Preprocessor`, `Annotation`, `Error`.
- Plugins can extend with custom types; theme fallback logic maps unknown types to `Identifier`.

## 5. Language Plugin API (C style for DLL stability)
```c
// rawrxd_language_plugin.h
#ifdef __cplusplus
extern "C" {
#endif

typedef struct RawrXDToken {
    unsigned startOffset;
    unsigned length;
    unsigned typeId; // maps to theme style
} RawrXDToken;

// Lex a buffer slice; return number of tokens written.
typedef unsigned (*RawrXDLexFn)(const char* text, unsigned length, RawrXDToken* outTokens, unsigned maxTokens);

// Optional semantic enrichment after lexical tokens
typedef void (*RawrXDSemanticFn)(RawrXDToken* tokens, unsigned count);

// Registration entry exported by plugin DLL
__declspec(dllexport) int rawrxd_register_language(
    const char* languageName,
    RawrXDLexFn lexFn,
    RawrXDSemanticFn semanticFn);

#ifdef __cplusplus
}
#endif
```
- Host maintains a registry: `std::unordered_map<std::string, LanguageDescriptor>`.
- Fallback generic lexer for unknown languages (ASCII word + punctuation splitting).

## 6. Rendering Pipeline
- Technology: DirectWrite + Direct2D (preferred for high DPI & font metrics). Fallback: GDI for minimal build environments.
- Double-buffered offscreen surface per visible editor view.
- Render Steps:
  1. Determine visible line range from scroll position.
  2. Fetch tokens for those lines; apply theme styles.
  3. Draw background (selection, search highlights, diagnostics underlines).
  4. Draw text runs batched by style to minimize state changes.
  5. Draw gutter (line numbers, breakpoint markers).
- Invalidation rectangles queued on edit; scroll triggers full viewport redraw.

## 7. Undo/Redo System
- Command pattern: each edit packaged as `EditCommand { offset, removedText, insertedText }`.
- Coalescing: merges sequential single-character inserts within TIME_WINDOW (e.g., 500ms) and same cursor region.
- Bounded memory: cap total undo history size; oldest commands discarded with a snapshot fallback.

## 8. Multi-Tab Support (1000 Tabs)
- `TabManager` keeps lightweight metadata; inactive tabs can release token caches while retaining raw buffer.
- Memory Strategy:
  - Active tabs: full token + render caches.
  - Inactive >N (e.g., >40): purge semantic tokens, keep lexical minimal.
  - LRU rehydration when switching.
- Session persistence stores last 20 dirty tabs fully; others store file path + cursor position only.

## 9. Search & Replace
- Per-buffer on-demand index for contiguous plain-text searches (Boyer–Moore Horspool).
- Regex support via RE2 or custom simplified engine (deferred integration).
- Highlights overlay drawn in rendering pipeline (list of match ranges intersecting viewport).

## 10. Session Persistence
- JSON file `session.json`:
```json
{
  "version":1,
  "tabs":[{"file":"C:/code/main.cpp","cursor":1024,"selection":{"start":1024,"end":1030},"undoDepth":42}],
  "activeTab":0,
  "theme":"default-dark"
}
```
- Periodic autosave (e.g., every 30s or on idle).

## 11. Plugin Loader
- Scans `plugins/` directory at launch.
- Loads DLL, locates `rawrxd_register_language`; invokes to populate registry.
- Safety: version negotiation integer return; reject if mismatch; no exceptions crossing boundary.

## 12. Performance & Instrumentation
- Metrics: keystroke latency, repaint time, tokenization duration, memory per tab.
- Floating panel displays aggregates (already have panel; add section for metrics).
- Stress test harness opens synthetic files to validate scaling (target < 1.5GB RAM at 1000 modest tabs ~10KB each).

## 13. Theming
- Theme file: token type -> RGBA + font style.
```json
{
  "keyword":"#C586C0",
  "string":"#CE9178",
  "comment":"#6A9955",
  "identifier":"#D4D4D4"
}
```
- Supports dynamic reload; triggers re-render of all visible views.

## 14. Future LSP Bridge (Deferred)
- Thin adapter mapping LanguagePlugin lexical tokens to semantic info from LSP (diagnostics, completions, hovers).
- Non-blocking: run in background thread; apply diagnostics overlay when available.

## 15. Error Handling & Resilience
- All plugin calls wrapped in structured exception handling (`__try / __except` on Windows) if necessary.
- Corruption fallback: if buffer invariants break, serialize current text, rebuild BufferModel fresh, restore state.

## 16. Implementation Phases
1. BufferModel + line index (gap buffer).
2. Basic rendering (monochrome) using DirectWrite.
3. Lexer framework + generic lexer + theme coloring.
4. Tabs and memory management heuristics.
5. Undo/redo + search/replace.
6. Plugin loader + initial language lexers.
7. Rope integration for large files.
8. Session persistence + metrics.
9. Stress test & optimization round.
10. Optional LSP bridge.

## 17. Risks & Mitigations
| Risk | Impact | Mitigation |
|------|--------|-----------|
| 1000 tabs memory blow-up | High | Lazy token cache purge, rope for large buffers |
| Complex multi-language lexers | Medium | Start with simplified token sets; add semantic passes later |
| Rendering flicker/perf | Medium | Double buffering + dirty rectangles |
| Plugin crash | Medium | Isolation via version checks and error guards |
| Large file edits slow | Medium | Rope + incremental token invalidation |

## 18. Minimal Initial Interfaces (C++ Skeleton)
```cpp
class BufferModel {
public:
    size_t size() const; 
    void insert(size_t pos, std::string_view text);
    void erase(size_t pos, size_t len);
    std::string getText(size_t pos, size_t len) const;
    std::string getLine(size_t line) const;
private:
    std::vector<char> m_data; // gap buffer representation
    size_t m_gapStart{};
    size_t m_gapEnd{};
    std::vector<size_t> m_lineOffsets; // incremental maintenance
    void moveGap(size_t pos);
    void rebuildLineIndex(size_t from, size_t to);
};

struct Token { unsigned start; unsigned length; unsigned type; };

class LanguagePlugin {
public:
    virtual std::string name() const = 0;
    virtual void lex(std::string_view text, std::vector<Token>& outTokens) = 0;
    virtual void semantic(std::vector<Token>& tokens) { /* optional */ }
    virtual ~LanguagePlugin() = default;
};

class SyntaxEngine {
public:
    void setLanguage(LanguagePlugin* lang);
    void tokenize(BufferModel& buffer, size_t startLine, size_t lineCount);
private:
    LanguagePlugin* m_lang = nullptr;
};
```

## 19. Immediate Next Step
Proceed to implement `BufferModel` (gap buffer) and skeletal lexer registry.

---
This document serves as the blueprint; update as subsystems evolve.
