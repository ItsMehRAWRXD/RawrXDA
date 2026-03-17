/**
 * @file memory_snapshot_widget.h
 * @brief Memory Snapshot Management UI with Session Persistence
 * 
 * Provides UI for managing memory snapshots with:
 * - Save/Load snapshot dialogs
 * - Snapshot browser with preview
 * - Session state visualization
 * - Diff viewer between snapshots
 * - Auto-save configuration
 * - Export/Import capabilities
 */

#pragma once

#include <QWidget>
#include <QString>
#include <QMap>
#include <QDateTime>
#include <QJsonObject>
#include "session_persistence.h"

class QListWidget;
class QListWidgetItem;
class QTextEdit;
class QPushButton;
class QLabel;
class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;
class QCheckBox;
class QSpinBox;
class QTabWidget;
class QProgressBar;
class QSplitter;

/**
 * @class MemorySnapshotWidget
 * @brief Complete UI for memory snapshot management
 */
class MemorySnapshotWidget : public QWidget {
    Q_OBJECT

public:
    explicit MemorySnapshotWidget(QWidget* parent = nullptr);
    ~MemorySnapshotWidget() override;

    void initialize();
    void setSessionPersistence(RawrXD::SessionPersistence* persistence);

public slots:
    void createSnapshot();
    void loadSnapshot();
    void deleteSnapshot();
    void exportSnapshot();
    void importSnapshot();
    void compareSnapshots();
    void onSnapshotSelected(QListWidgetItem* item);
    void refreshSnapshotList();
    void configureAutoSave();

signals:
    void snapshotCreated(const QString& snapshotId);
    void snapshotLoaded(const QString& snapshotId);
    void snapshotDeleted(const QString& snapshotId);

private:
    void setupUI();
    void createToolbar();
    void createSnapshotBrowser();
    void createDetailPanel();
    void createDiffViewer();
    
    void updateSnapshotList();
    void updateDetailView(const QString& snapshotId);
    void performDiff(const QString& snapshot1, const QString& snapshot2);
    
    struct SnapshotMetadata {
        QString id;
        QString name;
        QDateTime timestamp;
        qint64 sizeBytes;
        int sessionCount;
        QJsonObject data;
    };
    
    RawrXD::SessionPersistence* m_persistence = nullptr;
    QMap<QString, SnapshotMetadata> m_snapshots;
    QString m_currentSnapshotId;
    
    // UI Components
    QListWidget* m_snapshotList = nullptr;
    QTextEdit* m_detailView = nullptr;
    QTreeWidget* m_diffTree = nullptr;
    QPushButton* m_createBtn = nullptr;
    QPushButton* m_loadBtn = nullptr;
    QPushButton* m_deleteBtn = nullptr;
    QPushButton* m_exportBtn = nullptr;
    QPushButton* m_compareBtn = nullptr;
    QLabel* m_statusLabel = nullptr;
    QTabWidget* m_tabWidget = nullptr;
    QSplitter* m_splitter = nullptr;
    
    // Auto-save config
    QCheckBox* m_autoSaveEnabled = nullptr;
    QSpinBox* m_autoSaveInterval = nullptr;
};
