/**
 * @file SemanticHighlighter.cpp
 * @brief Complete LSP Semantic Highlighting Implementation for RawrXD Agentic IDE
 * 
 * Provides advanced semantic highlighting based on token types and scopes,
 * going beyond simple syntax highlighting to understand code semantics.
 * 
 * @author RawrXD Team
 * @copyright 2024 RawrXD
 */

#include "SemanticHighlighter.h"
#include <QRegularExpression>
#include <QRegularExpressionMatchIterator>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QMutexLocker>
#include <QThreadPool>
#include <QtConcurrent>
#include <QFuture>
#include <QFutureWatcher>
#include <algorithm>
#include <functional>

namespace RawrXD {

// ============================================================================
// SemanticTokenCache Implementation
// ============================================================================

SemanticTokenCache::SemanticTokenCache(int maxSize)
    : m_maxSize(maxSize)
{
}

void SemanticTokenCache::insert(const QString& key, const QList<SemanticToken>& tokens, const QString& contentHash)
{
    QMutexLocker locker(&m_mutex);
    
    CacheEntry entry;
    entry.tokens = tokens;
    entry.contentHash = contentHash;
    entry.timestamp = QDateTime::currentMSecsSinceEpoch();
    
    m_cache[key] = entry;
    
    // Evict old entries if cache is too large
    if (m_cache.size() > m_maxSize) {
        evictOldest();
    }
}

bool SemanticTokenCache::lookup(const QString& key, const QString& contentHash, QList<SemanticToken>& outTokens) const
{
    QMutexLocker locker(&m_mutex);
    
    auto it = m_cache.find(key);
    if (it == m_cache.end()) {
        return false;
    }
    
    // Check if content hash matches (invalidate if content changed)
    if (it->contentHash != contentHash) {
        return false;
    }
    
    outTokens = it->tokens;
    return true;
}

void SemanticTokenCache::invalidate(const QString& key)
{
    QMutexLocker locker(&m_mutex);
    m_cache.remove(key);
}

void SemanticTokenCache::clear()
{
    QMutexLocker locker(&m_mutex);
    m_cache.clear();
}

int SemanticTokenCache::size() const
{
    QMutexLocker locker(&m_mutex);
    return m_cache.size();
}

void SemanticTokenCache::evictOldest()
{
    if (m_cache.isEmpty()) return;
    
    QString oldestKey;
    qint64 oldestTime = std::numeric_limits<qint64>::max();
    
    for (auto it = m_cache.begin(); it != m_cache.end(); ++it) {
        if (it->timestamp < oldestTime) {
            oldestTime = it->timestamp;
            oldestKey = it.key();
        }
    }
    
    if (!oldestKey.isEmpty()) {
        m_cache.remove(oldestKey);
    }
}

// ============================================================================
// SemanticHighlighter Implementation
// ============================================================================

SemanticHighlighter::SemanticHighlighter(QObject* parent)
    : QObject(parent)
    , m_cache(std::make_unique<SemanticTokenCache>(100))
    , m_enabled(true)
    , m_aggressiveTypeInference(false)
    , m_highlightUnusedSymbols(false)
    , m_asyncEnabled(true)
{
    initializeLanguagePatterns();
    initializeColorScheme();
    
    // Structured logging for initialization
    qDebug() << "[SemanticHighlighter] Initialized with features:"
             << "enabled=" << m_enabled
             << "aggressive=" << m_aggressiveTypeInference
             << "unused=" << m_highlightUnusedSymbols;
}

SemanticHighlighter::~SemanticHighlighter()
{
    qDebug() << "[SemanticHighlighter] Destroyed, cache size:" << m_cache->size();
}

void SemanticHighlighter::initializeLanguagePatterns()
{
    // C++ patterns
    SemanticLanguagePatterns cpp;
    cpp.namespacePattern = QRegularExpression(
        R"(^\s*namespace\s+(\w+)\s*\{)",
        QRegularExpression::MultilineOption
    );
    cpp.classPattern = QRegularExpression(
        R"(^\s*(?:template\s*<[^>]+>\s*)?(?:class|struct)\s+(\w+)(?:\s*:\s*(?:public|private|protected)\s+[\w:,\s]+)?\s*\{)",
        QRegularExpression::MultilineOption
    );
    cpp.structPattern = QRegularExpression(
        R"(^\s*struct\s+(\w+)(?:\s*:\s*(?:public|private|protected)\s+[\w:,\s]+)?\s*\{)",
        QRegularExpression::MultilineOption
    );
    cpp.enumPattern = QRegularExpression(
        R"(^\s*enum\s+(?:class\s+)?(\w+)\s*\{)",
        QRegularExpression::MultilineOption
    );
    cpp.interfacePattern = QRegularExpression(
        R"(^\s*__interface\s+(\w+)\s*\{)",
        QRegularExpression::MultilineOption
    );
    cpp.functionPattern = QRegularExpression(
        R"(^\s*(?:(?:static|virtual|inline|constexpr|explicit|friend|extern)\s+)*)"
        R"((?:(?:const|volatile|unsigned|signed|long|short)\s+)*)"
        R"([\w:]+(?:<[^>]+>)?(?:\s*[*&]+)?\s+)"
        R"((\w+)\s*\([^)]*\)(?:\s*(?:const|noexcept|override|final|=\s*0|=\s*default|=\s*delete))*\s*(?:\{|;|$))",
        QRegularExpression::MultilineOption
    );
    cpp.methodPattern = QRegularExpression(
        R"(^\s*(?:(?:virtual|static|inline|constexpr)\s+)*[\w:]+(?:<[^>]+>)?(?:\s*[*&]+)?\s+(\w+)::(\w+)\s*\([^)]*\))",
        QRegularExpression::MultilineOption
    );
    cpp.variablePattern = QRegularExpression(
        R"(^\s*(?:(?:const|volatile|static|thread_local)\s+)*(?:(?:unsigned|signed|long|short)\s+)*[\w:]+(?:<[^>]+>)?(?:\s*[*&]+)?\s+(\w+)\s*(?:=|;|,))",
        QRegularExpression::MultilineOption
    );
    cpp.parameterPattern = QRegularExpression(
        R"((?:(?:const|volatile)\s+)*(?:(?:unsigned|signed|long|short)\s+)*[\w:]+(?:<[^>]+>)?(?:\s*[*&]+)?\s+(\w+))",
        QRegularExpression::MultilineOption
    );
    cpp.propertyPattern = QRegularExpression(
        R"((?:public|private|protected|internal|static|readonly|virtual|override|abstract|sealed|internal|protected internal|private protected)\s+[\w:]+(?:\s*[*&]+)?\s+(\w+)\s*(?:\{|=|;))",
        QRegularExpression::MultilineOption
    );
    cpp.macroPattern = QRegularExpression(
        R"(^\s*#\s*(define|ifdef|ifndef|if|else|elif|endif|include|pragma|error|warning|line)\s+)",
        QRegularExpression::MultilineOption
    );
    cpp.keywordPattern = QRegularExpression(
        R"(\b(?:auto|break|case|catch|char|class|const|constexpr|continue|default|delete|do|double|else|enum|explicit|export|extern|false|float|for|friend|goto|if|inline|int|long|mutable|namespace|new|nullptr|operator|private|protected|public|ref|register|reinterpret_cast|return|short|signed|sizeof|static|struct|switch|template|this|throw|true|try|typedef|typeid|typename|union|unsigned|using|virtual|void|volatile|wchar_t|while)\b)",
        QRegularExpression::MultilineOption
    );
    cpp.modifierPattern = QRegularExpression(
        R"(\b(?:public|private|protected|internal|static|const|readonly|virtual|override|abstract|sealed|extern|friend|inline|constexpr|noexcept|thread_local|alignas|alignof)\b)",
        QRegularExpression::MultilineOption
    );
    cpp.builtinPattern = QRegularExpression(
        R"(\b(?:std|string|vector|map|set|queue|stack|shared_ptr|unique_ptr|optional|variant|function|lambdas|pair|tuple|array|algorithm|numeric|iterator|chrono|random|stringstream|stringbuf|string_view|regex|mutex|lock_guard|unique_lock|condition_variable|thread|async|future|promise|atomic|reflection|any|codecvt|codecvt_base|codecvt_byname|codecvt_utf8|codecvt_utf16|codecvt_utf8_utf16|facet|ios|ios_base|istream|ostream|iostream|fstream|sstream|streambuf|ios_base_sync_with_stdio|ios_base::fmtflags|ios_base::openmode|ios_base::seekdir|ios_base::beg|ios_base::cur|ios_base::end|ios_base::badbit|ios_base::failbit|ios_base::eofbit|ios_base::goodbit|cout|cin|cerr|clog|endl|ends|flush|noskipws|skipws|showbase|noshowbase|showpoint|noshowpoint|showpos|noshowpos|uppercase|nouppercase|dec|oct|hex|left|right|internal|fixed|scientific|hexfloat|defaultfloat|noshowbase|noshowpoint|noshowpos|noskipws|skipws|unitbuf|nounitbuf|boolalpha|noboolalpha|uppercase|nouppercase|showbase|noshowbase|showpoint|noshowpoint|showpos|noshowpos|left|right|internal|dec|oct|hex|fixed|scientific|hexfloat|defaultfloat)\b)",
        QRegularExpression::MultilineOption
    );
    cpp.decoratorPattern = QRegularExpression(
        R"(\[(?:[\w\.]+\s*(?:,\s*[\w\.]+\s*)*)?\])",
        QRegularExpression::MultilineOption
    );
    m_languagePatterns[QStringLiteral("cpp")] = cpp;
    m_languagePatterns[QStringLiteral("c")] = cpp;
    m_languagePatterns[QStringLiteral("h")] = cpp;
    m_languagePatterns[QStringLiteral("hpp")] = cpp;
    
    // Python patterns
    SemanticLanguagePatterns python;
    python.classPattern = QRegularExpression(
        R"(^\s*class\s+(\w+)(?:\([^)]*\))?\s*:)",
        QRegularExpression::MultilineOption
    );
    python.functionPattern = QRegularExpression(
        R"(^\s*(?:async\s+)?def\s+(\w+)\s*\([^)]*\))",
        QRegularExpression::MultilineOption
    );
    python.methodPattern = QRegularExpression(
        R"(^\s+(?:async\s+)?def\s+(\w+)\s*\(self[^)]*\))",
        QRegularExpression::MultilineOption
    );
    python.variablePattern = QRegularExpression(
        R"(^\s*(\w+)\s*=\s*(?:[^#\n]*?)(?:\n|$))",
        QRegularExpression::MultilineOption
    );
    python.parameterPattern = QRegularExpression(
        R"(\b(\w+)\s*:\s*[\w\[\]]+|\b(\w+)\s*=)",
        QRegularExpression::MultilineOption
    );
    python.keywordPattern = QRegularExpression(
        R"(\b(?:False|class|finally|is|return|None|continue|for|lambda|try|True|def|from|nonlocal|while|and|del|global|not|with|as|elif|if|or|yield|assert|else|import|pass|break|except|in|raise|async|await)\b)",
        QRegularExpression::MultilineOption
    );
    python.modifierPattern = QRegularExpression(
        R"(\b(?:@staticmethod|@classmethod|@property|@abstractmethod|@override|@final|@deprecated|@classmethod|@staticmethod|@cache|lru_cache|functools)\b)",
        QRegularExpression::MultilineOption
    );
    python.builtinPattern = QRegularExpression(
        R"(\b(?:int|float|str|bool|list|dict|set|tuple|range|len|print|input|type|isinstance|hasattr|getattr|setattr|delattr|dir|vars|locals|globals|exec|eval|compile|next|iter|enumerate|zip|reversed|sorted|map|filter|reduce|sum|min|max|abs|round|pow|divmod|ord|chr|bin|oct|hex|bytes|bytearray|memoryview|object|super|__init__|__str__|__repr__|__len__|__getitem__|__setitem__|__delitem__|__contains__|__iter__|__next__|__call__|__add__|__sub__|__mul__|__truediv__|__floordiv__|__mod__|__pow__|__lt__|__le__|__eq__|__ne__|__gt__|__ge__|__hash__|__bool__|__and__|__or__|__xor__|__invert__|__lshift__|__rshift__|__format__|__round__|__ceil__|__floor__|__trunc__)\b)",
        QRegularExpression::MultilineOption
    );
    m_languagePatterns[QStringLiteral("py")] = python;
    m_languagePatterns[QStringLiteral("pyw")] = python;
    
    // JavaScript/TypeScript patterns
    SemanticLanguagePatterns javascript;
    javascript.classPattern = QRegularExpression(
        R"(^\s*(?:export\s+)?(?:abstract\s+)?class\s+(\w+)(?:\s+extends\s+[\w.]+)?(?:\s+implements\s+[\w.,\s]+)?\s*\{)",
        QRegularExpression::MultilineOption
    );
    javascript.functionPattern = QRegularExpression(
        R"((?:^\s*(?:export\s+)?(?:async\s+)?function\s+(\w+)\s*\([^)]*\))|(?:^\s*(?:const|let|var)\s+(\w+)\s*=\s*(?:async\s+)?(?:\([^)]*\)|[^=]+)\s*=>))",
        QRegularExpression::MultilineOption
    );
    javascript.methodPattern = QRegularExpression(
        R"(^\s*(?:public|private|protected|static|async|get|set)?\s*(\w+)\s*\([^)]*\)\s*(?::\s*[\w<>[\],\s|]+)?\s*\{)",
        QRegularExpression::MultilineOption
    );
    javascript.variablePattern = QRegularExpression(
        R"(^\s*(?:const|let|var)\s+(\w+)\s*(?:=|;|,))",
        QRegularExpression::MultilineOption
    );
    javascript.parameterPattern = QRegularExpression(
        R"(\b(\w+)\s*:\s*[\w<>[\],\s|]+|\b(\w+)\s*=)",
        QRegularExpression::MultilineOption
    );
    javascript.keywordPattern = QRegularExpression(
        R"(\b(?:break|case|catch|class|const|continue|debugger|default|delete|do|else|enum|export|extends|finally|for|function|if|import|in|instanceof|new|return|super|switch|this|throw|try|typeof|var|void|while|with|yield|await|async|of|let|static|implements|interface|package|private|protected|public|as|from|get|set)\b)",
        QRegularExpression::MultilineOption
    );
    javascript.modifierPattern = QRegularExpression(
        R"(\b(?:static|readonly|abstract|override|virtual|sealed|async|await)\b)",
        QRegularExpression::MultilineOption
    );
    javascript.builtinPattern = QRegularExpression(
        R"(\b(?:Object|Array|String|Number|Boolean|Date|RegExp|Map|Set|WeakMap|WeakSet|Promise|Error|TypeError|ReferenceError|SyntaxError|URIError|RangeError|EvalError|JSON|Math|Intl|Reflect|Proxy|ArrayBuffer|DataView|TypedArray|SharedArrayBuffer|Global|Math|DOM|Window|Document|Element|Node|Event|EventTarget|XMLHttpRequest|Fetch|FormData|FileReader|Blob|URL|URLSearchParams|console|window|document|module|require|exports|global|Buffer|process|BufferEncoding|BufferConstants|BufferPool|BufferStatic|BufferPrototype|BufferClass|BufferStaticMethods|BufferPrototypeMethods|BufferInstanceMethods|BufferPool|BufferPoolMethods|BufferPoolInstance|BufferPoolInstanceMethods)\b)",
        QRegularExpression::MultilineOption
    );
    javascript.decoratorPattern = QRegularExpression(
        R"(@[\w\.]+)",
        QRegularExpression::MultilineOption
    );
    m_languagePatterns[QStringLiteral("js")] = javascript;
    m_languagePatterns[QStringLiteral("jsx")] = javascript;
    m_languagePatterns[QStringLiteral("ts")] = javascript;
    m_languagePatterns[QStringLiteral("tsx")] = javascript;
    
    // Rust patterns
    SemanticLanguagePatterns rust;
    rust.classPattern = QRegularExpression(
        R"(^\s*(?:pub(?:\([^)]+\))?\s+)?(?:struct|enum|trait)\s+(\w+)(?:<[^>]+>)?)",
        QRegularExpression::MultilineOption
    );
    rust.functionPattern = QRegularExpression(
        R"(^\s*(?:pub(?:\([^)]+\))?\s+)?(?:async\s+)?fn\s+(\w+)(?:<[^>]+>)?\s*\([^)]*\))",
        QRegularExpression::MultilineOption
    );
    rust.methodPattern = QRegularExpression(
        R"(^\s*(?:pub(?:\([^)]+\))?\s+)?(?:async\s+)?fn\s+(\w+)(?:<[^>]+>)?\s*\(&?(?:mut\s+)?self)",
        QRegularExpression::MultilineOption
    );
    rust.variablePattern = QRegularExpression(
        R"(^\s*(?:(?:pub\s*\([^)]+\)\s+)?(?:const|let|let mut|static)\s+(\w+)\s*(?:=|:))",
        QRegularExpression::MultilineOption
    );
    rust.parameterPattern = QRegularExpression(
        R"(\b(\w+)\s*:\s*[\w<>[\],\s|]+|\b(\w+)\s*=)",
        QRegularExpression::MultilineOption
    );
    rust.keywordPattern = QRegularExpression(
        R"(\b(?:as|async|await|break|const|continue|crate|else|enum|extern|false|fn|for|if|impl|in|let|loop|match|mod|move|mut|pub|ref|return|self|Self|static|struct|super|trait|true|type|unsafe|use|where|while|const fn|async fn|async move|const unsafe fn|extern \"C\"|extern \"system\"|unsafe fn|async trait|default impl|const impl)\b)",
        QRegularExpression::MultilineOption
    );
    rust.modifierPattern = QRegularExpression(
        R"(\b(?:pub|pub(crate)|pub(super)|pub(in)|const|async|unsafe|static|lifetime|'static)\b)",
        QRegularExpression::MultilineOption
    );
    rust.builtinPattern = QRegularExpression(
        R"(\b(?:Option|Result|String|&str|&String|&mut String|Vec|&Vec|&mut Vec|HashMap|HashSet|BTreeMap|BTreeSet|Box|Arc|Rc|Pin|RefCell|Cell|OnceCell|PhantomData|unwrap|expect|map|and_then|or_else|ok|err|is_some|is_none|unwrap_some|unwrap_err|clone|copy|move|drop|format|println|print|eprintln|debug_assert|debug_assert_eq|debug_assert_ne|assert|assert_eq|assert_ne|panic|unreachable|unimplemented)\b)",
        QRegularExpression::MultilineOption
    );
    rust.decoratorPattern = QRegularExpression(
        R"(#\[derive\([^\)]*\)\]|#\[cfg\([^\)]*\)\]|#\[allow\([^\)]*\)\]|#\[warn\([^\)]*\)\]|#\[error\([^\)]*\)\]|#\[deprecated\]|#\[inline\]|#\[no_mangle\]|#\[extern\([^\)]*\)\])",
        QRegularExpression::MultilineOption
    );
    m_languagePatterns[QStringLiteral("rs")] = rust;
    
    // Go patterns
    SemanticLanguagePatterns golang;
    golang.classPattern = QRegularExpression(
        R"(^\s*type\s+(\w+)\s+(?:struct|interface)\s*\{)",
        QRegularExpression::MultilineOption
    );
    golang.functionPattern = QRegularExpression(
        R"(^\s*func\s+(\w+)\s*\([^)]*\))",
        QRegularExpression::MultilineOption
    );
    golang.methodPattern = QRegularExpression(
        R"(^\s*func\s+\(\s*\w+\s+\*?(\w+)\s*\)\s+(\w+)\s*\([^)]*\))",
        QRegularExpression::MultilineOption
    );
    golang.variablePattern = QRegularExpression(
        R"(^\s*(?:const|var)\s+(\w+)\s*(?:=|:|= ))",
        QRegularExpression::MultilineOption
    );
    golang.parameterPattern = QRegularExpression(
        R"(\b(\w+)\s+[\w\[\]]+|\b(\w+)\s+=)",
        QRegularExpression::MultilineOption
    );
    golang.keywordPattern = QRegularExpression(
        R"(\b(?:break|case|chan|const|continue|default|defer|else|fallthrough|for|func|go|goto|if|import|interface|map|package|range|return|select|struct|switch|type|var)\b)",
        QRegularExpression::MultilineOption
    );
    golang.modifierPattern = QRegularExpression(
        R"(\b(?:var|const|func|interface|struct|map|chan|package|import|return|if|else|switch|for|break|continue|fallthrough|default|select|case|defer|go|goto|range|var|type|const|func|interface|struct|map|chan|package|import|return|if|else|switch|for|break|continue|fallthrough|default|select|case|defer|go|goto|range)\b)",
        QRegularExpression::MultilineOption
    );
    golang.builtinPattern = QRegularExpression(
        R"(\b(?:bool|byte|complex64|complex128|error|float32|float64|int|int8|int16|int32|int64|rune|string|uint|uint8|uint16|uint32|uint64|uintptr|true|false|nil|iota|append|cap|close|complex|copy|delete|imag|len|make|new|panic|print|println|real|recover)\b)",
        QRegularExpression::MultilineOption
    );
    m_languagePatterns[QStringLiteral("go")] = golang;
}

void SemanticHighlighter::initializeColorScheme()
{
    // Default color scheme for light theme
    m_tokenColors[SemanticTokenType::Namespace] = QColor(0, 122, 204);      // Blue
    m_tokenColors[SemanticTokenType::Class] = QColor(0, 122, 204);          // Blue
    m_tokenColors[SemanticTokenType::Struct] = QColor(0, 122, 204);         // Blue
    m_tokenColors[SemanticTokenType::Interface] = QColor(0, 122, 204);      // Blue
    m_tokenColors[SemanticTokenType::Enum] = QColor(170, 0, 255);           // Purple
    m_tokenColors[SemanticTokenType::Type] = QColor(0, 122, 204);           // Blue
    m_tokenColors[SemanticTokenType::TypeParameter] = QColor(255, 180, 0);   // Orange
    m_tokenColors[SemanticTokenType::Parameter] = QColor(106, 115, 125);     // Gray
    m_tokenColors[SemanticTokenType::Variable] = QColor(153, 0, 153);        // Magenta
    m_tokenColors[SemanticTokenType::Property] = QColor(153, 0, 153);        // Magenta
    m_tokenColors[SemanticTokenType::EnumMember] = QColor(255, 180, 0);       // Orange
    m_tokenColors[SemanticTokenType::Function] = QColor(0, 153, 0);          // Green
    m_tokenColors[SemanticTokenType::Method] = QColor(0, 153, 0);            // Green
    m_tokenColors[SemanticTokenType::Macro] = QColor(128, 128, 128);         // Gray
    m_tokenColors[SemanticTokenType::Keyword] = QColor(170, 0, 255);        // Purple
    m_tokenColors[SemanticTokenType::Modifier] = QColor(170, 0, 255);       // Purple
    m_tokenColors[SemanticTokenType::Comment] = QColor(106, 115, 125);       // Gray
    m_tokenColors[SemanticTokenType::String] = QColor(204, 102, 0);         // Brown
    m_tokenColors[SemanticTokenType::Number] = QColor(170, 0, 255);          // Purple
    m_tokenColors[SemanticTokenType::Regexp] = QColor(204, 102, 0);         // Brown
    m_tokenColors[SemanticTokenType::Operator] = QColor(170, 0, 255);        // Purple
    m_tokenColors[SemanticTokenType::Decorator] = QColor(0, 153, 0);          // Green
    m_tokenColors[SemanticTokenType::Builtin] = QColor(170, 0, 255);         // Purple
    m_tokenColors[SemanticTokenType::Event] = QColor(255, 0, 255);           // Magenta
    m_tokenColors[SemanticTokenType::Unknown] = QColor(153, 0, 153);         // Magenta
    
    // Modifier colors
    m_modifierColors[SemanticTokenModifier::Declaration] = QColor(0, 0, 0);        // Black
    m_modifierColors[SemanticTokenModifier::Definition] = QColor(0, 0, 0);         // Black
    m_modifierColors[SemanticTokenModifier::Readonly] = QColor(0, 0, 255);          // Blue
    m_modifierColors[SemanticTokenModifier::Static] = QColor(255, 0, 0);           // Red
    m_modifierColors[SemanticTokenModifier::Deprecated] = QColor(255, 0, 0);       // Red
    m_modifierColors[SemanticTokenModifier::Abstract] = QColor(128, 0, 128);      // Purple
    m_modifierColors[SemanticTokenModifier::Async] = QColor(0, 128, 0);           // Green
    m_modifierColors[SemanticTokenModifier::Modification] = QColor(255, 0, 0);     // Red
    m_modifierColors[SemanticTokenModifier::Documentation] = QColor(0, 0, 255);    // Blue
    m_modifierColors[SemanticTokenModifier::DefaultLibrary] = QColor(128, 128, 128); // Gray
}

QString SemanticHighlighter::detectLanguage(const QString& filePath) const
{
    QFileInfo fileInfo(filePath);
    QString extension = fileInfo.suffix().toLower();
    
    // Map common extensions to language keys
    static const QHash<QString, QString> extensionMap = {
        {QStringLiteral("cpp"), QStringLiteral("cpp")},
        {QStringLiteral("cxx"), QStringLiteral("cpp")},
        {QStringLiteral("cc"), QStringLiteral("cpp")},
        {QStringLiteral("c"), QStringLiteral("c")},
        {QStringLiteral("h"), QStringLiteral("h")},
        {QStringLiteral("hpp"), QStringLiteral("hpp")},
        {QStringLiteral("hxx"), QStringLiteral("hpp")},
        {QStringLiteral("py"), QStringLiteral("py")},
        {QStringLiteral("pyw"), QStringLiteral("pyw")},
        {QStringLiteral("js"), QStringLiteral("js")},
        {QStringLiteral("jsx"), QStringLiteral("jsx")},
        {QStringLiteral("ts"), QStringLiteral("ts")},
        {QStringLiteral("tsx"), QStringLiteral("tsx")},
        {QStringLiteral("rs"), QStringLiteral("rs")},
        {QStringLiteral("go"), QStringLiteral("go")}
    };
    
    return extensionMap.value(extension, QString());
}

QString SemanticHighlighter::computeContentHash(const QString& content) const
{
    return QString::fromLatin1(
        QCryptographicHash::hash(content.toUtf8(), QCryptographicHash::Md5).toHex()
    );
}

QList<SemanticToken> SemanticHighlighter::highlightSemanticTokens(const QString& content)
{
    // For backward compatibility, treat content as filePath
    return highlightSemanticTokens(content, content);
}

QList<SemanticToken> SemanticHighlighter::highlightSemanticTokens(const QString& filePath, const QString& content)
{
    const QString language = detectLanguage(filePath);
    const QString contentHash = computeContentHash(content);
    
    // Check cache first
    QList<SemanticToken> cachedTokens;
    if (m_cache->lookup(filePath, contentHash, cachedTokens)) {
        qDebug() << "[SemanticHighlighter] Cache hit for" << filePath;
        return cachedTokens;
    }
    
    qDebug() << "[SemanticHighlighter] Computing semantic tokens for" << filePath << "language:" << language;
    
    QList<SemanticToken> tokens;
    
    if (m_enabled) {
        // Extract semantic scopes
        QList<SemanticScope> scopes = extractSemanticScopes(content, language);
        
        // Resolve symbol references
        resolveSymbolReferences(scopes, content);
        
        // Infer variable types if aggressive mode
        if (m_aggressiveTypeInference) {
            inferVariableTypes(scopes, content, language);
        }
        
        // Merge tokens from scopes
        tokens = mergeTokensFromScopes(scopes);
        
        // Apply color scheme
        applyColorScheme(tokens);
        
        // Filter based on enabled features
        tokens = filterTokens(tokens);
    }
    
    // Cache the results
    m_cache->insert(filePath, tokens, contentHash);
    
    qDebug() << "[SemanticHighlighter] Generated" << tokens.size() << "semantic tokens for" << filePath;
    
    return tokens;
}

QList<SemanticToken> SemanticHighlighter::highlightSemanticTokensForRange(
    const QString& content,
    int startLine,
    int endLine
)
{
    QList<SemanticToken> allTokens = highlightSemanticTokens(content);
    QList<SemanticToken> rangeTokens;
    
    for (const SemanticToken& token : allTokens) {
        if (token.line >= startLine && token.line <= endLine) {
            rangeTokens.append(token);
        }
    }
    
    return rangeTokens;
}

void SemanticHighlighter::highlightSemanticTokensAsync(
    const QString& filePath,
    const QString& content,
    std::function<void(const QList<SemanticToken>&)> callback
)
{
    if (!m_asyncEnabled) {
        // Fall back to synchronous
        callback(highlightSemanticTokens(filePath, content));
        return;
    }
    
    QFuture<QList<SemanticToken>> future = QtConcurrent::run([this, filePath, content]() {
        return highlightSemanticTokens(filePath, content);
    });
    
    QFutureWatcher<QList<SemanticToken>>* watcher = new QFutureWatcher<QList<SemanticToken>>(this);
    connect(watcher, &QFutureWatcher<QList<SemanticToken>>::finished, this, [watcher, callback]() {
        callback(watcher->result());
        watcher->deleteLater();
    });
    
    watcher->setFuture(future);
}

void SemanticHighlighter::invalidateFile(const QString& filePath)
{
    m_cache->invalidate(filePath);
    qDebug() << "[SemanticHighlighter] Invalidated cache for" << filePath;
}

void SemanticHighlighter::clearCache()
{
    m_cache->clear();
    qDebug() << "[SemanticHighlighter] Cache cleared";
}

void SemanticHighlighter::setTokenColor(SemanticTokenType type, const QColor& color)
{
    m_tokenColors[type] = color;
    qDebug() << "[SemanticHighlighter] Set color for" << static_cast<int>(type) << "to" << color.name();
}

QColor SemanticHighlighter::tokenColor(SemanticTokenType type) const
{
    return m_tokenColors.value(type, QColor(0, 0, 0));
}

void SemanticHighlighter::setModifierColor(SemanticTokenModifier modifier, const QColor& color)
{
    m_modifierColors[modifier] = color;
}

QColor SemanticHighlighter::modifierColor(SemanticTokenModifier modifier) const
{
    return m_modifierColors.value(modifier, QColor(0, 0, 0));
}

void SemanticHighlighter::setEnabled(bool enabled)
{
    m_enabled = enabled;
    if (!enabled) {
        clearCache();
    }
}

bool SemanticHighlighter::isEnabled() const
{
    return m_enabled;
}

void SemanticHighlighter::setAggressiveTypeInference(bool aggressive)
{
    m_aggressiveTypeInference = aggressive;
    if (aggressive) {
        clearCache(); // Need to recompute with new inference
    }
}

bool SemanticHighlighter::aggressiveTypeInference() const
{
    return m_aggressiveTypeInference;
}

void SemanticHighlighter::setHighlightUnusedSymbols(bool highlight)
{
    m_highlightUnusedSymbols = highlight;
    if (highlight) {
        clearCache(); // Need to analyze usage
    }
}

bool SemanticHighlighter::highlightUnusedSymbols() const
{
    return m_highlightUnusedSymbols;
}

QList<SemanticToken> SemanticHighlighter::extractSemanticTokens(
    const QString& content,
    const QString& language
) const
{
    QList<SemanticToken> tokens;
    auto it = m_languagePatterns.find(language);
    if (it == m_languagePatterns.end()) {
        return tokens;
    }
    
    const SemanticLanguagePatterns& patterns = *it;
    QStringList lines = content.split(QLatin1Char('\n'));
    
    // Extract classes/structs/interfaces
    if (patterns.classPattern.isValid()) {
        QRegularExpressionMatchIterator iter = patterns.classPattern.globalMatch(content);
        while (iter.hasNext()) {
            QRegularExpressionMatch match = iter.next();
            SemanticToken token;
            token.line = content.left(match.capturedStart()).count(QLatin1Char('\n'));
            token.column = match.capturedStart() - content.lastIndexOf(QLatin1Char('\n'), match.capturedStart()) - 1;
            token.length = match.captured(0).length();
            token.text = match.captured(1);
            token.type = SemanticTokenType::Class;
            token.modifiers.append(SemanticTokenModifier::Declaration);
            tokens.append(token);
        }
    }
    
    // Extract enums
    if (patterns.enumPattern.isValid()) {
        QRegularExpressionMatchIterator iter = patterns.enumPattern.globalMatch(content);
        while (iter.hasNext()) {
            QRegularExpressionMatch match = iter.next();
            SemanticToken token;
            token.line = content.left(match.capturedStart()).count(QLatin1Char('\n'));
            token.column = match.capturedStart() - content.lastIndexOf(QLatin1Char('\n'), match.capturedStart()) - 1;
            token.length = match.captured(0).length();
            token.text = match.captured(1);
            token.type = SemanticTokenType::Enum;
            token.modifiers.append(SemanticTokenModifier::Declaration);
            tokens.append(token);
        }
    }
    
    // Extract functions
    if (patterns.functionPattern.isValid()) {
        QRegularExpressionMatchIterator iter = patterns.functionPattern.globalMatch(content);
        while (iter.hasNext()) {
            QRegularExpressionMatch match = iter.next();
            SemanticToken token;
            token.line = content.left(match.capturedStart()).count(QLatin1Char('\n'));
            token.column = match.capturedStart() - content.lastIndexOf(QLatin1Char('\n'), match.capturedStart()) - 1;
            token.length = match.captured(0).length();
            
            // Try different capture groups (different languages use different groups)
            for (int i = 1; i <= match.lastCapturedIndex(); ++i) {
                if (!match.captured(i).isEmpty()) {
                    token.text = match.captured(i);
                    break;
                }
            }
            
            token.type = SemanticTokenType::Function;
            token.modifiers.append(SemanticTokenModifier::Declaration);
            tokens.append(token);
        }
    }
    
    // Extract methods
    if (patterns.methodPattern.isValid()) {
        QRegularExpressionMatchIterator iter = patterns.methodPattern.globalMatch(content);
        while (iter.hasNext()) {
            QRegularExpressionMatch match = iter.next();
            SemanticToken token;
            token.line = content.left(match.capturedStart()).count(QLatin1Char('\n'));
            token.column = match.capturedStart() - content.lastIndexOf(QLatin1Char('\n'), match.capturedStart()) - 1;
            token.length = match.captured(0).length();
            
            // For C++ methods, name is in capture group 2 (after class name)
            // For other languages, it may be in group 1
            for (int i = match.lastCapturedIndex(); i >= 1; --i) {
                if (!match.captured(i).isEmpty()) {
                    token.text = match.captured(i);
                    break;
                }
            }
            
            token.type = SemanticTokenType::Method;
            token.modifiers.append(SemanticTokenModifier::Declaration);
            tokens.append(token);
        }
    }
    
    // Extract variables
    if (patterns.variablePattern.isValid()) {
        QRegularExpressionMatchIterator iter = patterns.variablePattern.globalMatch(content);
        while (iter.hasNext()) {
            QRegularExpressionMatch match = iter.next();
            SemanticToken token;
            token.line = content.left(match.capturedStart()).count(QLatin1Char('\n'));
            token.column = match.capturedStart() - content.lastIndexOf(QLatin1Char('\n'), match.capturedStart()) - 1;
            token.length = match.captured(1).length();
            token.text = match.captured(1);
            token.type = SemanticTokenType::Variable;
            token.modifiers.append(SemanticTokenModifier::Declaration);
            tokens.append(token);
        }
    }
    
    // Extract parameters
    if (patterns.parameterPattern.isValid()) {
        QRegularExpressionMatchIterator iter = patterns.parameterPattern.globalMatch(content);
        while (iter.hasNext()) {
            QRegularExpressionMatch match = iter.next();
            SemanticToken token;
            token.line = content.left(match.capturedStart()).count(QLatin1Char('\n'));
            token.column = match.capturedStart() - content.lastIndexOf(QLatin1Char('\n'), match.capturedStart()) - 1;
            token.length = match.captured(0).length();
            
            // Try different capture groups
            for (int i = 0; i <= match.lastCapturedIndex(); ++i) {
                if (!match.captured(i).isEmpty() && match.captured(i) != token.text) {
                    token.text = match.captured(i);
                    break;
                }
            }
            
            token.type = SemanticTokenType::Parameter;
            token.modifiers.append(SemanticTokenModifier::Declaration);
            tokens.append(token);
        }
    }
    
    // Extract keywords
    if (patterns.keywordPattern.isValid()) {
        QRegularExpressionMatchIterator iter = patterns.keywordPattern.globalMatch(content);
        while (iter.hasNext()) {
            QRegularExpressionMatch match = iter.next();
            SemanticToken token;
            token.line = content.left(match.capturedStart()).count(QLatin1Char('\n'));
            token.column = match.capturedStart() - content.lastIndexOf(QLatin1Char('\n'), match.capturedStart()) - 1;
            token.length = match.captured(0).length();
            token.text = match.captured(0);
            token.type = SemanticTokenType::Keyword;
            tokens.append(token);
        }
    }
    
    // Extract builtins
    if (patterns.builtinPattern.isValid()) {
        QRegularExpressionMatchIterator iter = patterns.builtinPattern.globalMatch(content);
        while (iter.hasNext()) {
            QRegularExpressionMatch match = iter.next();
            SemanticToken token;
            token.line = content.left(match.capturedStart()).count(QLatin1Char('\n'));
            token.column = match.capturedStart() - content.lastIndexOf(QLatin1Char('\n'), match.capturedStart()) - 1;
            token.length = match.captured(0).length();
            token.text = match.captured(0);
            token.type = SemanticTokenType::Builtin;
            token.modifiers.append(SemanticTokenModifier::DefaultLibrary);
            tokens.append(token);
        }
    }
    
    return tokens;
}

QList<SemanticScope> SemanticHighlighter::extractSemanticScopes(
    const QString& content,
    const QString& language
) const
{
    QList<SemanticScope> scopes;
    auto it = m_languagePatterns.find(language);
    if (it == m_languagePatterns.end()) {
        return scopes;
    }
    
    const SemanticLanguagePatterns& patterns = *it;
    QStringList lines = content.split(QLatin1Char('\n'));
    
    // Track nesting level and current scope
    QList<SemanticScope> scopeStack;
    int currentLine = 0;
    
    for (int i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];
        
        // Try to match class/struct/interface definitions
        if (patterns.classPattern.isValid()) {
            QRegularExpressionMatch match = patterns.classPattern.match(line);
            if (match.hasMatch()) {
                QString className = match.captured(1);
                SemanticScope scope(className, SemanticTokenType::Class, i);
                scopeStack.append(scope);
                continue;
            }
        }
        
        // Try to match namespace definitions
        if (patterns.namespacePattern.isValid()) {
            QRegularExpressionMatch match = patterns.namespacePattern.match(line);
            if (match.hasMatch()) {
                QString namespaceName = match.captured(1);
                SemanticScope scope(namespaceName, SemanticTokenType::Namespace, i);
                scopeStack.append(scope);
                continue;
            }
        }
        
        // Try to match function definitions
        if (patterns.functionPattern.isValid()) {
            QRegularExpressionMatch match = patterns.functionPattern.match(line);
            if (match.hasMatch()) {
                QString functionName;
                for (int j = 1; j <= match.lastCapturedIndex(); ++j) {
                    if (!match.captured(j).isEmpty()) {
                        functionName = match.captured(j);
                        break;
                    }
                }
                
                if (!functionName.isEmpty()) {
                    SemanticTokenType type = scopeStack.isEmpty() ? SemanticTokenType::Function : SemanticTokenType::Method;
                    SemanticScope scope(functionName, type, i);
                    
                    // Determine end line
                    scope.endLine = findMatchingEnd(lines, i);
                    if (scope.endLine < 0) {
                        scope.endLine = lines.size() - 1;
                    }
                    
                    scopeStack.append(scope);
                }
            }
        }
        
        // Check for scope endings
        if (line.contains(QLatin1Char('}'))) {
            // Pop scopes that end at this line
            for (int j = scopeStack.size() - 1; j >= 0; --j) {
                if (scopeStack[j].endLine <= i) {
                    if (j == scopeStack.size() - 1) {
                        scopeStack.removeLast();
                    } else {
                        scopeStack.removeAt(j);
                    }
                }
            }
        }
        
        // Extract tokens for this line within each scope
        for (const SemanticScope& scope : scopeStack) {
            if (i >= scope.startLine && i <= scope.endLine) {
                // Extract tokens from this line
                QList<SemanticToken> lineTokens = extractSemanticTokensFromLine(line, i, patterns);
                for (SemanticToken& token : lineTokens) {
                    scope.tokens.append(token);
                }
            }
        }
    }
    
