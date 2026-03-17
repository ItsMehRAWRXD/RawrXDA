/**
 * @file CodeLensProvider.cpp
 * @brief Complete LSP CodeLens Implementation for RawrXD Agentic IDE
 * 
 * Provides actionable code hints displayed inline above functions, classes, and other
 * symbols. Implements reference counting, test detection, git blame integration,
 * performance annotations, and custom lens types.
 * 
 * @author RawrXD Team
 * @copyright 2024 RawrXD
 */

#include "CodeLensProvider.h"
#include <QRegularExpression>
#include <QRegularExpressionMatchIterator>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDateTime>
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
// CodeLensItem Implementation
// ============================================================================

CodeLensItem::CodeLensItem()
    : m_line(0)
    , m_column(0)
    , m_type(CodeLensType::Reference)
    , m_referenceCount(0)
    , m_priority(0)
    , m_isClickable(true)
    , m_isVisible(true)
{
}

CodeLensItem::CodeLensItem(int line, int column, const QString& text, CodeLensType type)
    : m_line(line)
    , m_column(column)
    , m_text(text)
    , m_type(type)
    , m_referenceCount(0)
    , m_priority(static_cast<int>(type))
    , m_isClickable(true)
    , m_isVisible(true)
{
}

bool CodeLensItem::operator<(const CodeLensItem& other) const
{
    if (m_line != other.m_line) {
        return m_line < other.m_line;
    }
    if (m_priority != other.m_priority) {
        return m_priority > other.m_priority; // Higher priority first
    }
    return m_column < other.m_column;
}

bool CodeLensItem::isValid() const
{
    return m_line >= 0 && !m_text.isEmpty();
}

QString CodeLensItem::toDisplayString() const
{
    QString prefix;
    switch (m_type) {
        case CodeLensType::Reference:
            prefix = QStringLiteral("⟁ ");
            break;
        case CodeLensType::Implementation:
            prefix = QStringLiteral("⟐ ");
            break;
        case CodeLensType::Test:
            prefix = QStringLiteral("⚡ ");
            break;
        case CodeLensType::GitBlame:
            prefix = QStringLiteral("⎇ ");
            break;
        case CodeLensType::Performance:
            prefix = QStringLiteral("⏱ ");
            break;
        case CodeLensType::Documentation:
            prefix = QStringLiteral("📖 ");
            break;
        case CodeLensType::Debug:
            prefix = QStringLiteral("🔍 ");
            break;
        case CodeLensType::Custom:
            prefix = QStringLiteral("◆ ");
            break;
    }
    return prefix + m_text;
}

// ============================================================================
// SymbolInfo Implementation  
// ============================================================================

SymbolInfo::SymbolInfo()
    : line(0)
    , column(0)
    , endLine(0)
    , endColumn(0)
    , type(SymbolType::Unknown)
{
}

bool SymbolInfo::isValid() const
{
    return !name.isEmpty() && line >= 0;
}

// ============================================================================
// CodeLensCache Implementation
// ============================================================================

CodeLensCache::CodeLensCache(int maxSize)
    : m_maxSize(maxSize)
{
}

void CodeLensCache::insert(const QString& key, const QList<CodeLensItem>& items, const QString& contentHash)
{
    QMutexLocker locker(&m_mutex);
    
    CacheEntry entry;
    entry.items = items;
    entry.contentHash = contentHash;
    entry.timestamp = QDateTime::currentMSecsSinceEpoch();
    
    m_cache[key] = entry;
    
    // Evict old entries if cache is too large
    if (m_cache.size() > m_maxSize) {
        evictOldest();
    }
}

bool CodeLensCache::lookup(const QString& key, const QString& contentHash, QList<CodeLensItem>& outItems) const
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
    
    outItems = it->items;
    return true;
}

void CodeLensCache::invalidate(const QString& key)
{
    QMutexLocker locker(&m_mutex);
    m_cache.remove(key);
}

void CodeLensCache::clear()
{
    QMutexLocker locker(&m_mutex);
    m_cache.clear();
}

int CodeLensCache::size() const
{
    QMutexLocker locker(&m_mutex);
    return m_cache.size();
}

void CodeLensCache::evictOldest()
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
// CodeLensProvider Implementation
// ============================================================================

