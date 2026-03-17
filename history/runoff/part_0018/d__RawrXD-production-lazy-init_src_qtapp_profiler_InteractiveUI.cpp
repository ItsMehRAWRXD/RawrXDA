#include "InteractiveUI.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QDebug>
#include <algorithm>
#include <QMessageBox>
#include <QFileDialog>
#include "ReportExporter.h"

namespace RawrXD {

ProfilingInteractiveUI::ProfilingInteractiveUI(QWidget *parent)
    : QWidget(parent),
      m_session(nullptr),
      m_callGraph(nullptr),
      m_memAnalyzer(nullptr),
      m_comparisonSession(nullptr),
      m_comparisonMode(false),
      m_filterType(0),
      m_sortType(0),
      m_sortAscending(false),
      m_minTimeUs(0),
      m_rangeStartUs(0),
      m_rangeEndUs(ULLONG_MAX) {
    setupUi();
}

void ProfilingInteractiveUI::setupUi() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Filter toolbar
    QGroupBox *filterGroup = new QGroupBox("Filters & Search", this);
    QHBoxLayout *filterLayout = new QHBoxLayout(filterGroup);

    m_searchBox = new QLineEdit(this);
    m_searchBox->setPlaceholderText("Search function name...");
    connect(m_searchBox, &QLineEdit::textChanged, this, &ProfilingInteractiveUI::onSearchTextChanged);
    filterLayout->addWidget(new QLabel("Search:"), 0);
    filterLayout->addWidget(m_searchBox, 1);

    m_filterTypeCombo = new QComboBox(this);
    m_filterTypeCombo->addItems({"By Name", "By Time", "By Memory"});
    connect(m_filterTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ProfilingInteractiveUI::onFilterTypeChanged);
    filterLayout->addWidget(new QLabel("Type:"), 0);
    filterLayout->addWidget(m_filterTypeCombo, 0);

    m_sortCombo = new QComboBox(this);
    m_sortCombo->addItems({"Total Time", "Self Time", "Calls", "Name"});
    connect(m_sortCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ProfilingInteractiveUI::onSortOptionChanged);
    filterLayout->addWidget(new QLabel("Sort:"), 0);
    filterLayout->addWidget(m_sortCombo, 0);

    m_minTimeSpinBox = new QSpinBox(this);
    m_minTimeSpinBox->setRange(0, 1000000);
    m_minTimeSpinBox->setValue(0);
    m_minTimeSpinBox->setSuffix(" µs");
    filterLayout->addWidget(new QLabel("Min Time:"), 0);
    filterLayout->addWidget(m_minTimeSpinBox, 0);

    QPushButton *applyBtn = new QPushButton("Apply", this);
    connect(applyBtn, &QPushButton::clicked, this, &ProfilingInteractiveUI::applyFilters);
    filterLayout->addWidget(applyBtn, 0);

    mainLayout->addWidget(filterGroup);

    // Time range selector
    QGroupBox *timeGroup = new QGroupBox("Time Range", this);
    QHBoxLayout *timeLayout = new QHBoxLayout(timeGroup);

    m_startTimeEdit = new QDateTimeEdit(this);
    m_startTimeEdit->setDateTime(QDateTime::currentDateTime());
    timeLayout->addWidget(new QLabel("From:"), 0);
    timeLayout->addWidget(m_startTimeEdit, 1);

    m_endTimeEdit = new QDateTimeEdit(this);
    m_endTimeEdit->setDateTime(QDateTime::currentDateTime().addSecs(3600));
    timeLayout->addWidget(new QLabel("To:"), 0);
    timeLayout->addWidget(m_endTimeEdit, 1);

    m_applyTimeRangeBtn = new QPushButton("Apply Range", this);
    connect(m_applyTimeRangeBtn, &QPushButton::clicked, this, &ProfilingInteractiveUI::onApplyTimeRange);
    timeLayout->addWidget(m_applyTimeRangeBtn, 0);

