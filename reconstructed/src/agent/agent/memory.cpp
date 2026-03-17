#include "agent_memory.hpp"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDateTime>
#include <QMutexLocker>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>

AgentMemory::AgentMemory(QObject* parent)
    : QObject(parent)
    , m_database(QSqlDatabase::addDatabase("QSQLITE"))
    , m_mutex(QMutex::Recursive)
{
}

AgentMemory::~AgentMemory()
{
    if (m_database.isOpen()) {
        m_database.close();
    }
}

bool AgentMemory::initialize(const QString& databasePath)
{
    QMutexLocker locker(&m_mutex);
    
    // Store the database path
    m_databasePath = databasePath;
    
    // Ensure the directory exists
    QFileInfo fileInfo(databasePath);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qWarning() << "Failed to create directory for database:" << dir.absolutePath();
            return false;
        }
    }
    
    // Configure the database
    m_database.setDatabaseName(databasePath);
    
    // Open the database
    if (!m_database.open()) {
        qWarning() << "Failed to open database:" << m_database.lastError().text();
        return false;
    }
    
    // Create tables
    if (!createTables()) {
        qWarning() << "Failed to create database tables";
        return false;
    }
    
    qDebug() << "AgentMemory initialized successfully with database:" << databasePath;
    return true;
}