CodeLensProvider::CodeLensProvider(QObject* parent)
    : QObject(parent)
    , m_cache(std::make_unique<CodeLensCache>(100))
    , m_enableReferenceCounting(true)
    , m_enableTestDetection(true)
    , m_enableGitBlame(true)
    , m_enablePerformanceHints(true)
    , m_enableDocumentationLinks(true)
    , m_asyncEnabled(true)
    , m_minReferencesForDisplay(1)
{
    initializeLanguagePatterns();
    
    // Structured logging for initialization
    qDebug() << "[CodeLensProvider] Initialized with features:"
             << "references=" << m_enableReferenceCounting
             << "tests=" << m_enableTestDetection
             << "git=" << m_enableGitBlame
             << "performance=" << m_enablePerformanceHints;
}

CodeLensProvider::~CodeLensProvider()
{
    qDebug() << "[CodeLensProvider] Destroyed, cache size:" << m_cache->size();
}

void CodeLensProvider::initializeLanguagePatterns()
{
    // C++ patterns
    LanguagePatterns cpp;
    cpp.functionPattern = QRegularExpression(
        R"(^\s*(?:(?:static|virtual|inline|constexpr|explicit|friend|extern)\s+)*)"
        R"((?:(?:const|volatile|unsigned|signed|long|short)\s+)*)"
        R"([\w:]+(?:<[^>]+>)?(?:\s*[*&]+)?\s+)"
        R"((\w+)\s*\([^)]*\)(?:\s*(?:const|noexcept|override|final|=\s*0|=\s*default|=\s*delete))*\s*(?:\{|;|$))",
        QRegularExpression::MultilineOption
    );
    cpp.classPattern = QRegularExpression(
        R"(^\s*(?:template\s*<[^>]+>\s*)?(?:class|struct)\s+(\w+)(?:\s*:\s*(?:public|private|protected)\s+[\w:,\s]+)?\s*\{)",
        QRegularExpression::MultilineOption
    );
    cpp.methodPattern = QRegularExpression(
        R"(^\s*(?:(?:virtual|static|inline|constexpr)\s+)*[\w:]+(?:<[^>]+>)?(?:\s*[*&]+)?\s+(\w+)::(\w+)\s*\([^)]*\))",
        QRegularExpression::MultilineOption
    );
    cpp.namespacePattern = QRegularExpression(
        R"(^\s*namespace\s+(\w+)\s*\{)",
        QRegularExpression::MultilineOption
    );
    cpp.testPattern = QRegularExpression(
        R"((?:TEST|TEST_F|TEST_P|BOOST_AUTO_TEST_CASE|QTEST_MAIN|Q_SLOT\s+void\s+test)\s*\(\s*(\w+))",
        QRegularExpression::MultilineOption
    );
    m_languagePatterns[QStringLiteral("cpp")] = cpp;
    m_languagePatterns[QStringLiteral("c")] = cpp;
    m_languagePatterns[QStringLiteral("h")] = cpp;
    m_languagePatterns[QStringLiteral("hpp")] = cpp;
    
    // Python patterns
    LanguagePatterns python;
    python.functionPattern = QRegularExpression(
        R"(^\s*(?:async\s+)?def\s+(\w+)\s*\([^)]*\))",
        QRegularExpression::MultilineOption
    );
    python.classPattern = QRegularExpression(
        R"(^\s*class\s+(\w+)(?:\([^)]*\))?\s*:)",
        QRegularExpression::MultilineOption
    );
    python.methodPattern = QRegularExpression(
        R"(^\s+(?:async\s+)?def\s+(\w+)\s*\(self[^)]*\))",
        QRegularExpression::MultilineOption
    );
    python.testPattern = QRegularExpression(
        R"(^\s*(?:async\s+)?def\s+(test_\w+)\s*\([^)]*\)|@pytest\.mark|class\s+Test\w+)",
        QRegularExpression::MultilineOption
    );
    m_languagePatterns[QStringLiteral("py")] = python;
    m_languagePatterns[QStringLiteral("pyw")] = python;
    
    // JavaScript/TypeScript patterns
    LanguagePatterns javascript;
    javascript.functionPattern = QRegularExpression(
        R"((?:^\s*(?:export\s+)?(?:async\s+)?function\s+(\w+)\s*\([^)]*\))|(?:^\s*(?:const|let|var)\s+(\w+)\s*=\s*(?:async\s+)?(?:\([^)]*\)|[^=]+)\s*=>))",
        QRegularExpression::MultilineOption
    );
    javascript.classPattern = QRegularExpression(
        R"(^\s*(?:export\s+)?(?:abstract\s+)?class\s+(\w+)(?:\s+extends\s+[\w.]+)?(?:\s+implements\s+[\w.,\s]+)?\s*\{)",
        QRegularExpression::MultilineOption
    );
    javascript.methodPattern = QRegularExpression(
        R"(^\s*(?:public|private|protected|static|async|get|set)?\s*(\w+)\s*\([^)]*\)\s*(?::\s*[\w<>[\],\s|]+)?\s*\{)",
        QRegularExpression::MultilineOption
    );
    javascript.testPattern = QRegularExpression(
        R"((?:describe|it|test|beforeEach|afterEach|beforeAll|afterAll)\s*\(\s*['"](.*?)['"])",
        QRegularExpression::MultilineOption
    );
    m_languagePatterns[QStringLiteral("js")] = javascript;
    m_languagePatterns[QStringLiteral("jsx")] = javascript;
    m_languagePatterns[QStringLiteral("ts")] = javascript;
    m_languagePatterns[QStringLiteral("tsx")] = javascript;
    
    // Rust patterns
    LanguagePatterns rust;
    rust.functionPattern = QRegularExpression(
        R"(^\s*(?:pub(?:\([^)]+\))?\s+)?(?:async\s+)?fn\s+(\w+)(?:<[^>]+>)?\s*\([^)]*\))",
        QRegularExpression::MultilineOption
    );
    rust.classPattern = QRegularExpression(
        R"(^\s*(?:pub(?:\([^)]+\))?\s+)?(?:struct|enum|trait)\s+(\w+)(?:<[^>]+>)?)",
        QRegularExpression::MultilineOption
    );
    rust.methodPattern = QRegularExpression(
        R"(^\s*(?:pub(?:\([^)]+\))?\s+)?(?:async\s+)?fn\s+(\w+)(?:<[^>]+>)?\s*\(&?(?:mut\s+)?self)",
        QRegularExpression::MultilineOption
    );
    rust.testPattern = QRegularExpression(
        R"(#\[test\]|#\[tokio::test\])",
        QRegularExpression::MultilineOption
    );
    m_languagePatterns[QStringLiteral("rs")] = rust;
    
    // Go patterns
    LanguagePatterns golang;
    golang.functionPattern = QRegularExpression(
        R"(^\s*func\s+(\w+)\s*\([^)]*\))",
        QRegularExpression::MultilineOption
    );
    golang.classPattern = QRegularExpression(
        R"(^\s*type\s+(\w+)\s+(?:struct|interface)\s*\{)",
        QRegularExpression::MultilineOption
    );
    golang.methodPattern = QRegularExpression(
        R"(^\s*func\s+\(\s*\w+\s+\*?(\w+)\s*\)\s+(\w+)\s*\([^)]*\))",
        QRegularExpression::MultilineOption
    );
    golang.testPattern = QRegularExpression(
        R"(^\s*func\s+(Test\w+|Benchmark\w+)\s*\()",
        QRegularExpression::MultilineOption
    );
    m_languagePatterns[QStringLiteral("go")] = golang;
}

