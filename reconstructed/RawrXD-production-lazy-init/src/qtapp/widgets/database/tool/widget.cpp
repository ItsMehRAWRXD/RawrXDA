/**
 * @file database_tool_widget.cpp
 * @brief Full Database Tool implementation for RawrXD IDE
 * @author RawrXD Team
 */

#include "database_tool_widget.h"
#include "integration/ProdIntegration.h"
#include "integration/InitializationTracker.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolBar>
#include <QLabel>
#include <QHeaderView>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QSqlTableModel>
#include <QSqlIndex>
#include <QSqlDriver>
#include <QSqlField>
#include <QElapsedTimer>
#include <QClipboard>
#include <QApplication>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QCryptographicHash>
#include <QRandomGenerator>

// No namespace - classes are at global scope (matching header)

// =============================================================================
// DatabaseConnection Implementation
// =============================================================================

QJsonObject DatabaseConnection::toJson() const {
    QJsonObject obj;
    obj["name"] = name;
    obj["driver"] = driver;
    obj["host"] = host;
    obj["port"] = port;
    obj["database"] = database;
    obj["username"] = username;
    obj["savePassword"] = savePassword;
    obj["connectionString"] = connectionString;
    
    // Only save encrypted password if requested
    if (savePassword && !password.isEmpty()) {
        // Simple XOR obfuscation (not secure, but better than plaintext)
        QByteArray data = password.toUtf8();
        for (int i = 0; i < data.size(); ++i) {
            data[i] = data[i] ^ 0x5A;
        }
        obj["password"] = QString::fromLatin1(data.toBase64());
    }
    
    return obj;
}

DatabaseConnection DatabaseConnection::fromJson(const QJsonObject& obj) {
    DatabaseConnection conn;
    conn.name = obj["name"].toString();
    conn.driver = obj["driver"].toString();
    conn.host = obj["host"].toString();
    conn.port = obj["port"].toInt();
    conn.database = obj["database"].toString();
    conn.username = obj["username"].toString();
    conn.savePassword = obj["savePassword"].toBool();
    conn.connectionString = obj["connectionString"].toString();
    
    if (conn.savePassword && obj.contains("password")) {
        QByteArray data = QByteArray::fromBase64(obj["password"].toString().toLatin1());
        for (int i = 0; i < data.size(); ++i) {
            data[i] = data[i] ^ 0x5A;
        }
        conn.password = QString::fromUtf8(data);
    }
    
    return conn;
}

QString DatabaseConnection::driverDisplayName() const {
    if (driver == "QSQLITE") return "SQLite";
    if (driver == "QPSQL") return "PostgreSQL";
    if (driver == "QMYSQL") return "MySQL";
    if (driver == "QODBC") return "ODBC";
    if (driver == "QOCI") return "Oracle";
    return driver;
}

int DatabaseConnection::defaultPort() const {
    if (driver == "QPSQL") return 5432;
    if (driver == "QMYSQL") return 3306;
    if (driver == "QOCI") return 1521;
    return 0;
}

// =============================================================================
// DatabaseToolWidget Implementation
// =============================================================================

DatabaseToolWidget::DatabaseToolWidget(QWidget* parent)
    : QWidget(parent)
    , m_settings(new QSettings("RawrXD", "IDE", this))
{
    RawrXD::Integration::ScopedInitTimer init("DatabaseToolWidget");
    setupUI();
    loadConnections();
    loadHistory();
    updateConnectionUI();
}

DatabaseToolWidget::~DatabaseToolWidget() {
    disconnectFromDatabase();
    saveHistory();
}

void DatabaseToolWidget::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    setupToolbar();
    mainLayout->addWidget(m_toolbar);
    
    // Main horizontal splitter
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    
    // Left panel - Schema tree
    QWidget* leftPanel = new QWidget(this);
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(4, 4, 4, 4);
    
    setupConnectionPanel();
    setupSchemaTree();
    
    leftLayout->addWidget(m_schemaTree);
    leftPanel->setMinimumWidth(200);
    leftPanel->setMaximumWidth(400);
    
    m_mainSplitter->addWidget(leftPanel);
    
    // Right panel - Query editor and results
    m_rightSplitter = new QSplitter(Qt::Vertical, this);
    
    setupQueryEditor();
    setupResultsPanel();
    setupHistoryPanel();
    
    m_rightSplitter->addWidget(m_queryEditor->parentWidget());
    m_rightSplitter->addWidget(m_bottomTabs);
    m_rightSplitter->setStretchFactor(0, 1);
    m_rightSplitter->setStretchFactor(1, 2);
    
    m_mainSplitter->addWidget(m_rightSplitter);
    m_mainSplitter->setStretchFactor(0, 1);
    m_mainSplitter->setStretchFactor(1, 3);
    
    mainLayout->addWidget(m_mainSplitter);
}