    mainLayout->addWidget(timeGroup);

    // Comparison and export toolbar
    QGroupBox *toolGroup = new QGroupBox("Tools", this);
    QHBoxLayout *toolLayout = new QHBoxLayout(toolGroup);

    m_comparisonModeBtn = new QPushButton("Enable Comparison Mode", this);
    m_comparisonModeBtn->setCheckable(true);
    connect(m_comparisonModeBtn, &QPushButton::toggled, this, &ProfilingInteractiveUI::onComparisonToggled);
    toolLayout->addWidget(m_comparisonModeBtn, 0);

    m_comparisonLabel = new QLabel("No comparison active", this);
    toolLayout->addWidget(m_comparisonLabel, 1);

    m_exportBtn = new QPushButton("Export Report", this);
    connect(m_exportBtn, &QPushButton::clicked, this, &ProfilingInteractiveUI::onExportReport);
    toolLayout->addWidget(m_exportBtn, 0);

    mainLayout->addWidget(toolGroup);

    // Stats display
    m_statsLabel = new QLabel("No profiling data loaded", this);
    mainLayout->addWidget(m_statsLabel);

    // Function table
    QGroupBox *funcGroup = new QGroupBox("Function Statistics", this);
    QVBoxLayout *funcLayout = new QVBoxLayout(funcGroup);

    m_functionTable = new QTableWidget(this);
    m_functionTable->setColumnCount(7);
    m_functionTable->setHorizontalHeaderLabels(
        {"Function", "Total Time", "Self Time", "Calls", "Avg Time", "Min Time", "Max Time"});
    m_functionTable->horizontalHeader()->setStretchLastSection(true);
    m_functionTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_functionTable->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_functionTable, &QTableWidget::cellDoubleClicked, this, &ProfilingInteractiveUI::onTableRowDoubleClicked);
    funcLayout->addWidget(m_functionTable);

    mainLayout->addWidget(funcGroup);

    // Call graph table
    QGroupBox *callGroup = new QGroupBox("Call Graph", this);
    QVBoxLayout *callLayout = new QVBoxLayout(callGroup);

    m_callGraphTable = new QTableWidget(this);
    m_callGraphTable->setColumnCount(5);
    m_callGraphTable->setHorizontalHeaderLabels({"Caller", "Callee", "Calls", "Total Time", "Avg Time"});
    m_callGraphTable->horizontalHeader()->setStretchLastSection(true);
    callLayout->addWidget(m_callGraphTable);

    mainLayout->addWidget(callGroup);

    // Memory table
    QGroupBox *memGroup = new QGroupBox("Memory Analysis", this);
    QVBoxLayout *memLayout = new QVBoxLayout(memGroup);

    m_memoryTable = new QTableWidget(this);
    m_memoryTable->setColumnCount(5);
    m_memoryTable->setHorizontalHeaderLabels({"Function", "Total Allocated", "Count", "Avg Size", "Peak Size"});
    m_memoryTable->horizontalHeader()->setStretchLastSection(true);
    memLayout->addWidget(m_memoryTable);

    mainLayout->addWidget(memGroup);

    setLayout(mainLayout);
}

void ProfilingInteractiveUI::attachSession(ProfileSession *session) {
    m_session = session;
    updateFunctionTable();

    if (m_session) {
        m_statsLabel->setText(
            QString("Runtime: %1 s | Functions: %2 | Samples: %3")
                .arg(m_session->totalRuntimeUs() / 1000000.0, 0, 'f', 2)
                .arg(m_session->functionStats().size())
                .arg(m_session->callStacks().size()));
    }
}

void ProfilingInteractiveUI::attachCallGraph(CallGraph *callGraph) {
    m_callGraph = callGraph;
    updateCallGraphDisplay();
}