QString CodeLensProvider::detectLanguage(const QString& filePath) const
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

QString CodeLensProvider::computeContentHash(const QString& content) const
{
    return QString::fromLatin1(
        QCryptographicHash::hash(content.toUtf8(), QCryptographicHash::Md5).toHex()
    );
}

QList<CodeLensItem> CodeLensProvider::getCodeLenses(const QString& filePath, const QString& content)
{
    const QString language = detectLanguage(filePath);
    const QString contentHash = computeContentHash(content);
    
    // Check cache first
    QList<CodeLensItem> cachedItems;
    if (m_cache->lookup(filePath, contentHash, cachedItems)) {
        qDebug() << "[CodeLensProvider] Cache hit for" << filePath;
        return cachedItems;
    }
    
    qDebug() << "[CodeLensProvider] Computing code lenses for" << filePath << "language:" << language;
    
    QList<CodeLensItem> items;
    
    // Extract symbols first
    QList<SymbolInfo> symbols = extractSymbols(content, language);
    
    // Generate various types of code lenses
    if (m_enableReferenceCounting) {
        items.append(generateReferenceLenses(symbols, content, filePath));
    }
    
    if (m_enableTestDetection) {
        items.append(generateTestLenses(content, language));
    }
    
    if (m_enableGitBlame) {
        items.append(generateGitBlameLenses(filePath, symbols));
    }
    
    if (m_enablePerformanceHints) {
        items.append(generatePerformanceLenses(symbols, content, language));
    }
    
    if (m_enableDocumentationLinks) {
        items.append(generateDocumentationLenses(symbols, content));
    }
    
    // Sort items by line and priority
    std::sort(items.begin(), items.end());
    
    // Merge items on the same line
    items = mergeCodeLenses(items);
    
    // Cache the results
    m_cache->insert(filePath, items, contentHash);
    
    qDebug() << "[CodeLensProvider] Generated" << items.size() << "code lenses for" << filePath;
    
    return items;
}

