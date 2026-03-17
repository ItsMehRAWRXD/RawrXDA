#include "WorkspaceNavigator.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDir>
#include <QDebug>
#include <QUuid>
#include <algorithm>

QJsonObject WorkspaceNavigator::SymbolInfo::toJson() const {
    QJsonObject obj;
    obj["name"] = name;
    obj["kind"] = static_cast<int>(kind);
    obj["filePath"] = filePath;
    obj["line"] = line;
    obj["column"] = column;
    obj["signature"] = signature;
    obj["parentContext"] = parentContext;
    return obj;
}

WorkspaceNavigator::SymbolInfo WorkspaceNavigator::SymbolInfo::fromJson(const QJsonObject& obj) {
    SymbolInfo info;
    info.name = obj["name"].toString();
    info.kind = static_cast<SymbolKind>(obj["kind"].toInt());
    info.filePath = obj["filePath"].toString();
    info.line = obj["line"].toInt();
    info.column = obj["column"].toInt();
    info.signature = obj["signature"].toString();
    info.parentContext = obj["parentContext"].toString();
    return info;
}

QJsonObject WorkspaceNavigator::FileHistoryEntry::toJson() const {
    QJsonObject obj;
    obj["filePath"] = filePath;
    obj["timestamp"] = timestamp.toString(Qt::ISODate);
    obj["lineNumber"] = lineNumber;
    obj["accessCount"] = accessCount;
    obj["context"] = context;
    return obj;
}

QJsonObject WorkspaceNavigator::BreadcrumbNode::toJson() const {
    QJsonObject obj;
    obj["label"] = label;
    obj["filePath"] = filePath;
    obj["line"] = line;
    obj["kind"] = static_cast<int>(kind);
    QJsonArray childrenArr;
    for (const BreadcrumbNode& child : children) {
        childrenArr.append(child.toJson());
    }
    obj["children"] = childrenArr;
    return obj;
}

QJsonObject WorkspaceNavigator::Bookmark::toJson() const {
    QJsonObject obj;
    obj["id"] = id;
    obj["label"] = label;
    obj["filePath"] = filePath;
    obj["line"] = line;
    obj["column"] = column;
    obj["category"] = category;
    obj["createdAt"] = createdAt.toString(Qt::ISODate);
    obj["notes"] = notes;
    return obj;
}

QJsonObject WorkspaceNavigator::NavigationResult::toJson() const {
    QJsonObject obj;
    obj["type"] = static_cast<int>(type);
    QJsonArray symbolsArr;
    for (const SymbolInfo& sym : symbols) symbolsArr.append(sym.toJson());
    obj["symbols"] = symbolsArr;
    obj["totalResults"] = totalResults;
    obj["searchTime"] = searchTime;
    return obj;
}

WorkspaceNavigator::WorkspaceNavigator(QObject* parent) 
    : QObject(parent), m_indexing(false) {}

WorkspaceNavigator::~WorkspaceNavigator() {}

QList<WorkspaceNavigator::SymbolInfo> WorkspaceNavigator::searchSymbols(const QString& query, SymbolKind kind) {
    QMutexLocker locker(&m_mutex);
    QList<SymbolInfo> results;
    
    for (auto it = m_symbolIndex.begin(); it != m_symbolIndex.end(); ++it) {
        for (const SymbolInfo& sym : it.value()) {
            if ((kind == SymbolKind::Unknown || sym.kind == kind) &&
                sym.name.contains(query, Qt::CaseInsensitive)) {
                results.append(sym);
                emit symbolFound(sym);
            }
        }
    }
    
    return results;
}

QList<WorkspaceNavigator::SymbolInfo> WorkspaceNavigator::findSymbolsByName(const QString& name, bool caseSensitive) {
    QMutexLocker locker(&m_mutex);
    QList<SymbolInfo> results;
    Qt::CaseSensitivity cs = caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
    
    for (auto it = m_symbolIndex.begin(); it != m_symbolIndex.end(); ++it) {
        for (const SymbolInfo& sym : it.value()) {
            if (sym.name.compare(name, cs) == 0) {
                results.append(sym);
            }
        }
    }
    
    return results;
}