void DatabaseToolWidget::setupToolbar() {
    m_toolbar = new QToolBar("Database Toolbar", this);
    m_toolbar->setIconSize(QSize(16, 16));
    
    // Connection selector
    m_toolbar->addWidget(new QLabel(" Connection: "));
    m_connectionCombo = new QComboBox(this);
    m_connectionCombo->setMinimumWidth(150);
    connect(m_connectionCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &DatabaseToolWidget::onConnectionSelected);
    m_toolbar->addWidget(m_connectionCombo);
    
    m_newConnectionBtn = new QPushButton("+", this);
    m_newConnectionBtn->setToolTip("New Connection");
    m_newConnectionBtn->setMaximumWidth(30);
    connect(m_newConnectionBtn, &QPushButton::clicked, this, [this]() {
        ConnectionEditDialog dialog(this);
        if (dialog.exec() == QDialog::Accepted) {
            saveConnection(dialog.getConnection());
            loadConnections();
        }
    });
    m_toolbar->addWidget(m_newConnectionBtn);
    
    m_connectBtn = new QPushButton("Connect", this);
    connect(m_connectBtn, &QPushButton::clicked, this, &DatabaseToolWidget::onConnectClicked);
    m_toolbar->addWidget(m_connectBtn);
    
    m_disconnectBtn = new QPushButton("Disconnect", this);
    m_disconnectBtn->setEnabled(false);
    connect(m_disconnectBtn, &QPushButton::clicked, this, &DatabaseToolWidget::onDisconnectClicked);
    m_toolbar->addWidget(m_disconnectBtn);
    
    m_toolbar->addSeparator();
    
    // Query controls
    m_executeBtn = new QPushButton("▶ Execute", this);
    m_executeBtn->setToolTip("Execute Query (F5)");
    m_executeBtn->setShortcut(QKeySequence(Qt::Key_F5));
    m_executeBtn->setEnabled(false);
    connect(m_executeBtn, &QPushButton::clicked, this, &DatabaseToolWidget::onExecuteQuery);
    m_toolbar->addWidget(m_executeBtn);
    
    m_explainBtn = new QPushButton("Explain", this);
    m_explainBtn->setToolTip("Explain Query Plan");
    m_explainBtn->setEnabled(false);
    connect(m_explainBtn, &QPushButton::clicked, this, &DatabaseToolWidget::onExplainQuery);
    m_toolbar->addWidget(m_explainBtn);
    
    m_stopBtn = new QPushButton("⏹ Stop", this);
    m_stopBtn->setEnabled(false);
    connect(m_stopBtn, &QPushButton::clicked, this, &DatabaseToolWidget::onStopQuery);
    m_toolbar->addWidget(m_stopBtn);
    
    m_toolbar->addSeparator();
    
    QPushButton* exportBtn = new QPushButton("Export", this);
    connect(exportBtn, &QPushButton::clicked, this, &DatabaseToolWidget::onExportResults);
    m_toolbar->addWidget(exportBtn);
    
    m_toolbar->addSeparator();
    
    m_statusLabel = new QLabel("Not connected", this);
    m_statusLabel->setStyleSheet("color: gray;");
    m_toolbar->addWidget(m_statusLabel);
}

void DatabaseToolWidget::setupConnectionPanel() {
    // Connection combo populated in loadConnections()
}

void DatabaseToolWidget::setupQueryEditor() {
    QWidget* editorWidget = new QWidget(this);
    QVBoxLayout* editorLayout = new QVBoxLayout(editorWidget);
    editorLayout->setContentsMargins(4, 4, 4, 4);
    
    QLabel* label = new QLabel("SQL Query:", this);
    editorLayout->addWidget(label);
    
    m_queryEditor = new QTextEdit(this);
    m_queryEditor->setFont(QFont("Consolas", 11));
    m_queryEditor->setPlaceholderText("Enter SQL query here...\n\nExamples:\nSELECT * FROM table_name;\nINSERT INTO table_name (col1, col2) VALUES (val1, val2);");
    m_queryEditor->setAcceptRichText(false);
    m_queryEditor->setStyleSheet(
        "QTextEdit { background-color: #1e1e1e; color: #d4d4d4; "
        "selection-background-color: #264f78; }");
    
    editorLayout->addWidget(m_queryEditor);
}

void DatabaseToolWidget::setupSchemaTree() {
    m_schemaTree = new QTreeWidget(this);
    m_schemaTree->setHeaderLabels({"Database Objects"});
    m_schemaTree->setContextMenuPolicy(Qt::CustomContextMenu);
    
    connect(m_schemaTree, &QTreeWidget::itemDoubleClicked,
            this, &DatabaseToolWidget::onSchemaItemDoubleClicked);
    
    connect(m_schemaTree, &QTreeWidget::customContextMenuRequested,
            this, [this](const QPoint& pos) {
        QTreeWidgetItem* item = m_schemaTree->itemAt(pos);
        if (!item) return;
        
        QString type = item->data(0, Qt::UserRole).toString();
        QString name = item->data(0, Qt::UserRole + 1).toString();
        
        QMenu menu(this);
        
        if (type == "table") {
            QAction* selectAction = menu.addAction("SELECT * FROM " + name);
            connect(selectAction, &QAction::triggered, this, [this, name]() {
                m_queryEditor->setPlainText(QString("SELECT * FROM %1 LIMIT 100;").arg(name));
                onExecuteQuery();
            });
            
            QAction* descAction = menu.addAction("Describe Table");
            connect(descAction, &QAction::triggered, this, [this, name]() {
                QString schema = getTableSchema(name);
                m_messagesEdit->setPlainText(schema);
                m_bottomTabs->setCurrentIndex(1);  // Messages tab
            });
            
            menu.addSeparator();
            
            QAction* countAction = menu.addAction("Count Rows");
            connect(countAction, &QAction::triggered, this, [this, name]() {
                m_queryEditor->setPlainText(QString("SELECT COUNT(*) FROM %1;").arg(name));
                onExecuteQuery();
            });
        } else if (type == "column") {
            QString table = item->parent()->data(0, Qt::UserRole + 1).toString();
            
            QAction* selectAction = menu.addAction("SELECT " + name);
            connect(selectAction, &QAction::triggered, this, [this, table, name]() {
                m_queryEditor->setPlainText(QString("SELECT %1 FROM %2 LIMIT 100;").arg(name, table));
            });
        }
        
        if (!menu.isEmpty()) {
            menu.exec(m_schemaTree->mapToGlobal(pos));
        }
    });
}