    // Add all scopes to the result
    for (const SemanticScope& scope : scopeStack) {
        scopes.append(scope);
    }
    
    return scopes;
}

QList<SemanticToken> SemanticHighlighter::extractSemanticTokensFromLine(
    const QString& line,
    int lineNumber,
    const SemanticLanguagePatterns& patterns
) const
{
    QList<SemanticToken> tokens;
    
    // Extract keywords
    if (patterns.keywordPattern.isValid()) {
        QRegularExpressionMatchIterator iter = patterns.keywordPattern.globalMatch(line);
        while (iter.hasNext()) {
            QRegularExpressionMatch match = iter.next();
            SemanticToken token;
            token.line = lineNumber;
            token.column = match.capturedStart();
            token.length = match.captured(0).length();
            token.text = match.captured(0);
            token.type = SemanticTokenType::Keyword;
            tokens.append(token);
        }
    }
    
    // Extract builtins
    if (patterns.builtinPattern.isValid()) {
        QRegularExpressionMatchIterator iter = patterns.builtinPattern.globalMatch(line);
        while (iter.hasNext()) {
            QRegularExpressionMatch match = iter.next();
            SemanticToken token;
            token.line = lineNumber;
            token.column = match.capturedStart();
            token.length = match.captured(0).length();
            token.text = match.captured(0);
            token.type = SemanticTokenType::Builtin;
            token.modifiers.append(SemanticTokenModifier::DefaultLibrary);
            tokens.append(token);
        }
    }
    
    return tokens;
}

