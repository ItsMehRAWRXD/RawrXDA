// D:\temp\RawrXD-agentic-ide-production\RawrXD-ModelLoader\include\database_manager.h
// Database Manager header with connection pooling and caching

#pragma once

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariant>
#include <memory>
#include <vector>

namespace RawrXD {
namespace Database {

struct DatabaseConfig {
    QString host = "localhost";
    int port = 5432;
    QString username = "postgres";
    QString password = "";
    QString database = "production_db";
    bool sslEnabled = true;
    int connectionTimeout = 30;
    int maxRetries = 3;
};

struct QueryResult {
    bool success = false;
    QVariantList rows;
    int rowCount = 0;
    QString errorMessage;
};

class DatabaseManager {
public:
    class Impl;
    
    DatabaseManager();
    ~DatabaseManager();
    
    // Initialization
    bool initialize(const QString& dbType, const DatabaseConfig& config);
    
    // Query execution
    QueryResult executeQuery(const QString& sql, const QVariantList& params = {}, const QString& dbType = "postgres");
    bool executeMutation(const QString& sql, const QVariantList& params = {}, const QString& dbType = "postgres");
    
    // Migrations
    bool runMigration(const QString& migrationName, const QString& sql);
    
    // Caching
    bool cacheQuery(const QString& cacheKey, const QString& sql, int ttlSeconds = 300);
    QueryResult getCachedQuery(const QString& cacheKey);
    
private:
    bool createConnectionPool(const QString& dbType);
    QString acquireConnection(const QString& dbType);
    void releaseConnection(const QString& connName);
    
    std::unique_ptr<Impl> impl;
};

} // namespace Database
} // namespace RawrXD