QList<WorkspaceNavigator::SymbolInfo> WorkspaceNavigator::getSymbolsInFile(const QString& filePath) {
    QMutexLocker locker(&m_mutex);
    return m_symbolIndex.value(filePath);
}

QList<WorkspaceNavigator::SymbolInfo> WorkspaceNavigator::getSymbolsByKind(SymbolKind kind) {
    QMutexLocker locker(&m_mutex);
    QList<SymbolInfo> results;
    
    for (auto it = m_symbolIndex.begin(); it != m_symbolIndex.end(); ++it) {
        for (const SymbolInfo& sym : it.value()) {
            if (sym.kind == kind) {
                results.append(sym);
            }
        }
    }
    
    return results;
}

WorkspaceNavigator::SymbolInfo WorkspaceNavigator::getSymbolAtPosition(const QString& filePath, int line, int column) {
    QMutexLocker locker(&m_mutex);
    
    QList<SymbolInfo> symbols = m_symbolIndex.value(filePath);
    for (const SymbolInfo& sym : symbols) {
        if (sym.line == line) {
            return sym;
        }
    }
    
    return SymbolInfo();
}

QList<WorkspaceNavigator::SymbolInfo> WorkspaceNavigator::fuzzySearchSymbols(const QString& query, int maxResults) {
    QMutexLocker locker(&m_mutex);
    QList<QPair<double, SymbolInfo>> scored;
    
    for (auto it = m_symbolIndex.begin(); it != m_symbolIndex.end(); ++it) {
        for (const SymbolInfo& sym : it.value()) {
            double similarity = calculateSimilarity(query, sym.name);
            if (similarity > 0.3) {
                scored.append(qMakePair(similarity, sym));
            }
        }
    }
    
    std::sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) {
        return a.first > b.first;
    });
    
    QList<SymbolInfo> results;
    for (int i = 0; i < std::min(static_cast<int>(maxResults), static_cast<int>(scored.size())); ++i) {
        results.append(scored[i].second);
    }
    
    return results;
}

QStringList WorkspaceNavigator::fuzzySearchFiles(const QString& query, int maxResults) {
    QMutexLocker locker(&m_mutex);
    QStringList allFiles = m_symbolIndex.keys();
    
    QList<QPair<double, QString>> ranked = rankByRelevance(allFiles, query);
    
    QStringList results;
    for (int i = 0; i < std::min(static_cast<int>(maxResults), static_cast<int>(ranked.size())); ++i) {
        results.append(ranked[i].second);
    }
    
    return results;
}

double WorkspaceNavigator::calculateSimilarity(const QString& str1, const QString& str2) {
    if (str1.isEmpty() || str2.isEmpty()) return 0.0;
    
    int distance = levenshteinDistance(str1.toLower(), str2.toLower());
    int maxLen = std::max(str1.length(), str2.length());
    
    return 1.0 - (static_cast<double>(distance) / maxLen);
}

void WorkspaceNavigator::recordFileAccess(const QString& filePath, int lineNumber) {
    QMutexLocker locker(&m_mutex);
    
    if (m_fileHistory.contains(filePath)) {
        FileHistoryEntry& entry = m_fileHistory[filePath];
        entry.timestamp = QDateTime::currentDateTime();
        entry.lineNumber = lineNumber;
        entry.accessCount++;
    } else {
        FileHistoryEntry entry;
        entry.filePath = filePath;
        entry.timestamp = QDateTime::currentDateTime();
        entry.lineNumber = lineNumber;
        entry.accessCount = 1;
        m_fileHistory[filePath] = entry;
    }
    
    emit fileHistoryUpdated(filePath);
}

QList<WorkspaceNavigator::FileHistoryEntry> WorkspaceNavigator::getRecentFiles(int count) {
    QMutexLocker locker(&m_mutex);
    
    QList<FileHistoryEntry> entries = m_fileHistory.values();
    std::sort(entries.begin(), entries.end(), [](const FileHistoryEntry& a, const FileHistoryEntry& b) {
        return a.timestamp > b.timestamp;
    });
    
    return entries.mid(0, std::min(static_cast<int>(count), static_cast<int>(entries.size())));
}