void DatabaseToolWidget::setupResultsPanel() {
    m_bottomTabs = new QTabWidget(this);
    
    // Results table
    m_resultsTable = new QTableWidget(this);
    m_resultsTable->setAlternatingRowColors(true);
    m_resultsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_resultsTable->horizontalHeader()->setStretchLastSection(true);
    m_resultsTable->setContextMenuPolicy(Qt::CustomContextMenu);
    
    connect(m_resultsTable, &QTableWidget::customContextMenuRequested,
            this, [this](const QPoint& pos) {
        QMenu menu(this);
        
        QAction* copyAction = menu.addAction("Copy Selected");
        connect(copyAction, &QAction::triggered, this, [this]() {
            QStringList rows;
            for (const QModelIndex& index : m_resultsTable->selectionModel()->selectedRows()) {
                int row = index.row();
                QStringList cols;
                for (int col = 0; col < m_resultsTable->columnCount(); ++col) {
                    QTableWidgetItem* item = m_resultsTable->item(row, col);
                    cols.append(item ? item->text() : "");
                }
                rows.append(cols.join("\t"));
            }
            QApplication::clipboard()->setText(rows.join("\n"));
        });
        
        QAction* copyAllAction = menu.addAction("Copy All");
        connect(copyAllAction, &QAction::triggered, this, [this]() {
            QStringList rows;
            // Header
            QStringList header;
            for (int col = 0; col < m_resultsTable->columnCount(); ++col) {
                header.append(m_resultsTable->horizontalHeaderItem(col)->text());
            }
            rows.append(header.join("\t"));
            // Data
            for (int row = 0; row < m_resultsTable->rowCount(); ++row) {
                QStringList cols;
                for (int col = 0; col < m_resultsTable->columnCount(); ++col) {
                    QTableWidgetItem* item = m_resultsTable->item(row, col);
                    cols.append(item ? item->text() : "");
                }
                rows.append(cols.join("\t"));
            }
            QApplication::clipboard()->setText(rows.join("\n"));
        });
        
        menu.exec(m_resultsTable->mapToGlobal(pos));
    });
    
    QWidget* resultsWidget = new QWidget(this);
    QVBoxLayout* resultsLayout = new QVBoxLayout(resultsWidget);
    resultsLayout->setContentsMargins(0, 0, 0, 0);
    resultsLayout->addWidget(m_resultsTable);
    
    m_resultsStatusLabel = new QLabel("No results", this);
    resultsLayout->addWidget(m_resultsStatusLabel);
    
    m_bottomTabs->addTab(resultsWidget, "Results");
    
    // Messages
    m_messagesEdit = new QTextEdit(this);
    m_messagesEdit->setReadOnly(true);
    m_messagesEdit->setFont(QFont("Consolas", 10));
    m_bottomTabs->addTab(m_messagesEdit, "Messages");
}

void DatabaseToolWidget::setupHistoryPanel() {
    // History tree
    m_historyTree = new QTreeWidget(this);
    m_historyTree->setHeaderLabels({"Query", "Time", "Duration", "Status"});
    m_historyTree->setColumnWidth(0, 400);
    m_historyTree->setColumnWidth(1, 150);
    m_historyTree->setColumnWidth(2, 80);
    m_historyTree->setRootIsDecorated(false);
    
    connect(m_historyTree, &QTreeWidget::itemDoubleClicked,
            this, &DatabaseToolWidget::onHistoryItemDoubleClicked);
    
    m_bottomTabs->addTab(m_historyTree, "History");
    
    // Favorites
    m_favoritesTree = new QTreeWidget(this);
    m_favoritesTree->setHeaderLabels({"Name", "Query"});
    m_favoritesTree->setColumnWidth(0, 150);
    m_favoritesTree->setRootIsDecorated(false);
    
    connect(m_favoritesTree, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem* item, int) {
        m_queryEditor->setPlainText(item->text(1));
    });
    
    m_bottomTabs->addTab(m_favoritesTree, "Favorites");
}

// =============================================================================
// Connection Management
// =============================================================================

bool DatabaseToolWidget::connectToDatabase(const DatabaseConnection& conn) {
    disconnectFromDatabase();
    
    QString connectionName = QString("rawrxd_db_%1").arg(QRandomGenerator::global()->generate());
    m_database = QSqlDatabase::addDatabase(conn.driver, connectionName);
    
    if (conn.driver == "QSQLITE") {
        m_database.setDatabaseName(conn.database);
    } else {
        m_database.setHostName(conn.host);
        m_database.setPort(conn.port);
        m_database.setDatabaseName(conn.database);
        m_database.setUserName(conn.username);
        m_database.setPassword(conn.password);
        
        if (!conn.connectionString.isEmpty()) {
            m_database.setConnectOptions(conn.connectionString);
        }
    }
    
    if (!m_database.open()) {
        QString error = m_database.lastError().text();
        m_messagesEdit->append(QString("Connection failed: %1").arg(error));
        emit queryError(error);
        return false;
    }
    
    m_currentConnectionName = conn.name;
    m_statusLabel->setText(QString("Connected to %1").arg(conn.name));
    m_statusLabel->setStyleSheet("color: #4ec9b0;");
    
    updateConnectionUI();
    refreshSchema();
    
    emit connected(conn.name);
    return true;
}

