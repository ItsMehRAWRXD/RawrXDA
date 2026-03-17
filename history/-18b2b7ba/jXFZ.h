/**
 * @file database_tool_widget.h
 * @brief Full Database Tool widget for RawrXD IDE
 * @author RawrXD Team
 * 
 * Provides comprehensive database management including:
 * - Multiple database type support (SQLite, PostgreSQL, MySQL, MongoDB)
 * - Connection management with saved profiles
 * - SQL query editor with syntax highlighting
 * - Schema browser and table viewer
 * - Query history and favorites
 * - Result export to CSV/JSON
 */

#pragma once

#include <QWidget>
#include <QTreeWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QSplitter>
#include <QTabWidget>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QSettings>
#include <QJsonObject>
#include <QTimer>
#include <memory>

namespace RawrXD {

/**
 * @brief Database connection profile
 */
struct DatabaseConnection {
    QString name;
    QString driver;         // "QSQLITE", "QPSQL", "QMYSQL", "QODBC"
    QString host;
    int port = 0;
    QString database;
    QString username;
    QString password;       // Encrypted in storage
    QString connectionString;
    bool savePassword = false;
    
    QJsonObject toJson() const;
    static DatabaseConnection fromJson(const QJsonObject& obj);
    
    QString driverDisplayName() const;
    int defaultPort() const;
};

/**
 * @brief Query result structure
 */
struct QueryResult {
    bool success = false;
    QString error;
    QStringList columns;
    QVector<QVector<QVariant>> rows;
    int rowsAffected = 0;
    qint64 executionTimeMs = 0;
};

/**
 * @brief Query history entry
 */
struct QueryHistoryEntry {
    QString query;
    QString connection;
    QDateTime timestamp;
    bool success = false;
    qint64 executionTimeMs = 0;
    int rowsAffected = 0;
};

/**
 * @class DatabaseToolWidget
 * @brief Full-featured database management widget
 */
class DatabaseToolWidget : public QWidget {
    Q_OBJECT

public:
    explicit DatabaseToolWidget(QWidget* parent = nullptr);
    ~DatabaseToolWidget() override;

    // Connection management
    bool connect(const DatabaseConnection& conn);
    void disconnect();
    bool isConnected() const;
    QStringList getConnectionNames() const;
    DatabaseConnection* getConnection(const QString& name);
    
    // Query execution
    QueryResult executeQuery(const QString& sql);
    QueryResult executeQueryAsync(const QString& sql);
    void cancelQuery();
    
    // Schema browsing
    QStringList getTables();
    QStringList getColumns(const QString& table);
    QString getTableSchema(const QString& table);
    QStringList getStoredProcedures();
    QStringList getViews();
    
    // History
    QVector<QueryHistoryEntry> getHistory() const;
    void clearHistory();
    void addToFavorites(const QString& query, const QString& name);
    
    // Connection profiles
    void saveConnection(const DatabaseConnection& conn);
    void deleteConnection(const QString& name);
    void loadConnections();

signals:
    void connected(const QString& connectionName);
    void disconnected();
    void queryStarted();
    void queryFinished(const QueryResult& result);
    void queryError(const QString& error);
    void schemaChanged();

public slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onExecuteQuery();
    void onExplainQuery();
    void onStopQuery();
    void onSchemaItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onHistoryItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onExportResults();
    void refreshSchema();

private slots:
    void onConnectionSelected(int index);
    void onTableSelected(const QString& table);
    void updateConnectionUI();

private:
    void setupUI();
    void setupToolbar();
    void setupConnectionPanel();
    void setupQueryEditor();
    void setupSchemaTree();
    void setupResultsPanel();
    void setupHistoryPanel();
    
    void displayResults(const QueryResult& result);
    void addToHistory(const QString& query, const QueryResult& result);
    void loadHistory();
    void saveHistory();
    QString encryptPassword(const QString& password);
    QString decryptPassword(const QString& encrypted);

private:
    // UI Components
    QToolBar* m_toolbar = nullptr;
    QSplitter* m_mainSplitter = nullptr;
    QSplitter* m_rightSplitter = nullptr;
    QTabWidget* m_bottomTabs = nullptr;
    
    // Connection panel
    QComboBox* m_connectionCombo = nullptr;
    QPushButton* m_connectBtn = nullptr;
    QPushButton* m_disconnectBtn = nullptr;
    QPushButton* m_newConnectionBtn = nullptr;
    QLabel* m_statusLabel = nullptr;
    
    // Query editor
    QTextEdit* m_queryEditor = nullptr;
    QPushButton* m_executeBtn = nullptr;
    QPushButton* m_explainBtn = nullptr;
    QPushButton* m_stopBtn = nullptr;
    
    // Schema tree
    QTreeWidget* m_schemaTree = nullptr;
    
    // Results
    QTableWidget* m_resultsTable = nullptr;
    QTextEdit* m_messagesEdit = nullptr;
    QLabel* m_resultsStatusLabel = nullptr;
    
    // History
    QTreeWidget* m_historyTree = nullptr;
    QTreeWidget* m_favoritesTree = nullptr;
    
    // Data
    QVector<DatabaseConnection> m_connections;
    QString m_currentConnectionName;
    QSqlDatabase m_database;
    bool m_queryRunning = false;
    
    // History
    QVector<QueryHistoryEntry> m_history;
    QMap<QString, QString> m_favorites;
    
    // Settings
    QSettings* m_settings = nullptr;
};

/**
 * @class ConnectionEditDialog
 * @brief Dialog for creating/editing database connections
 */
class ConnectionEditDialog : public QDialog {
    Q_OBJECT

public:
    explicit ConnectionEditDialog(QWidget* parent = nullptr);
    
    void setConnection(const DatabaseConnection& conn);
    DatabaseConnection getConnection() const;

private slots:
    void onDriverChanged(const QString& driver);
    void onTestConnection();
    void validateInput();

private:
    void setupUI();
    
    QLineEdit* m_nameEdit = nullptr;
    QComboBox* m_driverCombo = nullptr;
    QLineEdit* m_hostEdit = nullptr;
    QSpinBox* m_portSpin = nullptr;
    QLineEdit* m_databaseEdit = nullptr;
    QLineEdit* m_usernameEdit = nullptr;
    QLineEdit* m_passwordEdit = nullptr;
    QCheckBox* m_savePasswordCheck = nullptr;
    QTextEdit* m_connectionStringEdit = nullptr;
    QPushButton* m_testBtn = nullptr;
    QPushButton* m_okBtn = nullptr;
    QLabel* m_testResultLabel = nullptr;
};

} // namespace RawrXD