QList<SymbolInfo> CodeLensProvider::extractSymbols(const QString& content, const QString& language) const
{
    QList<SymbolInfo> symbols;
    
    auto it = m_languagePatterns.find(language);
    if (it == m_languagePatterns.end()) {
        return symbols;
    }
    
    const LanguagePatterns& patterns = *it;
    const QStringList lines = content.split(QLatin1Char('\n'));
    
    // Extract classes/structs
    if (patterns.classPattern.isValid()) {
        QRegularExpressionMatchIterator iter = patterns.classPattern.globalMatch(content);
        while (iter.hasNext()) {
            QRegularExpressionMatch match = iter.next();
            SymbolInfo symbol;
            symbol.name = match.captured(1);
            symbol.type = SymbolType::Class;
            symbol.line = content.left(match.capturedStart()).count(QLatin1Char('\n'));
            symbol.column = match.capturedStart() - content.lastIndexOf(QLatin1Char('\n'), match.capturedStart()) - 1;
            symbol.signature = match.captured(0).trimmed();
            
            // Find end of class (simplified - just looks for matching brace)
            int braceCount = 0;
            bool foundOpen = false;
            for (int i = symbol.line; i < lines.size(); ++i) {
                const QString& line = lines[i];
                for (QChar c : line) {
                    if (c == QLatin1Char('{')) {
                        braceCount++;
                        foundOpen = true;
                    } else if (c == QLatin1Char('}')) {
                        braceCount--;
                        if (foundOpen && braceCount == 0) {
                            symbol.endLine = i;
                            goto foundClassEnd;
                        }
                    }
                }
            }
            foundClassEnd:
            
            if (symbol.isValid()) {
                symbols.append(symbol);
            }
        }
    }
    
    // Extract functions
    if (patterns.functionPattern.isValid()) {
        QRegularExpressionMatchIterator iter = patterns.functionPattern.globalMatch(content);
        while (iter.hasNext()) {
            QRegularExpressionMatch match = iter.next();
            SymbolInfo symbol;
            
            // Try different capture groups (different languages use different groups)
            for (int i = 1; i <= match.lastCapturedIndex(); ++i) {
                if (!match.captured(i).isEmpty()) {
                    symbol.name = match.captured(i);
                    break;
                }
            }
            
            if (symbol.name.isEmpty()) continue;
            
            symbol.type = SymbolType::Function;
            symbol.line = content.left(match.capturedStart()).count(QLatin1Char('\n'));
            symbol.column = match.capturedStart() - content.lastIndexOf(QLatin1Char('\n'), match.capturedStart()) - 1;
            symbol.signature = match.captured(0).trimmed();
            
            // Find end of function
            int braceCount = 0;
            bool foundOpen = false;
            for (int i = symbol.line; i < lines.size(); ++i) {
                const QString& line = lines[i];
                for (QChar c : line) {
                    if (c == QLatin1Char('{')) {
                        braceCount++;
                        foundOpen = true;
                    } else if (c == QLatin1Char('}')) {
                        braceCount--;
                        if (foundOpen && braceCount == 0) {
                            symbol.endLine = i;
                            goto foundFunctionEnd;
                        }
                    }
                }
            }
            foundFunctionEnd:
            
            if (symbol.isValid()) {
                symbols.append(symbol);
            }
        }
    }
    
    // Extract methods
    if (patterns.methodPattern.isValid()) {
        QRegularExpressionMatchIterator iter = patterns.methodPattern.globalMatch(content);
        while (iter.hasNext()) {
            QRegularExpressionMatch match = iter.next();
            SymbolInfo symbol;
            
            // For C++ methods, name is in capture group 2 (after class name)
            // For other languages, it may be in group 1
            for (int i = match.lastCapturedIndex(); i >= 1; --i) {
                if (!match.captured(i).isEmpty()) {
                    symbol.name = match.captured(i);
                    break;
                }
            }
            
            if (symbol.name.isEmpty()) continue;
            
            symbol.type = SymbolType::Method;
            symbol.line = content.left(match.capturedStart()).count(QLatin1Char('\n'));
            symbol.column = match.capturedStart() - content.lastIndexOf(QLatin1Char('\n'), match.capturedStart()) - 1;
            symbol.signature = match.captured(0).trimmed();
            
            if (symbol.isValid()) {
                symbols.append(symbol);
            }
        }
    }
    
    // Extract namespaces (C++/C# style)
    if (patterns.namespacePattern.isValid()) {
        QRegularExpressionMatchIterator iter = patterns.namespacePattern.globalMatch(content);
        while (iter.hasNext()) {
            QRegularExpressionMatch match = iter.next();
            SymbolInfo symbol;
            symbol.name = match.captured(1);
            symbol.type = SymbolType::Namespace;
            symbol.line = content.left(match.capturedStart()).count(QLatin1Char('\n'));
            symbol.column = match.capturedStart() - content.lastIndexOf(QLatin1Char('\n'), match.capturedStart()) - 1;
            
            if (symbol.isValid()) {
                symbols.append(symbol);
            }
        }
    }
    
    return symbols;
}