bool AgentMemory::createTables()
{
    QMutexLocker locker(&m_mutex);
    
    QSqlQuery query(m_database);
    
    // Create execution_history table
    QString createExecutionTable = R"(
        CREATE TABLE IF NOT EXISTS execution_history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            wish TEXT NOT NULL,
            plan TEXT,
            success BOOLEAN,
            execution_time INTEGER,
            error_message TEXT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";
    
    if (!query.exec(createExecutionTable)) {
        qWarning() << "Failed to create execution_history table:" << query.lastError().text();
        return false;
    }
    
    // Create learned_patterns table
    QString createPatternsTable = R"(
        CREATE TABLE IF NOT EXISTS learned_patterns (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            pattern_type TEXT,
            pattern_data TEXT,
            project_context TEXT,
            success_rate REAL DEFAULT 0.5,
            usage_count INTEGER DEFAULT 1,
            last_used DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";
    
    if (!query.exec(createPatternsTable)) {
        qWarning() << "Failed to create learned_patterns table:" << query.lastError().text();
        return false;
    }
    
    // Create user_preferences table
    QString createPreferencesTable = R"(
        CREATE TABLE IF NOT EXISTS user_preferences (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            preference_type TEXT,
            preference_value TEXT,
            confidence_level REAL DEFAULT 0.5,
            last_updated DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";
    
    if (!query.exec(createPreferencesTable)) {
        qWarning() << "Failed to create user_preferences table:" << query.lastError().text();
        return false;
    }
    
    return true;
}

void AgentMemory::recordExecution(const QString& wish, const QString& plan, bool success, 
                                 int executionTime, const QString& errorMessage)
{
    QMutexLocker locker(&m_mutex);
    
    QSqlQuery query(m_database);
    query.prepare(R"(
        INSERT INTO execution_history 
        (wish, plan, success, execution_time, error_message, timestamp)
        VALUES (?, ?, ?, ?, ?, ?)
    )");
    
    query.addBindValue(wish);
    query.addBindValue(plan);
    query.addBindValue(success);
    query.addBindValue(executionTime);
    query.addBindValue(errorMessage);
    query.addBindValue(QDateTime::currentDateTime());
    
    if (!query.exec()) {
        qWarning() << "Failed to record execution:" << query.lastError().text();
        return;
    }
    
    // Emit signal
    ExecutionRecord record;
    record.id = query.lastInsertId().toInt();
    record.wish = wish;
    record.plan = plan;
    record.success = success;
    record.executionTime = executionTime;
    record.errorMessage = errorMessage;
    record.timestamp = QDateTime::currentDateTime();
    
    emit executionRecorded(record);
}

QList<ExecutionRecord> AgentMemory::getExecutionHistory(int limit) const
{
    QMutexLocker locker(&m_mutex);
    
    QSqlQuery query(m_database);
    query.prepare(R"(
        SELECT id, wish, plan, success, execution_time, error_message, timestamp
        FROM execution_history
        ORDER BY timestamp DESC
        LIMIT ?
    )");
    
    query.addBindValue(limit);
    
    if (!query.exec()) {
        qWarning() << "Failed to retrieve execution history:" << query.lastError().text();
        return QList<ExecutionRecord>();
    }
    
    QList<ExecutionRecord> records;
    while (query.next()) {
        ExecutionRecord record;
        record.id = query.value(0).toInt();
        record.wish = query.value(1).toString();
        record.plan = query.value(2).toString();
        record.success = query.value(3).toBool();
        record.executionTime = query.value(4).toInt();
        record.errorMessage = query.value(5).toString();
        record.timestamp = query.value(6).toDateTime();
        records.append(record);
    }
    
    return records;
}

void AgentMemory::recordPattern(const QString& patternType, const QString& patternData,
                               const QString& projectContext, bool success)
{
    QMutexLocker locker(&m_mutex);
    
    // First, check if this pattern already exists
    QSqlQuery findQuery(m_database);
    findQuery.prepare(R"(
        SELECT id, success_rate, usage_count
        FROM learned_patterns
        WHERE pattern_type = ? AND pattern_data = ? AND project_context = ?
    )");
    
    findQuery.addBindValue(patternType);
    findQuery.addBindValue(patternData);
    findQuery.addBindValue(projectContext);
    
    if (!findQuery.exec()) {
        qWarning() << "Failed to search for existing pattern:" << findQuery.lastError().text();
        return;
    }
    
    if (findQuery.next()) {
        // Pattern exists, update it
        int patternId = findQuery.value(0).toInt();
        double currentSuccessRate = findQuery.value(1).toDouble();
        int usageCount = findQuery.value(2).toInt();
        
        // Calculate new success rate
        double newSuccessRate = (currentSuccessRate * usageCount + (success ? 1.0 : 0.0)) / (usageCount + 1);
        int newUsageCount = usageCount + 1;
        
        QSqlQuery updateQuery(m_database);
        updateQuery.prepare(R"(
            UPDATE learned_patterns
            SET success_rate = ?, usage_count = ?, last_used = ?
            WHERE id = ?
        )");
        
        updateQuery.addBindValue(newSuccessRate);
        updateQuery.addBindValue(newUsageCount);
        updateQuery.addBindValue(QDateTime::currentDateTime());
        updateQuery.addBindValue(patternId);
        
        if (!updateQuery.exec()) {
            qWarning() << "Failed to update pattern:" << updateQuery.lastError().text();
            return;
        }
        
        // Emit signal
        LearnedPattern pattern;
        pattern.id = patternId;
        pattern.patternType = patternType;
        pattern.patternData = patternData;
        pattern.projectContext = projectContext;
        pattern.successRate = newSuccessRate;
        pattern.lastUsed = QDateTime::currentDateTime();
        
        emit patternLearned(pattern);
    } else {
        // Pattern doesn't exist, create new one
        QSqlQuery insertQuery(m_database);
        insertQuery.prepare(R"(
            INSERT INTO learned_patterns
            (pattern_type, pattern_data, project_context, success_rate, usage_count, last_used)
            VALUES (?, ?, ?, ?, ?, ?)
        )");
        
        insertQuery.addBindValue(patternType);
        insertQuery.addBindValue(patternData);
        insertQuery.addBindValue(projectContext);
        insertQuery.addBindValue(success ? 1.0 : 0.0);
        insertQuery.addBindValue(1);
        insertQuery.addBindValue(QDateTime::currentDateTime());
        
        if (!insertQuery.exec()) {
            qWarning() << "Failed to insert new pattern:" << insertQuery.lastError().text();
            return;
        }
        
        // Emit signal
        LearnedPattern pattern;
        pattern.id = insertQuery.lastInsertId().toInt();
        pattern.patternType = patternType;
        pattern.patternData = patternData;
        pattern.projectContext = projectContext;
        pattern.successRate = success ? 1.0 : 0.0;
        pattern.lastUsed = QDateTime::currentDateTime();
        
        emit patternLearned(pattern);
    }
}

QList<LearnedPattern> AgentMemory::getLearnedPatterns(const QString& patternType) const
{
    QMutexLocker locker(&m_mutex);
    
    QString queryString;
    QSqlQuery query(m_database);
    
    if (patternType.isEmpty()) {
        queryString = R"(
            SELECT id, pattern_type, pattern_data, project_context, success_rate, last_used
            FROM learned_patterns
            ORDER BY last_used DESC
        )";
        query.prepare(queryString);
    } else {
        queryString = R"(
            SELECT id, pattern_type, pattern_data, project_context, success_rate, last_used
            FROM learned_patterns
            WHERE pattern_type = ?
            ORDER BY last_used DESC
        )";
        query.prepare(queryString);
        query.addBindValue(patternType);
    }
    
    if (!query.exec()) {
        qWarning() << "Failed to retrieve learned patterns:" << query.lastError().text();
        return QList<LearnedPattern>();
    }
    
    QList<LearnedPattern> patterns;
    while (query.next()) {
        LearnedPattern pattern;
        pattern.id = query.value(0).toInt();
        pattern.patternType = query.value(1).toString();
        pattern.patternData = query.value(2).toString();
        pattern.projectContext = query.value(3).toString();
        pattern.successRate = query.value(4).toDouble();
        pattern.lastUsed = query.value(5).toDateTime();
        patterns.append(pattern);
    }
    
    return patterns;
}