void ProfilingInteractiveUI::attachMemoryAnalyzer(MemoryAnalyzer *memAnalyzer) {
    m_memAnalyzer = memAnalyzer;
    updateMemoryDisplay();
}

void ProfilingInteractiveUI::onSearchTextChanged(const QString &text) {
    m_searchPattern = text;
}

void ProfilingInteractiveUI::onFilterTypeChanged(int index) {
    m_filterType = index;
}

void ProfilingInteractiveUI::onSortOptionChanged(int index) {
    m_sortType = index;
}

void ProfilingInteractiveUI::applyFilters() {
    updateFunctionTable();
    emit filterApplied(m_searchPattern);
}

void ProfilingInteractiveUI::onTableRowDoubleClicked(int row, int column) {
    Q_UNUSED(column);
    if (row >= 0 && row < m_functionTable->rowCount()) {
        QString funcName = m_functionTable->item(row, 0)->text();
        drillDownFunction(funcName);
    }
}

void ProfilingInteractiveUI::onApplyTimeRange() {
    // Update time range and refresh tables
    m_rangeStartUs = m_startTimeEdit->dateTime().toMSecsSinceEpoch() * 1000;
    m_rangeEndUs = m_endTimeEdit->dateTime().toMSecsSinceEpoch() * 1000;
    updateFunctionTable();
}

void ProfilingInteractiveUI::onComparisonToggled(bool checked) {
    if (checked && !m_comparisonMode) {
        QString filePath = QFileDialog::getOpenFileName(this, "Select Comparison Profile", "", "JSON Files (*.json)");
        if (!filePath.isEmpty()) {
            // In a full implementation, load the profile from file
            m_comparisonMode = true;
            m_comparisonLabel->setText("Comparison mode: Loaded profile");
            updateComparisonView();
            emit comparisonModeChanged(true);
        } else {
            m_comparisonModeBtn->setChecked(false);
        }
    } else {
        m_comparisonMode = false;
        m_comparisonSession = nullptr;
        m_comparisonLabel->setText("Comparison mode: Off");
        updateFunctionTable();
        emit comparisonModeChanged(false);
    }
}

void ProfilingInteractiveUI::onExportReport() {
    QString filePath = QFileDialog::getSaveFileName(this, "Export Profiling Report", "", "HTML (*.html);;CSV (*.csv);;PDF (*.pdf);;JSON (*.json)");
    if (!filePath.isEmpty()) {
        ProfilingReportExporter exporter(m_session, m_callGraph, m_memAnalyzer);

        if (filePath.endsWith(".html", Qt::CaseInsensitive)) {
            QString html = exporter.exportToHTML("Profiling Report", true, true, true);
            QFile file(filePath);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream ts(&file);
                ts << html;
                file.close();
                QMessageBox::information(this, "Export Successful", "Report exported to " + filePath);
            }
        } else if (filePath.endsWith(".csv", Qt::CaseInsensitive)) {
            QString csv = exporter.exportToCSV();
            QFile file(filePath);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream ts(&file);
                ts << csv;
                file.close();
                QMessageBox::information(this, "Export Successful", "Report exported to " + filePath);
            }
        } else if (filePath.endsWith(".json", Qt::CaseInsensitive)) {
            QString json = exporter.exportToJSON();
            QFile file(filePath);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream ts(&file);
                ts << json;
                file.close();
                QMessageBox::information(this, "Export Successful", "Report exported to " + filePath);
            }
        } else if (filePath.endsWith(".pdf", Qt::CaseInsensitive)) {
            if (exporter.exportToPDF(filePath, "Profiling Report")) {
                QMessageBox::information(this, "Export Successful", "PDF exported to " + filePath);
            } else {
                QMessageBox::warning(this, "Export Failed", "Could not export to PDF. Check if wkhtmltopdf is installed.");
            }
        }
    }
}