void DatabaseToolWidget::disconnectFromDatabase() {
    if (m_database.isOpen()) {
        m_database.close();
    }
    
    QString connName = m_database.connectionName();
    m_database = QSqlDatabase();
    QSqlDatabase::removeDatabase(connName);
    
    m_currentConnectionName.clear();
    m_statusLabel->setText("Not connected");
    m_statusLabel->setStyleSheet("color: gray;");
    
    m_schemaTree->clear();
    updateConnectionUI();
    
    emit disconnected();
}

bool DatabaseToolWidget::isConnected() const {
    return m_database.isOpen();
}

QStringList DatabaseToolWidget::getConnectionNames() const {
    QStringList names;
    for (const DatabaseConnection& conn : m_connections) {
        names.append(conn.name);
    }
    return names;
}

DatabaseConnection* DatabaseToolWidget::getConnection(const QString& name) {
    for (DatabaseConnection& conn : m_connections) {
        if (conn.name == name) {
            return &conn;
        }
    }
    return nullptr;
}

// =============================================================================
// Query Execution
// =============================================================================

QueryResult DatabaseToolWidget::executeQuery(const QString& sql) {
    QueryResult result;
    
    if (!isConnected()) {
        result.error = "Not connected to database";
        return result;
    }
    
    m_queryRunning = true;
    m_stopBtn->setEnabled(true);
    m_executeBtn->setEnabled(false);
    emit queryStarted();
    
    QElapsedTimer timer;
    timer.start();
    
    QSqlQuery query(m_database);
    result.success = query.exec(sql);
    
    result.executionTimeMs = timer.elapsed();
    
    if (result.success) {
        // Check if it's a SELECT query
        if (query.isSelect()) {
            QSqlRecord record = query.record();
            
            // Get column names
            for (int i = 0; i < record.count(); ++i) {
                result.columns.append(record.fieldName(i));
            }
            
            // Get rows
            while (query.next()) {
                QVector<QVariant> row;
                for (int i = 0; i < record.count(); ++i) {
                    row.append(query.value(i));
                }
                result.rows.append(row);
            }
            
            result.rowsAffected = result.rows.size();
        } else {
            result.rowsAffected = query.numRowsAffected();
        }
        
        m_messagesEdit->append(QString("Query executed successfully. %1 row(s) affected. (%2 ms)")
            .arg(result.rowsAffected).arg(result.executionTimeMs));
    } else {
        result.error = query.lastError().text();
        m_messagesEdit->append(QString("Error: %1").arg(result.error));
    }
    
    m_queryRunning = false;
    m_stopBtn->setEnabled(false);
    m_executeBtn->setEnabled(true);
    
    displayResults(result);
    addToHistory(sql, result);
    
    emit queryFinished(result);
    return result;
}

QueryResult DatabaseToolWidget::executeQueryAsync(const QString& sql) {
    // For now, just execute synchronously
    // Full async would use QtConcurrent or QThread
    return executeQuery(sql);
}

void DatabaseToolWidget::cancelQuery() {
    // Note: SQLite doesn't support query cancellation well
    // For other drivers, we could use database-specific methods
    m_queryRunning = false;
}

// =============================================================================
// Schema Browsing
// =============================================================================

QStringList DatabaseToolWidget::getTables() {
    if (!isConnected()) return {};
    return m_database.tables(QSql::Tables);
}

QStringList DatabaseToolWidget::getColumns(const QString& table) {
    if (!isConnected()) return {};
    
    QStringList columns;
    QSqlRecord record = m_database.record(table);
    
    for (int i = 0; i < record.count(); ++i) {
        columns.append(record.fieldName(i));
    }
    
    return columns;
}

QString DatabaseToolWidget::getTableSchema(const QString& table) {
    if (!isConnected()) return QString();
    
    QString schema;
    QSqlRecord record = m_database.record(table);
    
    schema += QString("Table: %1\n").arg(table);
    schema += QString("Columns: %1\n\n").arg(record.count());
    
    for (int i = 0; i < record.count(); ++i) {
        QSqlField field = record.field(i);
        schema += QString("  %1: %2")
            .arg(field.name())
            .arg(QVariant::typeToName(field.type()));
        
        if (field.length() > 0) {
            schema += QString("(%1)").arg(field.length());
        }
        
        if (field.requiredStatus() == QSqlField::Required) {
            schema += " NOT NULL";
        }
        
        schema += "\n";
    }
    
    // Get primary key
    QSqlIndex primaryKey = m_database.primaryIndex(table);
    if (!primaryKey.isEmpty()) {
        QStringList pkCols;
        for (int i = 0; i < primaryKey.count(); ++i) {
            pkCols.append(primaryKey.fieldName(i));
        }
        schema += QString("\nPrimary Key: %1\n").arg(pkCols.join(", "));
    }
    
    return schema;
}

QStringList DatabaseToolWidget::getStoredProcedures() {
    // Not all databases support this the same way
    return {};
}

QStringList DatabaseToolWidget::getViews() {
    if (!isConnected()) return {};
    return m_database.tables(QSql::Views);
}

// =============================================================================
// History
// =============================================================================