void AgentMemory::updateUserPreference(const QString& preferenceType, const QString& preferenceValue,
                                      double confidence)
{
    QMutexLocker locker(&m_mutex);
    
    // Check if preference already exists
    QSqlQuery findQuery(m_database);
    findQuery.prepare(R"(
        SELECT id, confidence_level
        FROM user_preferences
        WHERE preference_type = ?
    )");
    
    findQuery.addBindValue(preferenceType);
    
    if (!findQuery.exec()) {
        qWarning() << "Failed to search for existing preference:" << findQuery.lastError().text();
        return;
    }
    
    if (findQuery.next()) {
        // Preference exists, update it with weighted confidence
        int preferenceId = findQuery.value(0).toInt();
        double currentConfidence = findQuery.value(1).toDouble();
        
        // Calculate new confidence (simple average for now)
        double newConfidence = (currentConfidence + confidence) / 2.0;
        
        QSqlQuery updateQuery(m_database);
        updateQuery.prepare(R"(
            UPDATE user_preferences
            SET preference_value = ?, confidence_level = ?, last_updated = ?
            WHERE id = ?
        )");
        
        updateQuery.addBindValue(preferenceValue);
        updateQuery.addBindValue(newConfidence);
        updateQuery.addBindValue(QDateTime::currentDateTime());
        updateQuery.addBindValue(preferenceId);
        
        if (!updateQuery.exec()) {
            qWarning() << "Failed to update preference:" << updateQuery.lastError().text();
            return;
        }
        
        // Emit signal
        UserPreference preference;
        preference.id = preferenceId;
        preference.preferenceType = preferenceType;
        preference.preferenceValue = preferenceValue;
        preference.confidenceLevel = newConfidence;
        preference.lastUpdated = QDateTime::currentDateTime();
        
        emit preferenceUpdated(preference);
    } else {
        // Preference doesn't exist, create new one
        QSqlQuery insertQuery(m_database);
        insertQuery.prepare(R"(
            INSERT INTO user_preferences
            (preference_type, preference_value, confidence_level, last_updated)
            VALUES (?, ?, ?, ?)
        )");
        
        insertQuery.addBindValue(preferenceType);
        insertQuery.addBindValue(preferenceValue);
        insertQuery.addBindValue(confidence);
        insertQuery.addBindValue(QDateTime::currentDateTime());
        
        if (!insertQuery.exec()) {
            qWarning() << "Failed to insert new preference:" << insertQuery.lastError().text();
            return;
        }
        
        // Emit signal
        UserPreference preference;
        preference.id = insertQuery.lastInsertId().toInt();
        preference.preferenceType = preferenceType;
        preference.preferenceValue = preferenceValue;
        preference.confidenceLevel = confidence;
        preference.lastUpdated = QDateTime::currentDateTime();
        
        emit preferenceUpdated(preference);
    }
}