int SemanticHighlighter::findMatchingEnd(const QStringList& lines, int startLine) const
{
    if (startLine >= lines.size()) {
        return -1;
    }
    
    // Count braces to find matching end
    int braceCount = 0;
    for (int i = startLine; i < lines.size(); ++i) {
        const QString& line = lines[i];
        for (QChar c : line) {
            if (c == QLatin1Char('{')) {
                braceCount++;
            } else if (c == QLatin1Char('}')) {
                braceCount--;
                if (braceCount == 0) {
                    return i;
                }
            }
        }
    }
    
    return -1;
}

void SemanticHighlighter::resolveSymbolReferences(
    QList<SemanticScope>& scopes,
    const QString& content
) const
{
    // Build symbol table
    QHash<QString, SemanticToken> symbolTable;
    for (const SemanticScope& scope : scopes) {
        for (const SemanticToken& token : scope.tokens) {
            if (token.modifiers.contains(SemanticTokenModifier::Declaration) ||
                token.modifiers.contains(SemanticTokenModifier::Definition)) {
                symbolTable[token.text] = token;
            }
        }
    }
    
    // Resolve references within each scope
    for (SemanticScope& scope : scopes) {
        for (SemanticToken& token : scope.tokens) {
            if (symbolTable.contains(token.text) && 
                !token.modifiers.contains(SemanticTokenModifier::Declaration)) {
                // This is a reference, not a declaration
                token.modifiers.append(SemanticTokenModifier::Modification);
            }
        }
    }
}