QList<CodeLensItem> CodeLensProvider::generateReferenceLenses(
    const QList<SymbolInfo>& symbols,
    const QString& content,
    const QString& filePath) const
{
    QList<CodeLensItem> items;
    
    for (const SymbolInfo& symbol : symbols) {
        // Skip namespaces for reference counting
        if (symbol.type == SymbolType::Namespace) continue;
        
        // Count references to this symbol in the file
        int referenceCount = countReferences(symbol.name, content);
        
        // Subtract 1 for the definition itself
        referenceCount = qMax(0, referenceCount - 1);
        
        if (referenceCount >= m_minReferencesForDisplay) {
            CodeLensItem item(symbol.line, symbol.column, QString(), CodeLensType::Reference);
            
            if (referenceCount == 0) {
                item.setText(QStringLiteral("0 references"));
            } else if (referenceCount == 1) {
                item.setText(QStringLiteral("1 reference"));
            } else {
                item.setText(QStringLiteral("%1 references").arg(referenceCount));
            }
            
            item.setReferenceCount(referenceCount);
            item.setSymbolName(symbol.name);
            item.setCommand(QStringLiteral("rawrxd.findReferences"));
            item.setCommandArgs({filePath, QString::number(symbol.line), symbol.name});
            
            items.append(item);
        }
        
        // For classes and interfaces, also show implementations
        if (symbol.type == SymbolType::Class || symbol.type == SymbolType::Interface) {
            int implCount = countImplementations(symbol.name, content);
            if (implCount > 0) {
                CodeLensItem implItem(symbol.line, symbol.column + 1, QString(), CodeLensType::Implementation);
                implItem.setText(implCount == 1 
                    ? QStringLiteral("1 implementation") 
                    : QStringLiteral("%1 implementations").arg(implCount));
                implItem.setSymbolName(symbol.name);
                implItem.setCommand(QStringLiteral("rawrxd.findImplementations"));
                implItem.setCommandArgs({filePath, QString::number(symbol.line), symbol.name});
                implItem.setPriority(5);
                
                items.append(implItem);
            }
        }
    }
    
    return items;
}

int CodeLensProvider::countReferences(const QString& symbolName, const QString& content) const
{
    // Create a pattern that matches the symbol as a word boundary
    QRegularExpression pattern(QStringLiteral("\\b%1\\b").arg(QRegularExpression::escape(symbolName)));
    
    int count = 0;
    QRegularExpressionMatchIterator iter = pattern.globalMatch(content);
    while (iter.hasNext()) {
        iter.next();
        count++;
    }
    
    return count;
}

int CodeLensProvider::countImplementations(const QString& symbolName, const QString& content) const
{
    // Look for inheritance patterns
    QRegularExpression pattern(
        QStringLiteral("(?:extends|implements|:\\s*(?:public|private|protected)\\s+)\\s*%1\\b")
            .arg(QRegularExpression::escape(symbolName))
    );
    
    int count = 0;
    QRegularExpressionMatchIterator iter = pattern.globalMatch(content);
    while (iter.hasNext()) {
        iter.next();
        count++;
    }
    
    return count;
}

QList<CodeLensItem> CodeLensProvider::generateTestLenses(const QString& content, const QString& language) const
{
    QList<CodeLensItem> items;
    
    auto it = m_languagePatterns.find(language);
    if (it == m_languagePatterns.end() || !it->testPattern.isValid()) {
        return items;
    }
    
    const LanguagePatterns& patterns = *it;
    
    QRegularExpressionMatchIterator iter = patterns.testPattern.globalMatch(content);
    while (iter.hasNext()) {
        QRegularExpressionMatch match = iter.next();
        
        int line = content.left(match.capturedStart()).count(QLatin1Char('\n'));
        
        QString testName;
        for (int i = 1; i <= match.lastCapturedIndex(); ++i) {
            if (!match.captured(i).isEmpty()) {
                testName = match.captured(i);
                break;
            }
        }
        
        if (testName.isEmpty()) {
            testName = QStringLiteral("Test");
        }
        
        CodeLensItem runItem(line, 0, QStringLiteral("▶ Run Test"), CodeLensType::Test);
        runItem.setSymbolName(testName);
        runItem.setCommand(QStringLiteral("rawrxd.runTest"));
        runItem.setCommandArgs({testName});
        runItem.setPriority(10);
        items.append(runItem);
        
        CodeLensItem debugItem(line, 1, QStringLiteral("🔍 Debug Test"), CodeLensType::Debug);
        debugItem.setSymbolName(testName);
        debugItem.setCommand(QStringLiteral("rawrxd.debugTest"));
        debugItem.setCommandArgs({testName});
        debugItem.setPriority(9);
        items.append(debugItem);
    }
    
    return items;
}