QList<WorkspaceNavigator::FileHistoryEntry> WorkspaceNavigator::getFileHistory(const QString& filePath) {
    QMutexLocker locker(&m_mutex);
    QList<FileHistoryEntry> history;
    
    if (m_fileHistory.contains(filePath)) {
        history.append(m_fileHistory[filePath]);
    }
    
    return history;
}

void WorkspaceNavigator::clearFileHistory() {
    QMutexLocker locker(&m_mutex);
    m_fileHistory.clear();
}

WorkspaceNavigator::BreadcrumbNode WorkspaceNavigator::buildBreadcrumb(const QString& filePath, int line) {
    BreadcrumbNode root;
    root.label = "Root";
    root.filePath = filePath;
    root.line = 0;
    root.kind = SymbolKind::Unknown;
    
    QList<SymbolInfo> symbols = getSymbolsInFile(filePath);
    for (const SymbolInfo& sym : symbols) {
        if (sym.line <= line) {
            BreadcrumbNode node;
            node.label = sym.name;
            node.filePath = sym.filePath;
            node.line = sym.line;
            node.kind = sym.kind;
            root.children.append(node);
        }
    }
    
    return root;
}

QStringList WorkspaceNavigator::getBreadcrumbPath(const QString& filePath, int line) {
    QStringList path;
    BreadcrumbNode breadcrumb = buildBreadcrumb(filePath, line);
    
    path.append(breadcrumb.label);
    for (const BreadcrumbNode& child : breadcrumb.children) {
        path.append(child.label);
    }
    
    emit breadcrumbChanged(path);
    return path;
}

QList<WorkspaceNavigator::BreadcrumbNode> WorkspaceNavigator::getContextHierarchy(const QString& filePath, int line) {
    QList<BreadcrumbNode> hierarchy;
    QList<SymbolInfo> symbols = getSymbolsInFile(filePath);
    
    for (const SymbolInfo& sym : symbols) {
        if (sym.line <= line) {
            BreadcrumbNode node;
            node.label = sym.name;
            node.filePath = sym.filePath;
            node.line = sym.line;
            node.kind = sym.kind;
            hierarchy.append(node);
        }
    }
    
    return hierarchy;
}

QStringList WorkspaceNavigator::quickOpenFile(const QString& query) {
    return fuzzySearchFiles(query, 50);
}

QList<WorkspaceNavigator::SymbolInfo> WorkspaceNavigator::quickOpenSymbol(const QString& query) {
    return fuzzySearchSymbols(query, 50);
}

QList<WorkspaceNavigator::Bookmark> WorkspaceNavigator::quickOpenBookmark(const QString& query) {
    QMutexLocker locker(&m_mutex);
    QList<Bookmark> results;
    
    for (const Bookmark& bm : m_bookmarks) {
        if (bm.label.contains(query, Qt::CaseInsensitive) ||
            bm.filePath.contains(query, Qt::CaseInsensitive)) {
            results.append(bm);
        }
    }
    
    return results;
}

QString WorkspaceNavigator::addBookmark(const QString& filePath, int line, const QString& label, const QString& category) {
    QMutexLocker locker(&m_mutex);
    
    Bookmark bm;
    bm.id = generateBookmarkId();
    bm.label = label;
    bm.filePath = filePath;
    bm.line = line;
    bm.column = 0;
    bm.category = category;
    bm.createdAt = QDateTime::currentDateTime();
    
    m_bookmarks[bm.id] = bm;
    
    emit bookmarkAdded(bm.id);
    return bm.id;
}

bool WorkspaceNavigator::removeBookmark(const QString& id) {
    QMutexLocker locker(&m_mutex);
    
    if (m_bookmarks.remove(id) > 0) {
        emit bookmarkRemoved(id);
        return true;
    }
    
    return false;
}

bool WorkspaceNavigator::updateBookmark(const QString& id, const QString& label, const QString& notes) {
    QMutexLocker locker(&m_mutex);
    
    if (m_bookmarks.contains(id)) {
        m_bookmarks[id].label = label;
        m_bookmarks[id].notes = notes;
        return true;
    }
    
    return false;
}