void ProfilingInteractiveUI::updateFunctionTable() {
    if (!m_session) return;

    m_functionTable->setRowCount(0);

    auto functions = getFilteredFunctions();

    for (int i = 0; i < functions.size(); ++i) {
        const auto &func = functions[i];

        m_functionTable->insertRow(i);
        m_functionTable->setItem(i, 0, new QTableWidgetItem(func.functionName));
        m_functionTable->setItem(i, 1, new QTableWidgetItem(formatTime(func.totalTimeUs)));
        m_functionTable->setItem(i, 2, new QTableWidgetItem(formatTime(func.selfTimeUs)));
        m_functionTable->setItem(i, 3, new QTableWidgetItem(QString::number(func.callCount)));
        m_functionTable->setItem(i, 4, new QTableWidgetItem(formatTime(func.totalTimeUs / func.callCount)));
        m_functionTable->setItem(i, 5, new QTableWidgetItem(formatTime(0)));  // Min placeholder
        m_functionTable->setItem(i, 6, new QTableWidgetItem(formatTime(0)));  // Max placeholder
    }
}

void ProfilingInteractiveUI::updateCallGraphDisplay() {
    if (!m_callGraph) return;

    m_callGraphTable->setRowCount(0);

    auto edges = m_callGraph->getCallEdges();
    for (int i = 0; i < edges.size() && i < 100; ++i) {  // Top 100 edges
        const auto &edge = edges[i];

        m_callGraphTable->insertRow(i);
        m_callGraphTable->setItem(i, 0, new QTableWidgetItem(edge.caller));
        m_callGraphTable->setItem(i, 1, new QTableWidgetItem(edge.callee));
        m_callGraphTable->setItem(i, 2, new QTableWidgetItem(QString::number(edge.callCount)));
        m_callGraphTable->setItem(i, 3, new QTableWidgetItem(formatTime(edge.totalTimeUs)));
        m_callGraphTable->setItem(i, 4, new QTableWidgetItem(formatTime(edge.averageTimeUs)));
    }
}

void ProfilingInteractiveUI::updateMemoryDisplay() {
    if (!m_memAnalyzer) return;

    m_memoryTable->setRowCount(0);

    auto hotspots = m_memAnalyzer->getAllocationHotspots(50);
    for (int i = 0; i < hotspots.size(); ++i) {
        const auto &hs = hotspots[i];

        m_memoryTable->insertRow(i);
        m_memoryTable->setItem(i, 0, new QTableWidgetItem(hs.functionName));
        m_memoryTable->setItem(i, 1, new QTableWidgetItem(formatMemory(hs.totalAllocatedBytes)));
        m_memoryTable->setItem(i, 2, new QTableWidgetItem(QString::number(hs.allocationCount)));
        m_memoryTable->setItem(i, 3, new QTableWidgetItem(formatMemory(hs.averageAllocationBytes)));
        m_memoryTable->setItem(i, 4, new QTableWidgetItem(formatMemory(hs.peakAllocationBytes)));
    }
}

void ProfilingInteractiveUI::updateComparisonView() {
    // Placeholder for comparison visualization
    if (m_comparisonMode && m_comparisonSession) {
        m_comparisonLabel->setText("Comparing profiles...");
    }
}

QList<FunctionStat> ProfilingInteractiveUI::getFilteredFunctions() {
    if (!m_session) return {};

    QList<FunctionStat> result = m_session->functionStats().values();

    // Apply search filter
    if (!m_searchPattern.isEmpty()) {
        result.erase(std::remove_if(result.begin(), result.end(), [this](const FunctionStat &f) {
            return !f.functionName.contains(m_searchPattern, Qt::CaseInsensitive);
        }), result.end());
    }

    // Apply time filter
    if (m_minTimeUs > 0) {
        result.erase(std::remove_if(result.begin(), result.end(), [this](const FunctionStat &f) {
            return f.totalTimeUs < m_minTimeUs;
        }), result.end());
    }

    // Sort
    std::sort(result.begin(), result.end(), [this](const FunctionStat &a, const FunctionStat &b) {
        bool asc = m_sortAscending;
        switch (m_sortType) {
        case 0:  // Total time
            return asc ? a.totalTimeUs < b.totalTimeUs : a.totalTimeUs > b.totalTimeUs;
        case 1:  // Self time
            return asc ? a.selfTimeUs < b.selfTimeUs : a.selfTimeUs > b.selfTimeUs;
        case 2:  // Calls
            return asc ? a.callCount < b.callCount : a.callCount > b.callCount;
        case 3:  // Name
            return asc ? a.functionName < b.functionName : a.functionName > b.functionName;
        default:
            return false;
        }
    });

    return result;
}