QVector<QueryHistoryEntry> DatabaseToolWidget::getHistory() const {
    return m_history;
}

void DatabaseToolWidget::clearHistory() {
    m_history.clear();
    m_historyTree->clear();
    saveHistory();
}

void DatabaseToolWidget::addToFavorites(const QString& query, const QString& name) {
    m_favorites[name] = query;
    
    QTreeWidgetItem* item = new QTreeWidgetItem(m_favoritesTree);
    item->setText(0, name);
    item->setText(1, query.left(100));
    
    m_settings->beginGroup("DatabaseFavorites");
    m_settings->setValue(name, query);
    m_settings->endGroup();
}

// =============================================================================
// Connection Profiles
// =============================================================================

void DatabaseToolWidget::saveConnection(const DatabaseConnection& conn) {
    // Update or add
    bool found = false;
    for (int i = 0; i < m_connections.size(); ++i) {
        if (m_connections[i].name == conn.name) {
            m_connections[i] = conn;
            found = true;
            break;
        }
    }
    
    if (!found) {
        m_connections.append(conn);
    }
    
    // Save to settings
    m_settings->beginWriteArray("DatabaseConnections");
    for (int i = 0; i < m_connections.size(); ++i) {
        m_settings->setArrayIndex(i);
        QJsonDocument doc(m_connections[i].toJson());
        m_settings->setValue("connection", doc.toJson(QJsonDocument::Compact));
    }
    m_settings->endArray();
}

void DatabaseToolWidget::deleteConnection(const QString& name) {
    for (int i = 0; i < m_connections.size(); ++i) {
        if (m_connections[i].name == name) {
            m_connections.removeAt(i);
            break;
        }
    }
    
    // Save updated list
    m_settings->beginWriteArray("DatabaseConnections");
    for (int i = 0; i < m_connections.size(); ++i) {
        m_settings->setArrayIndex(i);
        QJsonDocument doc(m_connections[i].toJson());
        m_settings->setValue("connection", doc.toJson(QJsonDocument::Compact));
    }
    m_settings->endArray();
    
    loadConnections();
}

void DatabaseToolWidget::loadConnections() {
    m_connections.clear();
    m_connectionCombo->clear();
    
    int count = m_settings->beginReadArray("DatabaseConnections");
    for (int i = 0; i < count; ++i) {
        m_settings->setArrayIndex(i);
        QJsonObject obj = QJsonDocument::fromJson(
            m_settings->value("connection").toByteArray()).object();
        if (!obj.isEmpty()) {
            m_connections.append(DatabaseConnection::fromJson(obj));
        }
    }
    m_settings->endArray();
    
    for (const DatabaseConnection& conn : m_connections) {
        m_connectionCombo->addItem(QString("%1 (%2)").arg(conn.name, conn.driverDisplayName()), 
                                   conn.name);
    }
}

// =============================================================================
// Slots
// =============================================================================

void DatabaseToolWidget::onConnectClicked() {
    int index = m_connectionCombo->currentIndex();
    if (index < 0 || index >= m_connections.size()) {
        QMessageBox::warning(this, "No Connection", "Please select or create a connection first.");
        return;
    }
    
    DatabaseConnection conn = m_connections[index];
    
    // If password not saved, prompt for it
    if (!conn.savePassword || conn.password.isEmpty()) {
        if (conn.driver != "QSQLITE") {
            bool ok;
            QString password = QInputDialog::getText(this, "Password Required",
                QString("Enter password for %1:").arg(conn.username),
                QLineEdit::Password, QString(), &ok);
            if (!ok) return;
            conn.password = password;
        }
    }
    
    connectToDatabase(conn);
}

void DatabaseToolWidget::onDisconnectClicked() {
    disconnectFromDatabase();
}

void DatabaseToolWidget::onExecuteQuery() {
    QString sql = m_queryEditor->toPlainText().trimmed();
    if (sql.isEmpty()) {
        m_messagesEdit->append("No query to execute.");
        return;
    }
    
    // Get selected text if any
    QTextCursor cursor = m_queryEditor->textCursor();
    if (cursor.hasSelection()) {
        sql = cursor.selectedText().trimmed();
        // QTextCursor uses paragraph separators, convert to newlines
        sql.replace(QChar::ParagraphSeparator, '\n');
    }
    
    executeQuery(sql);
}

void DatabaseToolWidget::onExplainQuery() {
    QString sql = m_queryEditor->toPlainText().trimmed();
    if (sql.isEmpty()) return;
    
    // Prefix with EXPLAIN
    QString explainSql;
    if (m_database.driverName() == "QSQLITE") {
        explainSql = "EXPLAIN QUERY PLAN " + sql;
    } else if (m_database.driverName() == "QPSQL") {
        explainSql = "EXPLAIN ANALYZE " + sql;
    } else {
        explainSql = "EXPLAIN " + sql;
    }
    
    executeQuery(explainSql);
}

void DatabaseToolWidget::onStopQuery() {
    cancelQuery();
}

void DatabaseToolWidget::onSchemaItemDoubleClicked(QTreeWidgetItem* item, int column) {
    Q_UNUSED(column)
    
    QString type = item->data(0, Qt::UserRole).toString();
    QString name = item->data(0, Qt::UserRole + 1).toString();
    
    if (type == "table") {
        m_queryEditor->setPlainText(QString("SELECT * FROM %1 LIMIT 100;").arg(name));
    } else if (type == "column") {
        QString table = item->parent()->data(0, Qt::UserRole + 1).toString();
        m_queryEditor->insertPlainText(name);
    }
}