void SemanticHighlighter::inferVariableTypes(
    QList<SemanticScope>& scopes,
    const QString& content,
    const QString& language
) const
{
    // Simple type inference based on assignment patterns
    QStringList lines = content.split(QLatin1Char('\n'));
    
    for (SemanticScope& scope : scopes) {
        for (const SemanticToken& token : scope.tokens) {
            if (token.type == SemanticTokenType::Variable) {
                // Find assignment on same line
                int lineIndex = token.line;
                if (lineIndex < lines.size()) {
                    QString line = lines[lineIndex];
                    if (line.contains(token.text + QLatin1Char('='))) {
                        // Simple type inference
                        QString assignment = line.mid(line.indexOf(token.text));
                        if (assignment.contains(QLatin1Char('"'))) {
                            // String literal
                            token.modifiers.append(SemanticTokenModifier::Documentation);
                        } else if (assignment.contains(QLatin1Char('['))) {
                            // Array/list
                            token.modifiers.append(SemanticTokenModifier::Modification);
                        } else if (assignment.contains(QLatin1Char('{'))) {
                            // Object/dict
                            token.modifiers.append(SemanticTokenModifier::Definition);
                        } else if (assignment.contains(QLatin1Char('('))) {
                            // Function call
                            token.modifiers.append(SemanticTokenModifier::Declaration);
                        }
                    }
                }
            }
        }
    }
}