QList<WorkspaceNavigator::Bookmark> WorkspaceNavigator::getBookmarks(const QString& category) {
    QMutexLocker locker(&m_mutex);
    
    if (category.isEmpty()) {
        return m_bookmarks.values();
    }
    
    QList<Bookmark> results;
    for (const Bookmark& bm : m_bookmarks) {
        if (bm.category == category) {
            results.append(bm);
        }
    }
    
    return results;
}

QList<WorkspaceNavigator::Bookmark> WorkspaceNavigator::getBookmarksInFile(const QString& filePath) {
    QMutexLocker locker(&m_mutex);
    
    QList<Bookmark> results;
    for (const Bookmark& bm : m_bookmarks) {
        if (bm.filePath == filePath) {
            results.append(bm);
        }
    }
    
    return results;
}

QStringList WorkspaceNavigator::getBookmarkCategories() {
    QMutexLocker locker(&m_mutex);
    
    QSet<QString> categories;
    for (const Bookmark& bm : m_bookmarks) {
        categories.insert(bm.category);
    }
    
    return categories.values();
}

void WorkspaceNavigator::pushContext(const QString& filePath, int line) {
    QMutexLocker locker(&m_mutex);
    m_contextStack.append(qMakePair(filePath, line));
    emit contextChanged(filePath, line);
}

QPair<QString, int> WorkspaceNavigator::popContext() {
    QMutexLocker locker(&m_mutex);
    
    if (m_contextStack.isEmpty()) {
        return qMakePair(QString(), 0);
    }
    
    return m_contextStack.takeLast();
}

QPair<QString, int> WorkspaceNavigator::peekContext() {
    QMutexLocker locker(&m_mutex);
    
    if (m_contextStack.isEmpty()) {
        return qMakePair(QString(), 0);
    }
    
    return m_contextStack.last();
}

void WorkspaceNavigator::clearContext() {
    QMutexLocker locker(&m_mutex);
    m_contextStack.clear();
}

int WorkspaceNavigator::getContextDepth() const {
    QMutexLocker locker(&m_mutex);
    return m_contextStack.size();
}

void WorkspaceNavigator::indexWorkspace(const QString& rootPath) {
    QMutexLocker locker(&m_mutex);
    m_workspaceRoot = rootPath;
    m_indexing = true;
    
    QDir dir(rootPath);
    QStringList files = dir.entryList(QStringList() << "*.cpp" << "*.h" << "*.c" << "*.hpp", QDir::Files, QDir::NoSort);
    
    emit indexingStarted(files.size());
    
    int processed = 0;
    for (const QString& file : files) {
        QString fullPath = dir.absoluteFilePath(file);
        indexFile(fullPath);
        processed++;
        emit indexingProgress(processed, files.size());
    }
    
    m_indexing = false;
    emit indexingCompleted(getTotalSymbols());
}

void WorkspaceNavigator::indexFile(const QString& filePath) {
    QMutexLocker locker(&m_mutex);
    parseSymbolsInFile(filePath);
}

void WorkspaceNavigator::clearIndex() {
    QMutexLocker locker(&m_mutex);
    m_symbolIndex.clear();
}

bool WorkspaceNavigator::isIndexing() const {
    QMutexLocker locker(&m_mutex);
    return m_indexing;
}

int WorkspaceNavigator::getIndexedFileCount() const {
    QMutexLocker locker(&m_mutex);
    return m_symbolIndex.size();
}

int WorkspaceNavigator::getTotalSymbols() const {
    QMutexLocker locker(&m_mutex);
    
    int total = 0;
    for (auto it = m_symbolIndex.begin(); it != m_symbolIndex.end(); ++it) {
        total += it.value().size();
    }
    
    return total;
}

int WorkspaceNavigator::getTotalFiles() const {
    QMutexLocker locker(&m_mutex);
    return m_symbolIndex.size();
}

QMap<QString, int> WorkspaceNavigator::getFileAccessStats() {
    QMutexLocker locker(&m_mutex);
    
    QMap<QString, int> stats;
    for (auto it = m_fileHistory.begin(); it != m_fileHistory.end(); ++it) {
        stats[it.key()] = it.value().accessCount;
    }
    
    return stats;
}