QString ProfilingInteractiveUI::formatTime(quint64 us) const {
    if (us < 1000) {
        return QString::number(us) + " µs";
    } else if (us < 1000000) {
        return QString::number(us / 1000.0, 'f', 2) + " ms";
    } else {
        return QString::number(us / 1000000.0, 'f', 2) + " s";
    }
}

QString ProfilingInteractiveUI::formatMemory(quint64 bytes) const {
    if (bytes < 1024) {
        return QString::number(bytes) + " B";
    } else if (bytes < 1024 * 1024) {
        return QString::number(bytes / 1024.0, 'f', 2) + " KB";
    } else if (bytes < 1024 * 1024 * 1024) {
        return QString::number(bytes / 1024.0 / 1024.0, 'f', 2) + " MB";
    } else {
        return QString::number(bytes / 1024.0 / 1024.0 / 1024.0, 'f', 2) + " GB";
    }
}

void ProfilingInteractiveUI::drillDownFunction(const QString &functionName) {
    m_navigationStack.append(functionName);
    emit functionSelected(functionName);

    if (m_callGraph) {
        auto callees = m_callGraph->getCalleesOf(functionName);
        m_callGraphTable->setRowCount(0);
        for (int i = 0; i < callees.size(); ++i) {
            const auto &edge = callees[i];
            m_callGraphTable->insertRow(i);
            m_callGraphTable->setItem(i, 0, new QTableWidgetItem(edge.caller));
            m_callGraphTable->setItem(i, 1, new QTableWidgetItem(edge.callee));
            m_callGraphTable->setItem(i, 2, new QTableWidgetItem(QString::number(edge.callCount)));
            m_callGraphTable->setItem(i, 3, new QTableWidgetItem(formatTime(edge.totalTimeUs)));
            m_callGraphTable->setItem(i, 4, new QTableWidgetItem(formatTime(edge.averageTimeUs)));
        }
    }
}

void ProfilingInteractiveUI::setSearchFilter(const QString &pattern) {
    m_searchBox->setText(pattern);
}

void ProfilingInteractiveUI::setTimeRange(quint64 startUs, quint64 endUs) {
    m_rangeStartUs = startUs;
    m_rangeEndUs = endUs;
}

void ProfilingInteractiveUI::setSortColumn(int column, bool ascending) {
    m_sortType = column;
    m_sortAscending = ascending;
}

QString ProfilingInteractiveUI::getSelectedFunction() const {
    if (m_navigationStack.isEmpty()) return "";
    return m_navigationStack.last();
}

void ProfilingInteractiveUI::enableComparisonMode(ProfileSession *otherSession) {
    m_comparisonSession = otherSession;
    m_comparisonMode = true;
    m_comparisonModeBtn->setChecked(true);
    m_comparisonLabel->setText("Comparison mode: Enabled");
    emit comparisonModeChanged(true);
}

void ProfilingInteractiveUI::disableComparisonMode() {
    m_comparisonSession = nullptr;
    m_comparisonMode = false;
    m_comparisonModeBtn->setChecked(false);
    m_comparisonLabel->setText("Comparison mode: Off");
    emit comparisonModeChanged(false);
}

} // namespace RawrXD