QList<SemanticToken> SemanticHighlighter::mergeTokensFromScopes(const QList<SemanticScope>& scopes) const
{
    QList<SemanticToken> allTokens;
    
    for (const SemanticScope& scope : scopes) {
        allTokens.append(scope.tokens);
    }
    
    return allTokens;
}

void SemanticHighlighter::applyColorScheme(QList<SemanticToken>& tokens) const
{
    for (SemanticToken& token : tokens) {
        // Colors are applied during rendering based on token type and modifiers
        // This method can be extended to add additional semantic information
        // such as deprecated status, unused symbols, etc.
        
        // Check for deprecated symbols
        if (token.text.contains(QLatin1String("deprecated"), Qt::CaseInsensitive) ||
            token.text.contains(QLatin1String("TODO")) ||
            token.text.contains(QLatin1String("FIXME")) ||
            token.text.contains(QLatin1String("HACK")) ||
            token.text.contains(QLatin1String("XXX"))) {
            token.modifiers.append(SemanticTokenModifier::Deprecated);
        }
    }
}

QList<SemanticToken> SemanticHighlighter::filterTokens(const QList<SemanticToken>& tokens) const
{
    if (!m_highlightUnusedSymbols) {
        // Remove tokens with only declaration modifier (unused symbols)
        return tokens;
    }
    
    QList<SemanticToken> filtered;
    for (const SemanticToken& token : tokens) {
        if (token.modifiers.contains(SemanticTokenModifier::Modification) ||
            token.modifiers.contains(SemanticTokenModifier::Definition) ||
            token.modifiers.contains(SemanticTokenModifier::Declaration) ||
            token.modifiers.contains(SemanticTokenModifier::Readonly) ||
            token.modifiers.contains(SemanticTokenModifier::Static) ||
            token.modifiers.contains(SemanticTokenModifier::Abstract) ||
            token.modifiers.contains(SemanticTokenModifier::Async) ||
            token.modifiers.contains(SemanticTokenModifier::Documentation) ||
            token.modifiers.contains(SemanticTokenModifier::DefaultLibrary) ||
            token.modifiers.contains(SemanticTokenModifier::Deprecated)) {
            filtered.append(token);
        }
    }
    
    return filtered;
}

} // namespace RawrXD