void DatabaseToolWidget::onHistoryItemDoubleClicked(QTreeWidgetItem* item, int column) {
    Q_UNUSED(column)
    
    QString query = item->data(0, Qt::UserRole).toString();
    m_queryEditor->setPlainText(query);
}

void DatabaseToolWidget::onExportResults() {
    if (m_resultsTable->rowCount() == 0) {
        QMessageBox::information(this, "No Results", "No results to export.");
        return;
    }
    
    QString path = QFileDialog::getSaveFileName(this, "Export Results",
        "results.csv", "CSV Files (*.csv);;JSON Files (*.json);;All Files (*)");
    if (path.isEmpty()) return;
    
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Export Failed", "Could not open file for writing.");
        return;
    }
    
    QTextStream out(&file);
    
    if (path.endsWith(".json")) {
        // Export as JSON
        QJsonArray rows;
        for (int row = 0; row < m_resultsTable->rowCount(); ++row) {
            QJsonObject rowObj;
            for (int col = 0; col < m_resultsTable->columnCount(); ++col) {
                QString colName = m_resultsTable->horizontalHeaderItem(col)->text();
                QTableWidgetItem* item = m_resultsTable->item(row, col);
                rowObj[colName] = item ? item->text() : "";
            }
            rows.append(rowObj);
        }
        out << QJsonDocument(rows).toJson(QJsonDocument::Indented);
    } else {
        // Export as CSV
        // Header
        QStringList header;
        for (int col = 0; col < m_resultsTable->columnCount(); ++col) {
            header.append(QString("\"%1\"").arg(m_resultsTable->horizontalHeaderItem(col)->text()));
        }
        out << header.join(",") << "\n";
        
        // Data
        for (int row = 0; row < m_resultsTable->rowCount(); ++row) {
            QStringList cols;
            for (int col = 0; col < m_resultsTable->columnCount(); ++col) {
                QTableWidgetItem* item = m_resultsTable->item(row, col);
                QString value = item ? item->text() : "";
                value.replace("\"", "\"\"");  // Escape quotes
                cols.append(QString("\"%1\"").arg(value));
            }
            out << cols.join(",") << "\n";
        }
    }
    
    file.close();
    m_messagesEdit->append(QString("Results exported to %1").arg(path));
}

void DatabaseToolWidget::refreshSchema() {
    m_schemaTree->clear();
    
    if (!isConnected()) return;
    
    // Tables
    QTreeWidgetItem* tablesItem = new QTreeWidgetItem(m_schemaTree);
    tablesItem->setText(0, "Tables");
    tablesItem->setExpanded(true);
    
    for (const QString& table : getTables()) {
        QTreeWidgetItem* tableItem = new QTreeWidgetItem(tablesItem);
        tableItem->setText(0, table);
        tableItem->setData(0, Qt::UserRole, "table");
        tableItem->setData(0, Qt::UserRole + 1, table);
        
        // Add columns
        for (const QString& column : getColumns(table)) {
            QTreeWidgetItem* colItem = new QTreeWidgetItem(tableItem);
            colItem->setText(0, column);
            colItem->setData(0, Qt::UserRole, "column");
            colItem->setData(0, Qt::UserRole + 1, column);
        }
    }
    
    // Views
    QStringList views = getViews();
    if (!views.isEmpty()) {
        QTreeWidgetItem* viewsItem = new QTreeWidgetItem(m_schemaTree);
        viewsItem->setText(0, "Views");
        
        for (const QString& view : views) {
            QTreeWidgetItem* viewItem = new QTreeWidgetItem(viewsItem);
            viewItem->setText(0, view);
            viewItem->setData(0, Qt::UserRole, "view");
            viewItem->setData(0, Qt::UserRole + 1, view);
        }
    }
    
    emit schemaChanged();
}

void DatabaseToolWidget::onConnectionSelected(int index) {
    Q_UNUSED(index)
    updateConnectionUI();
}

void DatabaseToolWidget::onTableSelected(const QString& table) {
    Q_UNUSED(table)
    // Could show table preview
}

void DatabaseToolWidget::updateConnectionUI() {
    bool connected = isConnected();
    
    m_connectBtn->setEnabled(!connected && m_connectionCombo->count() > 0);
    m_disconnectBtn->setEnabled(connected);
    m_executeBtn->setEnabled(connected);
    m_explainBtn->setEnabled(connected);
}

// =============================================================================
// Private Methods
// =============================================================================

void DatabaseToolWidget::displayResults(const QueryResult& result) {
    m_resultsTable->clear();
    m_resultsTable->setRowCount(0);
    m_resultsTable->setColumnCount(0);
    
    if (!result.success || result.columns.isEmpty()) {
        m_resultsStatusLabel->setText(result.success ? 
            QString("%1 row(s) affected").arg(result.rowsAffected) :
            QString("Error: %1").arg(result.error));
        return;
    }
    
    // Set columns
    m_resultsTable->setColumnCount(result.columns.size());
    m_resultsTable->setHorizontalHeaderLabels(result.columns);
    
    // Set rows
    m_resultsTable->setRowCount(result.rows.size());
    for (int row = 0; row < result.rows.size(); ++row) {
        for (int col = 0; col < result.rows[row].size(); ++col) {
            QTableWidgetItem* item = new QTableWidgetItem();
            QVariant value = result.rows[row][col];
            
            if (value.isNull()) {
                item->setText("NULL");
                item->setForeground(Qt::gray);
            } else {
                item->setText(value.toString());
            }
            
            m_resultsTable->setItem(row, col, item);
        }
    }
    
    m_resultsStatusLabel->setText(QString("%1 row(s) returned in %2 ms")
        .arg(result.rows.size()).arg(result.executionTimeMs));
    
    m_bottomTabs->setCurrentIndex(0);  // Results tab
}