QList<CodeLensItem> CodeLensProvider::generateGitBlameLenses(
    const QString& filePath,
    const QList<SymbolInfo>& symbols) const
{
    QList<CodeLensItem> items;
    
    // Run git blame for the file
    QProcess gitProcess;
    gitProcess.setWorkingDirectory(QFileInfo(filePath).absolutePath());
    gitProcess.start(QStringLiteral("git"), {
        QStringLiteral("blame"),
        QStringLiteral("--line-porcelain"),
        filePath
    });
    
    if (!gitProcess.waitForFinished(5000)) {
        qDebug() << "[CodeLensProvider] Git blame timeout for" << filePath;
        return items;
    }
    
    if (gitProcess.exitCode() != 0) {
        qDebug() << "[CodeLensProvider] Git blame failed for" << filePath;
        return items;
    }
    
    // Parse git blame output
    QMap<int, GitBlameInfo> blameInfo;
    QString output = QString::fromUtf8(gitProcess.readAllStandardOutput());
    QStringList lines = output.split(QLatin1Char('\n'));
    
    int currentLine = -1;
    GitBlameInfo currentBlame;
    
    for (const QString& line : lines) {
        if (line.isEmpty()) continue;
        
        // SHA line (40 char hash followed by line numbers)
        if (line.length() >= 40 && line[0].isLetterOrNumber()) {
            QStringList parts = line.split(QLatin1Char(' '));
            if (parts.size() >= 2) {
                currentBlame.commitHash = parts[0].left(8);
                currentLine = parts[1].toInt() - 1; // Convert to 0-based
            }
        } else if (line.startsWith(QStringLiteral("author "))) {
            currentBlame.author = line.mid(7);
        } else if (line.startsWith(QStringLiteral("author-time "))) {
            qint64 timestamp = line.mid(12).toLongLong();
            currentBlame.date = QDateTime::fromSecsSinceEpoch(timestamp);
        } else if (line.startsWith(QStringLiteral("summary "))) {
            currentBlame.summary = line.mid(8);
            if (currentLine >= 0) {
                blameInfo[currentLine] = currentBlame;
            }
        }
    }
    
    // Generate blame lenses for symbols
    for (const SymbolInfo& symbol : symbols) {
        if (blameInfo.contains(symbol.line)) {
            const GitBlameInfo& blame = blameInfo[symbol.line];
            
            QString displayText;
            if (blame.date.isValid()) {
                qint64 daysSince = blame.date.daysTo(QDateTime::currentDateTime());
                QString timeAgo;
                
                if (daysSince == 0) {
                    timeAgo = QStringLiteral("today");
                } else if (daysSince == 1) {
                    timeAgo = QStringLiteral("yesterday");
                } else if (daysSince < 7) {
                    timeAgo = QStringLiteral("%1 days ago").arg(daysSince);
                } else if (daysSince < 30) {
                    timeAgo = QStringLiteral("%1 weeks ago").arg(daysSince / 7);
                } else if (daysSince < 365) {
                    timeAgo = QStringLiteral("%1 months ago").arg(daysSince / 30);
                } else {
                    timeAgo = QStringLiteral("%1 years ago").arg(daysSince / 365);
                }
                
                displayText = QStringLiteral("%1, %2").arg(blame.author, timeAgo);
            } else {
                displayText = blame.author;
            }
            
            CodeLensItem item(symbol.line, 100, displayText, CodeLensType::GitBlame);
            item.setSymbolName(symbol.name);
            item.setCommand(QStringLiteral("rawrxd.showCommit"));
            item.setCommandArgs({blame.commitHash});
            item.setTooltip(QStringLiteral("%1\n%2").arg(blame.commitHash, blame.summary));
            item.setPriority(1);
            
            items.append(item);
        }
    }
    
    return items;
}