QMap<WorkspaceNavigator::SymbolKind, int> WorkspaceNavigator::getSymbolDistribution() {
    QMutexLocker locker(&m_mutex);
    
    QMap<SymbolKind, int> dist;
    for (auto it = m_symbolIndex.begin(); it != m_symbolIndex.end(); ++it) {
        for (const SymbolInfo& sym : it.value()) {
            dist[sym.kind]++;
        }
    }
    
    return dist;
}

void WorkspaceNavigator::parseSymbolsInFile(const QString& filePath) {
    QString code = readFile(filePath);
    if (code.isEmpty()) return;
    
    QList<SymbolInfo> symbols;
    symbols.append(parseFunctions(code, filePath));
    symbols.append(parseClasses(code, filePath));
    symbols.append(parseVariables(code, filePath));
    
    m_symbolIndex[filePath] = symbols;
}

QList<WorkspaceNavigator::SymbolInfo> WorkspaceNavigator::parseFunctions(const QString& code, const QString& filePath) {
    QList<SymbolInfo> functions;
    
    QRegularExpression funcRe(R"((\w+)\s+(\w+)\s*\([^)]*\)\s*\{)");
    QRegularExpressionMatchIterator it = funcRe.globalMatch(code);
    
    int lineOffset = 0;
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        SymbolInfo info;
        info.name = match.captured(2);
        info.kind = SymbolKind::Function;
        info.filePath = filePath;
        info.line = code.left(match.capturedStart()).count('\n') + 1;
        info.signature = match.captured(0);
        functions.append(info);
    }
    
    return functions;
}

QList<WorkspaceNavigator::SymbolInfo> WorkspaceNavigator::parseClasses(const QString& code, const QString& filePath) {
    QList<SymbolInfo> classes;
    
    QRegularExpression classRe(R"(class\s+(\w+))");
    QRegularExpressionMatchIterator it = classRe.globalMatch(code);
    
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        SymbolInfo info;
        info.name = match.captured(1);
        info.kind = SymbolKind::Class;
        info.filePath = filePath;
        info.line = code.left(match.capturedStart()).count('\n') + 1;
        classes.append(info);
    }
    
    return classes;
}

QList<WorkspaceNavigator::SymbolInfo> WorkspaceNavigator::parseVariables(const QString& code, const QString& filePath) {
    QList<SymbolInfo> variables;
    // Simplified variable parsing
    return variables;
}

int WorkspaceNavigator::levenshteinDistance(const QString& s1, const QString& s2) {
    int len1 = s1.length();
    int len2 = s2.length();
    QVector<QVector<int>> dp(len1 + 1, QVector<int>(len2 + 1));
    
    for (int i = 0; i <= len1; ++i) dp[i][0] = i;
    for (int j = 0; j <= len2; ++j) dp[0][j] = j;
    
    for (int i = 1; i <= len1; ++i) {
        for (int j = 1; j <= len2; ++j) {
            int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            dp[i][j] = std::min({dp[i - 1][j] + 1, dp[i][j - 1] + 1, dp[i - 1][j - 1] + cost});
        }
    }
    
    return dp[len1][len2];
}

QList<QPair<double, QString>> WorkspaceNavigator::rankByRelevance(const QStringList& items, const QString& query) {
    QList<QPair<double, QString>> ranked;
    
    for (const QString& item : items) {
        double similarity = calculateSimilarity(query, item);
        if (similarity > 0.0) {
            ranked.append(qMakePair(similarity, item));
        }
    }
    
    std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) {
        return a.first > b.first;
    });
    
    return ranked;
}

QString WorkspaceNavigator::readFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    QTextStream in(&file);
    return in.readAll();
}

WorkspaceNavigator::SymbolKind WorkspaceNavigator::detectSymbolKind(const QString& line) {
    if (line.contains("class")) return SymbolKind::Class;
    if (line.contains("struct")) return SymbolKind::Struct;
    if (line.contains("enum")) return SymbolKind::Enum;
    return SymbolKind::Unknown;
}

QString WorkspaceNavigator::extractSymbolName(const QString& line, SymbolKind kind) {
    QRegularExpression re(R"(\w+)");
    QRegularExpressionMatch match = re.match(line);
    return match.hasMatch() ? match.captured(0) : QString();
}

QString WorkspaceNavigator::generateBookmarkId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}
