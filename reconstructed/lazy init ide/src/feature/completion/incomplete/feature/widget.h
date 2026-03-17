#pragma once
// ═══════════════════════════════════════════════════════════════════════════════
// INCOMPLETE FEATURE COMPLETION WIDGET
// Qt GUI Widget for managing and monitoring feature completion
// ═══════════════════════════════════════════════════════════════════════════════

#ifndef INCOMPLETE_FEATURE_WIDGET_H
#define INCOMPLETE_FEATURE_WIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QProgressBar>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QSplitter>
#include <QGroupBox>
#include <QTabWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTimer>
#include <QMessageBox>
#include <QFileDialog>
#include <QMenu>
#include <QAction>
#include <QClipboard>
#include <QApplication>

#include "incomplete_feature_engine.h"

class IncompleteFeatureWidget : public QWidget {
    Q_OBJECT

public:
    explicit IncompleteFeatureWidget(QWidget* parent = nullptr);
    ~IncompleteFeatureWidget();
    
    // Set the engine
    void setEngine(IncompleteFeatureEngine* engine);
    IncompleteFeatureEngine* engine() const { return m_engine; }
    
    // Load manifest
    bool loadManifest(const QString& path);
    
    // Refresh display
    void refresh();

public slots:
    // Control slots
    void onStartClicked();
    void onStopClicked();
    void onPauseClicked();
    void onResumeClicked();
    
    // Selection slots
    void onPriorityChanged(int index);
    void onCategoryChanged(int index);
    void onSearchTextChanged(const QString& text);
    
    // Feature slots
    void onFeatureSelected(int row, int column);
    void onFeatureDoubleClicked(int row, int column);
    void onCompleteSelected();
    void onExportReport();
    void onLoadManifest();
    
    // Engine signal handlers
    void onManifestLoaded(int count);
    void onFeatureStarted(int featureId);
    void onFeatureCompleted(int featureId, bool success);
    void onFeatureProgress(int featureId, int percent);
    void onOverallProgress(double percent);
    void onErrorOccurred(int featureId, const QString& error);

signals:
    void featureSelected(int featureId);
    void completionStarted();
    void completionFinished();

private:
    void setupUI();
    void setupConnections();
    void populateTable();
    void updateStats();
    void updateFeatureDetails(int featureId);
    QString priorityToIcon(FeaturePriority priority);
    QString statusToIcon(CompletionStatus status);
    QColor priorityToColor(FeaturePriority priority);
    QColor statusToColor(CompletionStatus status);
    
    // UI Components
    QVBoxLayout* m_mainLayout;
    QSplitter* m_splitter;
    
    // Control panel
    QGroupBox* m_controlGroup;
    QPushButton* m_loadManifestBtn;
    QPushButton* m_startBtn;
    QPushButton* m_stopBtn;
    QPushButton* m_pauseBtn;
    QPushButton* m_resumeBtn;
    QPushButton* m_exportBtn;
    QSpinBox* m_concurrencySpinBox;
    
    // Filter panel
    QGroupBox* m_filterGroup;
    QComboBox* m_priorityCombo;
    QComboBox* m_categoryCombo;
    QComboBox* m_statusCombo;
    QLineEdit* m_searchEdit;
    
    // Progress panel
    QGroupBox* m_progressGroup;
    QProgressBar* m_overallProgress;
    QLabel* m_progressLabel;
    QLabel* m_criticalProgress;
    QLabel* m_highProgress;
    QLabel* m_mediumProgress;
    QLabel* m_lowProgress;
    QLabel* m_tokensLabel;
    QLabel* m_timeLabel;
    
    // Feature table
    QTableWidget* m_featureTable;
    
    // Detail panel
    QTabWidget* m_detailTabs;
    QTextEdit* m_detailText;
    QTextEdit* m_codePreview;
    QTreeWidget* m_dependencyTree;
    
    // Stats panel
    QGroupBox* m_statsGroup;
    QLabel* m_totalLabel;
    QLabel* m_completedLabel;
    QLabel* m_failedLabel;
    QLabel* m_blockedLabel;
    QLabel* m_confidenceLabel;
    QLabel* m_complexityLabel;
    
    // Engine
    IncompleteFeatureEngine* m_engine;
    
    // Timer for UI updates
    QTimer* m_updateTimer;
    
    // Current selection
    int m_selectedFeatureId;
};

#endif // INCOMPLETE_FEATURE_WIDGET_H