QList<CodeLensItem> CodeLensProvider::generatePerformanceLenses(
    const QList<SymbolInfo>& symbols,
    const QString& content,
    const QString& language) const
{
    QList<CodeLensItem> items;
    
    for (const SymbolInfo& symbol : symbols) {
        if (symbol.type != SymbolType::Function && symbol.type != SymbolType::Method) {
            continue;
        }
        
        // Calculate complexity metrics
        int complexity = calculateCyclomaticComplexity(symbol, content);
        int lineCount = symbol.endLine - symbol.line + 1;
        
        // Only show hints for complex functions
        if (complexity > 10 || lineCount > 50) {
            QString hintText;
            
            if (complexity > 10) {
                hintText = QStringLiteral("⚠ Complexity: %1").arg(complexity);
                if (lineCount > 50) {
                    hintText += QStringLiteral(" | Lines: %1").arg(lineCount);
                }
            } else {
                hintText = QStringLiteral("Lines: %1").arg(lineCount);
            }
            
            CodeLensItem item(symbol.line, 50, hintText, CodeLensType::Performance);
            item.setSymbolName(symbol.name);
            item.setCommand(QStringLiteral("rawrxd.showComplexityReport"));
            item.setCommandArgs({symbol.name, QString::number(complexity), QString::number(lineCount)});
            item.setPriority(2);
            
            if (complexity > 15) {
                item.setTooltip(QStringLiteral("High cyclomatic complexity. Consider refactoring."));
            }
            
            items.append(item);
        }
    }
    
    return items;
}

int CodeLensProvider::calculateCyclomaticComplexity(const SymbolInfo& symbol, const QString& content) const
{
    if (symbol.endLine <= symbol.line) {
        return 1;
    }
    
    // Extract the function body
    QStringList lines = content.split(QLatin1Char('\n'));
    QString functionBody;
    for (int i = symbol.line; i <= symbol.endLine && i < lines.size(); ++i) {
        functionBody += lines[i] + QLatin1Char('\n');
    }
    
    // Count decision points (simplified cyclomatic complexity)
    int complexity = 1; // Base complexity
    
    // Control flow keywords
    static const QStringList controlFlowKeywords = {
        QStringLiteral("if"),
        QStringLiteral("else if"),
        QStringLiteral("elif"),
        QStringLiteral("for"),
        QStringLiteral("while"),
        QStringLiteral("case"),
        QStringLiteral("catch"),
        QStringLiteral("&&"),
        QStringLiteral("||"),
        QStringLiteral("?"),
        QStringLiteral("and"),
        QStringLiteral("or")
    };
    
    for (const QString& keyword : controlFlowKeywords) {
        QRegularExpression pattern(QStringLiteral("\\b%1\\b").arg(QRegularExpression::escape(keyword)));
        QRegularExpressionMatchIterator iter = pattern.globalMatch(functionBody);
        while (iter.hasNext()) {
            iter.next();
            complexity++;
        }
    }
    
    return complexity;
}

QList<CodeLensItem> CodeLensProvider::generateDocumentationLenses(
    const QList<SymbolInfo>& symbols,
    const QString& content) const
{
    QList<CodeLensItem> items;
    QStringList lines = content.split(QLatin1Char('\n'));
    
    for (const SymbolInfo& symbol : symbols) {
        // Check if there's documentation above this symbol
        bool hasDocumentation = false;
        
        if (symbol.line > 0) {
            // Look for doc comments (/** */, ///, #, """)
            for (int i = symbol.line - 1; i >= 0 && i >= symbol.line - 5; --i) {
                const QString& line = lines[i].trimmed();
                if (line.isEmpty()) continue;
                
                if (line.startsWith(QStringLiteral("/**")) ||
                    line.startsWith(QStringLiteral("///")) ||
                    line.startsWith(QStringLiteral("//!")) ||
                    line.startsWith(QStringLiteral("\"\"\"")) ||
                    line.startsWith(QStringLiteral("'''")) ||
                    line.contains(QStringLiteral("@brief")) ||
                    line.contains(QStringLiteral(":param")) ||
                    line.contains(QStringLiteral("@param"))) {
                    hasDocumentation = true;
                    break;
                }
                
                // Stop if we hit code
                if (!line.startsWith(QStringLiteral("*")) &&
                    !line.startsWith(QStringLiteral("//")) &&
                    !line.startsWith(QStringLiteral("#"))) {
                    break;
                }
            }
        }
        
        if (!hasDocumentation && 
            (symbol.type == SymbolType::Function || 
             symbol.type == SymbolType::Method || 
             symbol.type == SymbolType::Class)) {
            CodeLensItem item(symbol.line, 200, QStringLiteral("📝 Add Documentation"), CodeLensType::Documentation);
            item.setSymbolName(symbol.name);
            item.setCommand(QStringLiteral("rawrxd.generateDocumentation"));
            item.setCommandArgs({symbol.name, symbol.signature});
            item.setPriority(0);
            item.setTooltip(QStringLiteral("Generate documentation for %1").arg(symbol.name));
            
            items.append(item);
        }
    }
    
    return items;
}