QList<UserPreference> AgentMemory::getUserPreferences(const QString& preferenceType) const
{
    QMutexLocker locker(&m_mutex);
    
    QString queryString;
    QSqlQuery query(m_database);
    
    if (preferenceType.isEmpty()) {
        queryString = R"(
            SELECT id, preference_type, preference_value, confidence_level, last_updated
            FROM user_preferences
            ORDER BY last_updated DESC
        )";
        query.prepare(queryString);
    } else {
        queryString = R"(
            SELECT id, preference_type, preference_value, confidence_level, last_updated
            FROM user_preferences
            WHERE preference_type = ?
            ORDER BY last_updated DESC
        )";
        query.prepare(queryString);
        query.addBindValue(preferenceType);
    }
    
    if (!query.exec()) {
        qWarning() << "Failed to retrieve user preferences:" << query.lastError().text();
        return QList<UserPreference>();
    }
    
    QList<UserPreference> preferences;
    while (query.next()) {
        UserPreference preference;
        preference.id = query.value(0).toInt();
        preference.preferenceType = query.value(1).toString();
        preference.preferenceValue = query.value(2).toString();
        preference.confidenceLevel = query.value(3).toDouble();
        preference.lastUpdated = query.value(4).toDateTime();
        preferences.append(preference);
    }
    
    return preferences;
}

double AgentMemory::getPatternSuccessRate(const QString& patternType) const
{
    QMutexLocker locker(&m_mutex);
    
    QSqlQuery query(m_database);
    query.prepare(R"(
        SELECT AVG(success_rate)
        FROM learned_patterns
        WHERE pattern_type = ?
    )");
    
    query.addBindValue(patternType);
    
    if (!query.exec() || !query.next()) {
        return 0.0;
    }
    
    QVariant result = query.value(0);
    return result.isNull() ? 0.0 : result.toDouble();
}

double AgentMemory::getAverageExecutionTime() const
{
    QMutexLocker locker(&m_mutex);
    
    QSqlQuery query(m_database);
    query.prepare(R"(
        SELECT AVG(execution_time)
        FROM execution_history
        WHERE success = 1
    )");
    
    if (!query.exec() || !query.next()) {
        return 0.0;
    }
    
    QVariant result = query.value(0);
    return result.isNull() ? 0.0 : result.toDouble();
}

double AgentMemory::getFailureRate() const
{
    QMutexLocker locker(&m_mutex);
    
    QSqlQuery totalCountQuery(m_database);
    totalCountQuery.prepare("SELECT COUNT(*) FROM execution_history");
    
    if (!totalCountQuery.exec() || !totalCountQuery.next()) {
        return 0.0;
    }
    
    int totalCount = totalCountQuery.value(0).toInt();
    
    if (totalCount == 0) {
        return 0.0;
    }
    
    QSqlQuery failureCountQuery(m_database);
    failureCountQuery.prepare("SELECT COUNT(*) FROM execution_history WHERE success = 0");
    
    if (!failureCountQuery.exec() || !failureCountQuery.next()) {
        return 0.0;
    }
    
    int failureCount = failureCountQuery.value(0).toInt();
    
    return static_cast<double>(failureCount) / totalCount;
}