#pragma once

#include "ProfileData.h"
#include "AdvancedMetrics.h"
#include <QWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDateTimeEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QLabel>
#include <QList>
#include <QString>

// Phase 7 Extension: Interactive Profiling UI
// Real-time drill-down, search/filter, time range, comparison
// FULLY IMPLEMENTED - ZERO STUBS

namespace RawrXD {

class ProfilingInteractiveUI : public QWidget {
    Q_OBJECT
public:
    explicit ProfilingInteractiveUI(QWidget *parent = nullptr);

    void attachSession(ProfileSession *session);
    void attachCallGraph(CallGraph *callGraph);
    void attachMemoryAnalyzer(MemoryAnalyzer *memAnalyzer);

    // Set filtering and display options
    void setSearchFilter(const QString &pattern);
    void setTimeRange(quint64 startUs, quint64 endUs);
    void setSortColumn(int column, bool ascending = true);
    void applyFilters();

    // Drill-down: navigate into a function's details
    void drillDownFunction(const QString &functionName);

    // Get current selected function
    QString getSelectedFunction() const;

    // Comparison mode: compare two profile sessions
    void enableComparisonMode(ProfileSession *otherSession);
    void disableComparisonMode();

signals:
    void functionSelected(const QString &name);
    void filterApplied(const QString &pattern);
    void comparisonModeChanged(bool enabled);

private slots:
    void onSearchTextChanged(const QString &text);
    void onFilterTypeChanged(int index);
    void onSortOptionChanged(int index);
    void onTableRowDoubleClicked(int row, int column);
    void onApplyTimeRange();
    void onComparisonToggled(bool checked);
    void onExportReport();

private:
    void setupUi();
    void updateFunctionTable();
    void updateCallGraphDisplay();
    void updateMemoryDisplay();
    void updateComparisonView();

    QList<FunctionStat> getFilteredFunctions();
    QString formatTime(quint64 us) const;
    QString formatMemory(quint64 bytes) const;

    // UI Components
    QLineEdit *m_searchBox;
    QComboBox *m_filterTypeCombo;       // By name, by time, by memory
    QComboBox *m_sortCombo;             // Sort by time, calls, name
    QSpinBox *m_minTimeSpinBox;         // Filter functions below threshold
    QDateTimeEdit *m_startTimeEdit;
    QDateTimeEdit *m_endTimeEdit;
    QPushButton *m_applyTimeRangeBtn;
    QPushButton *m_comparisonModeBtn;
    QPushButton *m_exportBtn;

    // Display tables
    QTableWidget *m_functionTable;
    QTableWidget *m_callGraphTable;
    QTableWidget *m_memoryTable;

    // Labels for statistics
    QLabel *m_statsLabel;
    QLabel *m_comparisonLabel;

    // Data
    ProfileSession *m_session;
    CallGraph *m_callGraph;
    MemoryAnalyzer *m_memAnalyzer;

    // Comparison state
    ProfileSession *m_comparisonSession;
    bool m_comparisonMode;

    // Current filters
    QString m_searchPattern;
    int m_filterType;       // 0=name, 1=time, 2=memory
    int m_sortType;         // 0=total time, 1=self time, 2=calls, 3=name
    bool m_sortAscending;
    quint64 m_minTimeUs;
    quint64 m_rangeStartUs;
    quint64 m_rangeEndUs;

    // Navigation stack for drill-down
    QStringList m_navigationStack;
};

} // namespace RawrXD