QList<CodeLensItem> CodeLensProvider::mergeCodeLenses(const QList<CodeLensItem>& items) const
{
    if (items.isEmpty()) {
        return items;
    }
    
    QList<CodeLensItem> merged;
    int currentLine = -1;
    QList<CodeLensItem> lineItems;
    
    for (const CodeLensItem& item : items) {
        if (item.line() != currentLine) {
            if (!lineItems.isEmpty()) {
                // Sort items on this line by priority
                std::sort(lineItems.begin(), lineItems.end(), 
                    [](const CodeLensItem& a, const CodeLensItem& b) {
                        return a.priority() > b.priority();
                    });
                merged.append(lineItems);
            }
            lineItems.clear();
            currentLine = item.line();
        }
        lineItems.append(item);
    }
    
    // Don't forget the last line
    if (!lineItems.isEmpty()) {
        std::sort(lineItems.begin(), lineItems.end(),
            [](const CodeLensItem& a, const CodeLensItem& b) {
                return a.priority() > b.priority();
            });
        merged.append(lineItems);
    }
    
    return merged;
}

void CodeLensProvider::getCodeLensesAsync(
    const QString& filePath,
    const QString& content,
    std::function<void(const QList<CodeLensItem>&)> callback)
{
    if (!m_asyncEnabled) {
        // Fall back to synchronous
        callback(getCodeLenses(filePath, content));
        return;
    }
    
    QFuture<QList<CodeLensItem>> future = QtConcurrent::run([this, filePath, content]() {
        return getCodeLenses(filePath, content);
    });
    
    QFutureWatcher<QList<CodeLensItem>>* watcher = new QFutureWatcher<QList<CodeLensItem>>(this);
    connect(watcher, &QFutureWatcher<QList<CodeLensItem>>::finished, this, [watcher, callback]() {
        callback(watcher->result());
        watcher->deleteLater();
    });
    
    watcher->setFuture(future);
}

void CodeLensProvider::invalidateFile(const QString& filePath)
{
    m_cache->invalidate(filePath);
    qDebug() << "[CodeLensProvider] Invalidated cache for" << filePath;
}

void CodeLensProvider::clearCache()
{
    m_cache->clear();
    qDebug() << "[CodeLensProvider] Cache cleared";
}

void CodeLensProvider::setEnabled(CodeLensType type, bool enabled)
{
    switch (type) {
        case CodeLensType::Reference:
            m_enableReferenceCounting = enabled;
            break;
        case CodeLensType::Test:
            m_enableTestDetection = enabled;
            break;
        case CodeLensType::GitBlame:
            m_enableGitBlame = enabled;
            break;
        case CodeLensType::Performance:
            m_enablePerformanceHints = enabled;
            break;
        case CodeLensType::Documentation:
            m_enableDocumentationLinks = enabled;
            break;
        default:
            break;
    }
    
    // Clear cache when settings change
    m_cache->clear();
}

bool CodeLensProvider::isEnabled(CodeLensType type) const
{
    switch (type) {
        case CodeLensType::Reference:
            return m_enableReferenceCounting;
        case CodeLensType::Test:
            return m_enableTestDetection;
        case CodeLensType::GitBlame:
            return m_enableGitBlame;
        case CodeLensType::Performance:
            return m_enablePerformanceHints;
        case CodeLensType::Documentation:
            return m_enableDocumentationLinks;
        default:
            return true;
    }
}

void CodeLensProvider::setMinReferencesForDisplay(int count)
{
    m_minReferencesForDisplay = qMax(0, count);
}

int CodeLensProvider::minReferencesForDisplay() const
{
    return m_minReferencesForDisplay;
}

void CodeLensProvider::registerCustomLensProvider(
    const QString& name,
    std::function<QList<CodeLensItem>(const QString&, const QString&)> provider)
{
    m_customProviders[name] = provider;
    qDebug() << "[CodeLensProvider] Registered custom provider:" << name;
}

void CodeLensProvider::unregisterCustomLensProvider(const QString& name)
{
    m_customProviders.remove(name);
    qDebug() << "[CodeLensProvider] Unregistered custom provider:" << name;
}

} // namespace RawrXD