void DatabaseToolWidget::addToHistory(const QString& query, const QueryResult& result) {
    QueryHistoryEntry entry;
    entry.query = query;
    entry.connection = m_currentConnectionName;
    entry.timestamp = QDateTime::currentDateTime();
    entry.success = result.success;
    entry.executionTimeMs = result.executionTimeMs;
    entry.rowsAffected = result.rowsAffected;
    
    m_history.prepend(entry);
    
    // Limit history size
    while (m_history.size() > 100) {
        m_history.removeLast();
    }
    
    // Update tree
    QTreeWidgetItem* item = new QTreeWidgetItem();
    item->setText(0, query.left(100).replace('\n', ' '));
    item->setText(1, entry.timestamp.toString("yyyy-MM-dd hh:mm:ss"));
    item->setText(2, QString("%1 ms").arg(entry.executionTimeMs));
    item->setText(3, entry.success ? "OK" : "Error");
    item->setData(0, Qt::UserRole, query);
    item->setForeground(3, entry.success ? QColor("#4ec9b0") : QColor("#f44747"));
    
    m_historyTree->insertTopLevelItem(0, item);
    
    saveHistory();
}

void DatabaseToolWidget::loadHistory() {
    m_history.clear();
    m_historyTree->clear();
    
    int count = m_settings->beginReadArray("DatabaseHistory");
    for (int i = 0; i < count; ++i) {
        m_settings->setArrayIndex(i);
        QueryHistoryEntry entry;
        entry.query = m_settings->value("query").toString();
        entry.connection = m_settings->value("connection").toString();
        entry.timestamp = m_settings->value("timestamp").toDateTime();
        entry.success = m_settings->value("success").toBool();
        entry.executionTimeMs = m_settings->value("executionTime").toLongLong();
        
        m_history.append(entry);
        
        QTreeWidgetItem* item = new QTreeWidgetItem(m_historyTree);
        item->setText(0, entry.query.left(100).replace('\n', ' '));
        item->setText(1, entry.timestamp.toString("yyyy-MM-dd hh:mm:ss"));
        item->setText(2, QString("%1 ms").arg(entry.executionTimeMs));
        item->setText(3, entry.success ? "OK" : "Error");
        item->setData(0, Qt::UserRole, entry.query);
    }
    m_settings->endArray();
    
    // Load favorites
    m_settings->beginGroup("DatabaseFavorites");
    for (const QString& key : m_settings->childKeys()) {
        QString query = m_settings->value(key).toString();
        m_favorites[key] = query;
        
        QTreeWidgetItem* item = new QTreeWidgetItem(m_favoritesTree);
        item->setText(0, key);
        item->setText(1, query.left(100));
    }
    m_settings->endGroup();
}

void DatabaseToolWidget::saveHistory() {
    m_settings->beginWriteArray("DatabaseHistory");
    for (int i = 0; i < m_history.size(); ++i) {
        m_settings->setArrayIndex(i);
        m_settings->setValue("query", m_history[i].query);
        m_settings->setValue("connection", m_history[i].connection);
        m_settings->setValue("timestamp", m_history[i].timestamp);
        m_settings->setValue("success", m_history[i].success);
        m_settings->setValue("executionTime", m_history[i].executionTimeMs);
    }
    m_settings->endArray();
}

// =============================================================================
// ConnectionEditDialog Implementation
// =============================================================================

ConnectionEditDialog::ConnectionEditDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUI();
    setWindowTitle("Database Connection");
    resize(450, 400);
}

void ConnectionEditDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    QFormLayout* formLayout = new QFormLayout();
    
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText("My Database");
    formLayout->addRow("Connection Name:", m_nameEdit);
    
    m_driverCombo = new QComboBox(this);
    m_driverCombo->addItem("SQLite", "QSQLITE");
    m_driverCombo->addItem("PostgreSQL", "QPSQL");
    m_driverCombo->addItem("MySQL", "QMYSQL");
    m_driverCombo->addItem("ODBC", "QODBC");
    connect(m_driverCombo, &QComboBox::currentTextChanged, this, &ConnectionEditDialog::onDriverChanged);
    formLayout->addRow("Database Type:", m_driverCombo);
    
    m_hostEdit = new QLineEdit(this);
    m_hostEdit->setPlaceholderText("localhost");
    formLayout->addRow("Host:", m_hostEdit);
    
    m_portSpin = new QSpinBox(this);
    m_portSpin->setRange(0, 65535);
    m_portSpin->setSpecialValueText("Default");
    formLayout->addRow("Port:", m_portSpin);
    
    m_databaseEdit = new QLineEdit(this);
    m_databaseEdit->setPlaceholderText("database_name or file path");
    formLayout->addRow("Database:", m_databaseEdit);
    
    QPushButton* browseBtn = new QPushButton("Browse...", this);
    connect(browseBtn, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, "Select Database File",
            QString(), "SQLite Files (*.db *.sqlite *.sqlite3);;All Files (*)");
        if (!path.isEmpty()) {
            m_databaseEdit->setText(path);
        }
    });
    formLayout->addRow("", browseBtn);
    
    m_usernameEdit = new QLineEdit(this);
    formLayout->addRow("Username:", m_usernameEdit);
    
    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    formLayout->addRow("Password:", m_passwordEdit);
    
    m_savePasswordCheck = new QCheckBox("Save password (not recommended)", this);
    formLayout->addRow("", m_savePasswordCheck);
    
    mainLayout->addLayout(formLayout);
    
    // Advanced options
    QGroupBox* advancedGroup = new QGroupBox("Advanced Options", this);
    QVBoxLayout* advLayout = new QVBoxLayout(advancedGroup);
    
    advLayout->addWidget(new QLabel("Connection String Options:"));
    m_connectionStringEdit = new QTextEdit(this);
    m_connectionStringEdit->setMaximumHeight(60);
    m_connectionStringEdit->setPlaceholderText("e.g., connect_timeout=10;application_name=RawrXD");
    advLayout->addWidget(m_connectionStringEdit);
    
    mainLayout->addWidget(advancedGroup);
    
    // Test connection
    QHBoxLayout* testLayout = new QHBoxLayout();
    m_testBtn = new QPushButton("Test Connection", this);
    connect(m_testBtn, &QPushButton::clicked, this, &ConnectionEditDialog::onTestConnection);
    testLayout->addWidget(m_testBtn);
    
    m_testResultLabel = new QLabel(this);
    testLayout->addWidget(m_testResultLabel, 1);
    
    mainLayout->addLayout(testLayout);
    
    // Buttons
    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_okBtn = buttons->button(QDialogButtonBox::Ok);
    m_okBtn->setEnabled(false);
    
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    
    mainLayout->addWidget(buttons);
    
    // Validation
    connect(m_nameEdit, &QLineEdit::textChanged, this, &ConnectionEditDialog::validateInput);
    connect(m_databaseEdit, &QLineEdit::textChanged, this, &ConnectionEditDialog::validateInput);
    
    onDriverChanged(m_driverCombo->currentText());
}

void ConnectionEditDialog::setConnection(const DatabaseConnection& conn) {
    m_nameEdit->setText(conn.name);
    m_driverCombo->setCurrentIndex(m_driverCombo->findData(conn.driver));
    m_hostEdit->setText(conn.host);
    m_portSpin->setValue(conn.port);
    m_databaseEdit->setText(conn.database);
    m_usernameEdit->setText(conn.username);
    m_passwordEdit->setText(conn.password);
    m_savePasswordCheck->setChecked(conn.savePassword);
    m_connectionStringEdit->setPlainText(conn.connectionString);
    
    validateInput();
}

DatabaseConnection ConnectionEditDialog::getConnection() const {
    DatabaseConnection conn;
    conn.name = m_nameEdit->text().trimmed();
    conn.driver = m_driverCombo->currentData().toString();
    conn.host = m_hostEdit->text().trimmed();
    conn.port = m_portSpin->value();
    conn.database = m_databaseEdit->text().trimmed();
    conn.username = m_usernameEdit->text().trimmed();
    conn.password = m_passwordEdit->text();
    conn.savePassword = m_savePasswordCheck->isChecked();
    conn.connectionString = m_connectionStringEdit->toPlainText().trimmed();
    
    return conn;
}

void ConnectionEditDialog::onDriverChanged(const QString& driver) {
    bool isSqlite = m_driverCombo->currentData().toString() == "QSQLITE";
    
    m_hostEdit->setEnabled(!isSqlite);
    m_portSpin->setEnabled(!isSqlite);
    m_usernameEdit->setEnabled(!isSqlite);
    m_passwordEdit->setEnabled(!isSqlite);
    m_savePasswordCheck->setEnabled(!isSqlite);
    
    // Set default port
    DatabaseConnection temp;
    temp.driver = m_driverCombo->currentData().toString();
    if (m_portSpin->value() == 0) {
        m_portSpin->setValue(temp.defaultPort());
    }
    
    Q_UNUSED(driver)
}

void ConnectionEditDialog::onTestConnection() {
    m_testResultLabel->setText("Testing...");
    m_testResultLabel->setStyleSheet("");
    QApplication::processEvents();
    
    DatabaseConnection conn = getConnection();
    
    QString testConnName = "test_connection_" + QString::number(QRandomGenerator::global()->generate());
    QSqlDatabase db = QSqlDatabase::addDatabase(conn.driver, testConnName);
    
    if (conn.driver == "QSQLITE") {
        db.setDatabaseName(conn.database);
    } else {
        db.setHostName(conn.host);
        db.setPort(conn.port);
        db.setDatabaseName(conn.database);
        db.setUserName(conn.username);
        db.setPassword(conn.password);
    }
    
    if (db.open()) {
        m_testResultLabel->setText("✓ Connection successful!");
        m_testResultLabel->setStyleSheet("color: #4ec9b0;");
        db.close();
    } else {
        m_testResultLabel->setText("✗ " + db.lastError().text());
        m_testResultLabel->setStyleSheet("color: #f44747;");
    }
    
    QSqlDatabase::removeDatabase(testConnName);
}

void ConnectionEditDialog::validateInput() {
    bool valid = !m_nameEdit->text().trimmed().isEmpty() &&
                 !m_databaseEdit->text().trimmed().isEmpty();
    m_okBtn->setEnabled(valid);
}
