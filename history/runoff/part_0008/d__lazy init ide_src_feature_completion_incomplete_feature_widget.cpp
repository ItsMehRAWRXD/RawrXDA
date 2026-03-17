// ═══════════════════════════════════════════════════════════════════════════════
// INCOMPLETE FEATURE COMPLETION WIDGET - IMPLEMENTATION
// Qt GUI Widget for managing and monitoring feature completion
// ═══════════════════════════════════════════════════════════════════════════════

#include "incomplete_feature_widget.h"
#include <QHeaderView>
#include <QDebug>

IncompleteFeatureWidget::IncompleteFeatureWidget(QWidget* parent)
    : QWidget(parent)
    , m_engine(nullptr)
    , m_selectedFeatureId(-1)
{
    setupUI();
    setupConnections();
    
    // Start update timer
    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &IncompleteFeatureWidget::updateStats);
    m_updateTimer->start(1000);  // Update every second
}

IncompleteFeatureWidget::~IncompleteFeatureWidget()
{
    m_updateTimer->stop();
}

void IncompleteFeatureWidget::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(10, 10, 10, 10);
    m_mainLayout->setSpacing(10);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // CONTROL PANEL
    // ═══════════════════════════════════════════════════════════════════════════
    
    m_controlGroup = new QGroupBox("Controls", this);
    QHBoxLayout* controlLayout = new QHBoxLayout(m_controlGroup);
    
    m_loadManifestBtn = new QPushButton("📂 Load Manifest", this);
    m_loadManifestBtn->setToolTip("Load incomplete features from markdown manifest");
    
    m_startBtn = new QPushButton("▶️ Start", this);
    m_startBtn->setToolTip("Start completing features");
    m_startBtn->setEnabled(false);
    
    m_stopBtn = new QPushButton("⏹️ Stop", this);
    m_stopBtn->setToolTip("Stop completion");
    m_stopBtn->setEnabled(false);
    
    m_pauseBtn = new QPushButton("⏸️ Pause", this);
    m_pauseBtn->setToolTip("Pause completion");
    m_pauseBtn->setEnabled(false);
    
    m_resumeBtn = new QPushButton("▶️ Resume", this);
    m_resumeBtn->setToolTip("Resume completion");
    m_resumeBtn->setEnabled(false);
    m_resumeBtn->setVisible(false);
    
    m_exportBtn = new QPushButton("📊 Export Report", this);
    m_exportBtn->setToolTip("Export progress report");
    
    QLabel* concurrencyLabel = new QLabel("Concurrent:", this);
    m_concurrencySpinBox = new QSpinBox(this);
    m_concurrencySpinBox->setRange(1, 16);
    m_concurrencySpinBox->setValue(4);
    m_concurrencySpinBox->setToolTip("Number of concurrent completions");
    
    controlLayout->addWidget(m_loadManifestBtn);
    controlLayout->addWidget(m_startBtn);
    controlLayout->addWidget(m_stopBtn);
    controlLayout->addWidget(m_pauseBtn);
    controlLayout->addWidget(m_resumeBtn);
    controlLayout->addStretch();
    controlLayout->addWidget(concurrencyLabel);
    controlLayout->addWidget(m_concurrencySpinBox);
    controlLayout->addWidget(m_exportBtn);
    
    m_mainLayout->addWidget(m_controlGroup);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // FILTER PANEL
    // ═══════════════════════════════════════════════════════════════════════════
    
    m_filterGroup = new QGroupBox("Filters", this);
    QHBoxLayout* filterLayout = new QHBoxLayout(m_filterGroup);
    
    filterLayout->addWidget(new QLabel("Priority:", this));
    m_priorityCombo = new QComboBox(this);
    m_priorityCombo->addItem("All Priorities", -1);
    m_priorityCombo->addItem("🔴 Critical", static_cast<int>(FeaturePriority::CRITICAL));
    m_priorityCombo->addItem("🟠 High", static_cast<int>(FeaturePriority::HIGH));
    m_priorityCombo->addItem("🟡 Medium", static_cast<int>(FeaturePriority::MEDIUM));
    m_priorityCombo->addItem("🟢 Low", static_cast<int>(FeaturePriority::LOW));
    filterLayout->addWidget(m_priorityCombo);
    
    filterLayout->addWidget(new QLabel("Category:", this));
    m_categoryCombo = new QComboBox(this);
    m_categoryCombo->addItem("All Categories", -1);
    m_categoryCombo->addItem("GPU - Vulkan", static_cast<int>(FeatureCategory::GPU_VULKAN));
    m_categoryCombo->addItem("GPU - CUDA", static_cast<int>(FeatureCategory::GPU_CUDA));
    m_categoryCombo->addItem("GPU - Metal", static_cast<int>(FeatureCategory::GPU_METAL));
    m_categoryCombo->addItem("GPU - OpenCL", static_cast<int>(FeatureCategory::GPU_OPENCL));
    m_categoryCombo->addItem("GPU - SYCL", static_cast<int>(FeatureCategory::GPU_SYCL));
    m_categoryCombo->addItem("GPU - HIP", static_cast<int>(FeatureCategory::GPU_HIP));
    m_categoryCombo->addItem("GPU - CANN", static_cast<int>(FeatureCategory::GPU_CANN));
    m_categoryCombo->addItem("Model Loading", static_cast<int>(FeatureCategory::MODEL_LOADING));
    m_categoryCombo->addItem("AI Integration", static_cast<int>(FeatureCategory::AI_INTEGRATION));
    m_categoryCombo->addItem("Cloud Integration", static_cast<int>(FeatureCategory::CLOUD_INTEGRATION));
    m_categoryCombo->addItem("GGUF Server", static_cast<int>(FeatureCategory::GGUF_SERVER));
    m_categoryCombo->addItem("Agentic System", static_cast<int>(FeatureCategory::AGENTIC_SYSTEM));
    m_categoryCombo->addItem("Editor Core", static_cast<int>(FeatureCategory::EDITOR_CORE));
    m_categoryCombo->addItem("Build System", static_cast<int>(FeatureCategory::BUILD_SYSTEM));
    m_categoryCombo->addItem("GUI/UI", static_cast<int>(FeatureCategory::GUI_UI));
    m_categoryCombo->addItem("Debug System", static_cast<int>(FeatureCategory::DEBUG_SYSTEM));
    m_categoryCombo->addItem("Terminal", static_cast<int>(FeatureCategory::TERMINAL));
    m_categoryCombo->addItem("Security", static_cast<int>(FeatureCategory::SECURITY));
    m_categoryCombo->addItem("Network", static_cast<int>(FeatureCategory::NETWORK));
    m_categoryCombo->addItem("Paint/Drawing", static_cast<int>(FeatureCategory::PAINT_DRAWING));
    m_categoryCombo->addItem("Plugins", static_cast<int>(FeatureCategory::PLUGINS));
    m_categoryCombo->addItem("Git Integration", static_cast<int>(FeatureCategory::GIT_INTEGRATION));
    m_categoryCombo->addItem("Miscellaneous", static_cast<int>(FeatureCategory::MISCELLANEOUS));
    filterLayout->addWidget(m_categoryCombo);
    
    filterLayout->addWidget(new QLabel("Status:", this));
    m_statusCombo = new QComboBox(this);
    m_statusCombo->addItem("All Status", -1);
    m_statusCombo->addItem("Not Started", static_cast<int>(CompletionStatus::NOT_STARTED));
    m_statusCombo->addItem("In Progress", static_cast<int>(CompletionStatus::GENERATING));
    m_statusCombo->addItem("Completed", static_cast<int>(CompletionStatus::COMPLETED));
    m_statusCombo->addItem("Failed", static_cast<int>(CompletionStatus::FAILED));
    m_statusCombo->addItem("Blocked", static_cast<int>(CompletionStatus::BLOCKED));
    filterLayout->addWidget(m_statusCombo);
    
    filterLayout->addStretch();
    filterLayout->addWidget(new QLabel("🔍", this));
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Search features...");
    m_searchEdit->setMinimumWidth(200);
    filterLayout->addWidget(m_searchEdit);
    
    m_mainLayout->addWidget(m_filterGroup);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // PROGRESS PANEL
    // ═══════════════════════════════════════════════════════════════════════════
    
    m_progressGroup = new QGroupBox("Progress", this);
    QGridLayout* progressLayout = new QGridLayout(m_progressGroup);
    
    m_overallProgress = new QProgressBar(this);
    m_overallProgress->setRange(0, 100);
    m_overallProgress->setValue(0);
    m_overallProgress->setFormat("%p% Complete");
    m_overallProgress->setMinimumHeight(30);
    progressLayout->addWidget(m_overallProgress, 0, 0, 1, 4);
    
    m_progressLabel = new QLabel("Ready", this);
    m_progressLabel->setAlignment(Qt::AlignCenter);
    progressLayout->addWidget(m_progressLabel, 1, 0, 1, 4);
    
    m_criticalProgress = new QLabel("🔴 Critical: 0/0", this);
    m_highProgress = new QLabel("🟠 High: 0/0", this);
    m_mediumProgress = new QLabel("🟡 Medium: 0/0", this);
    m_lowProgress = new QLabel("🟢 Low: 0/0", this);
    
    progressLayout->addWidget(m_criticalProgress, 2, 0);
    progressLayout->addWidget(m_highProgress, 2, 1);
    progressLayout->addWidget(m_mediumProgress, 2, 2);
    progressLayout->addWidget(m_lowProgress, 2, 3);
    
    m_tokensLabel = new QLabel("Tokens: 0", this);
    m_timeLabel = new QLabel("Time: 0:00", this);
    progressLayout->addWidget(m_tokensLabel, 3, 0, 1, 2);
    progressLayout->addWidget(m_timeLabel, 3, 2, 1, 2);
    
    m_mainLayout->addWidget(m_progressGroup);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // MAIN SPLITTER (Table + Details)
    // ═══════════════════════════════════════════════════════════════════════════
    
    m_splitter = new QSplitter(Qt::Horizontal, this);
    
    // Feature table
    m_featureTable = new QTableWidget(this);
    m_featureTable->setColumnCount(8);
    m_featureTable->setHorizontalHeaderLabels({
        "ID", "Priority", "Category", "Status", "File", "Function", "Confidence", "Description"
    });
    m_featureTable->horizontalHeader()->setStretchLastSection(true);
    m_featureTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_featureTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_featureTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_featureTable->setAlternatingRowColors(true);
    m_featureTable->setSortingEnabled(true);
    m_featureTable->setContextMenuPolicy(Qt::CustomContextMenu);
    
    m_splitter->addWidget(m_featureTable);
    
    // Detail tabs
    m_detailTabs = new QTabWidget(this);
    
    // Detail text tab
    m_detailText = new QTextEdit(this);
    m_detailText->setReadOnly(true);
    m_detailText->setFont(QFont("Consolas", 10));
    m_detailTabs->addTab(m_detailText, "Details");
    
    // Code preview tab
    m_codePreview = new QTextEdit(this);
    m_codePreview->setReadOnly(true);
    m_codePreview->setFont(QFont("Consolas", 10));
    m_detailTabs->addTab(m_codePreview, "Generated Code");
    
    // Dependency tree tab
    m_dependencyTree = new QTreeWidget(this);
    m_dependencyTree->setHeaderLabels({"Dependencies"});
    m_detailTabs->addTab(m_dependencyTree, "Dependencies");
    
    m_splitter->addWidget(m_detailTabs);
    m_splitter->setSizes({600, 400});
    
    m_mainLayout->addWidget(m_splitter, 1);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // STATS PANEL
    // ═══════════════════════════════════════════════════════════════════════════
    
    m_statsGroup = new QGroupBox("Statistics", this);
    QHBoxLayout* statsLayout = new QHBoxLayout(m_statsGroup);
    
    m_totalLabel = new QLabel("Total: 0", this);
    m_completedLabel = new QLabel("Completed: 0", this);
    m_failedLabel = new QLabel("Failed: 0", this);
    m_blockedLabel = new QLabel("Blocked: 0", this);
    m_confidenceLabel = new QLabel("Avg Confidence: 0%", this);
    m_complexityLabel = new QLabel("Avg Complexity: 0", this);
    
    statsLayout->addWidget(m_totalLabel);
    statsLayout->addWidget(m_completedLabel);
    statsLayout->addWidget(m_failedLabel);
    statsLayout->addWidget(m_blockedLabel);
    statsLayout->addStretch();
    statsLayout->addWidget(m_confidenceLabel);
    statsLayout->addWidget(m_complexityLabel);
    
    m_mainLayout->addWidget(m_statsGroup);
    
    // Apply stylesheet
    setStyleSheet(R"(
        QGroupBox {
            font-weight: bold;
            border: 1px solid #555;
            border-radius: 5px;
            margin-top: 10px;
            padding-top: 10px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px;
        }
        QPushButton {
            padding: 5px 15px;
            border-radius: 3px;
        }
        QPushButton:hover {
            background-color: #555;
        }
        QPushButton:disabled {
            color: #888;
        }
        QProgressBar {
            border: 1px solid #555;
            border-radius: 5px;
            text-align: center;
        }
        QProgressBar::chunk {
            background-color: #4CAF50;
            border-radius: 4px;
        }
        QTableWidget {
            gridline-color: #444;
        }
        QTableWidget::item:selected {
            background-color: #0078D4;
        }
    )");
}

void IncompleteFeatureWidget::setupConnections()
{
    // Button connections
    connect(m_loadManifestBtn, &QPushButton::clicked, this, &IncompleteFeatureWidget::onLoadManifest);
    connect(m_startBtn, &QPushButton::clicked, this, &IncompleteFeatureWidget::onStartClicked);
    connect(m_stopBtn, &QPushButton::clicked, this, &IncompleteFeatureWidget::onStopClicked);
    connect(m_pauseBtn, &QPushButton::clicked, this, &IncompleteFeatureWidget::onPauseClicked);
    connect(m_resumeBtn, &QPushButton::clicked, this, &IncompleteFeatureWidget::onResumeClicked);
    connect(m_exportBtn, &QPushButton::clicked, this, &IncompleteFeatureWidget::onExportReport);
    
    // Filter connections
    connect(m_priorityCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &IncompleteFeatureWidget::onPriorityChanged);
    connect(m_categoryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &IncompleteFeatureWidget::onCategoryChanged);
    connect(m_statusCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { populateTable(); });
    connect(m_searchEdit, &QLineEdit::textChanged, 
            this, &IncompleteFeatureWidget::onSearchTextChanged);
    
    // Table connections
    connect(m_featureTable, &QTableWidget::cellClicked, 
            this, &IncompleteFeatureWidget::onFeatureSelected);
    connect(m_featureTable, &QTableWidget::cellDoubleClicked,
            this, &IncompleteFeatureWidget::onFeatureDoubleClicked);
    
    // Context menu
    connect(m_featureTable, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QMenu menu(this);
        menu.addAction("Complete Selected", this, &IncompleteFeatureWidget::onCompleteSelected);
        menu.addAction("Copy ID", this, [this]() {
            if (m_selectedFeatureId >= 0) {
                QApplication::clipboard()->setText(QString::number(m_selectedFeatureId));
            }
        });
        menu.addAction("Copy Code", this, [this]() {
            QApplication::clipboard()->setText(m_codePreview->toPlainText());
        });
        menu.exec(m_featureTable->mapToGlobal(pos));
    });
}

void IncompleteFeatureWidget::setEngine(IncompleteFeatureEngine* engine)
{
    if (m_engine) {
        // Disconnect old engine
        disconnect(m_engine, nullptr, this, nullptr);
    }
    
    m_engine = engine;
    
    if (m_engine) {
        // Connect engine signals
        connect(m_engine, &IncompleteFeatureEngine::manifestLoaded,
                this, &IncompleteFeatureWidget::onManifestLoaded);
        connect(m_engine, &IncompleteFeatureEngine::featureStarted,
                this, &IncompleteFeatureWidget::onFeatureStarted);
        connect(m_engine, &IncompleteFeatureEngine::featureCompleted,
                this, &IncompleteFeatureWidget::onFeatureCompleted);
        connect(m_engine, &IncompleteFeatureEngine::featureProgress,
                this, &IncompleteFeatureWidget::onFeatureProgress);
        connect(m_engine, &IncompleteFeatureEngine::overallProgress,
                this, &IncompleteFeatureWidget::onOverallProgress);
        connect(m_engine, &IncompleteFeatureEngine::errorOccurred,
                this, &IncompleteFeatureWidget::onErrorOccurred);
        connect(m_engine, &IncompleteFeatureEngine::completionPaused, this, [this]() {
            m_pauseBtn->setVisible(false);
            m_resumeBtn->setVisible(true);
            m_resumeBtn->setEnabled(true);
            m_progressLabel->setText("Paused");
        });
        connect(m_engine, &IncompleteFeatureEngine::completionResumed, this, [this]() {
            m_pauseBtn->setVisible(true);
            m_resumeBtn->setVisible(false);
            m_progressLabel->setText("Running...");
        });
        connect(m_engine, &IncompleteFeatureEngine::completionStopped, this, [this]() {
            m_startBtn->setEnabled(true);
            m_stopBtn->setEnabled(false);
            m_pauseBtn->setEnabled(false);
            m_progressLabel->setText("Stopped");
            emit completionFinished();
        });
    }
}

bool IncompleteFeatureWidget::loadManifest(const QString& path)
{
    if (!m_engine) {
        qWarning() << "[IncompleteFeatureWidget] No engine set";
        return false;
    }
    
    return m_engine->loadManifest(path);
}

void IncompleteFeatureWidget::refresh()
{
    populateTable();
    updateStats();
}

void IncompleteFeatureWidget::populateTable()
{
    if (!m_engine) return;
    
    m_featureTable->setRowCount(0);
    m_featureTable->setSortingEnabled(false);
    
    QVector<IncompleteFeature> features = m_engine->getAllFeatures();
    
    // Apply filters
    int priorityFilter = m_priorityCombo->currentData().toInt();
    int categoryFilter = m_categoryCombo->currentData().toInt();
    int statusFilter = m_statusCombo->currentData().toInt();
    QString searchText = m_searchEdit->text().toLower();
    
    for (const auto& feature : features) {
        // Priority filter
        if (priorityFilter >= 0 && static_cast<int>(feature.priority) != priorityFilter) {
            continue;
        }
        
        // Category filter
        if (categoryFilter >= 0 && static_cast<int>(feature.category) != categoryFilter) {
            continue;
        }
        
        // Status filter
        if (statusFilter >= 0 && static_cast<int>(feature.status) != statusFilter) {
            continue;
        }
        
        // Search filter
        if (!searchText.isEmpty()) {
            QString searchable = QString("%1 %2 %3 %4")
                .arg(feature.sourceFile)
                .arg(feature.functionName)
                .arg(feature.description)
                .arg(featureCategoryToString(feature.category)).toLower();
            if (!searchable.contains(searchText)) {
                continue;
            }
        }
        
        int row = m_featureTable->rowCount();
        m_featureTable->insertRow(row);
        
        // ID
        QTableWidgetItem* idItem = new QTableWidgetItem();
        idItem->setData(Qt::DisplayRole, feature.featureId);
        idItem->setData(Qt::UserRole, feature.featureId);
        m_featureTable->setItem(row, 0, idItem);
        
        // Priority
        QTableWidgetItem* priorityItem = new QTableWidgetItem(
            priorityToIcon(feature.priority) + " " + featurePriorityToString(feature.priority)
        );
        priorityItem->setForeground(priorityToColor(feature.priority));
        m_featureTable->setItem(row, 1, priorityItem);
        
        // Category
        m_featureTable->setItem(row, 2, new QTableWidgetItem(
            featureCategoryToString(feature.category)
        ));
        
        // Status
        QTableWidgetItem* statusItem = new QTableWidgetItem(
            statusToIcon(feature.status) + " " + completionStatusToString(feature.status)
        );
        statusItem->setForeground(statusToColor(feature.status));
        m_featureTable->setItem(row, 3, statusItem);
        
        // File
        m_featureTable->setItem(row, 4, new QTableWidgetItem(feature.sourceFile));
        
        // Function
        m_featureTable->setItem(row, 5, new QTableWidgetItem(feature.functionName));
        
        // Confidence
        QTableWidgetItem* confItem = new QTableWidgetItem();
        confItem->setData(Qt::DisplayRole, QString::number(feature.confidence * 100, 'f', 1) + "%");
        m_featureTable->setItem(row, 6, confItem);
        
        // Description
        m_featureTable->setItem(row, 7, new QTableWidgetItem(feature.description));
    }
    
    m_featureTable->setSortingEnabled(true);
}

void IncompleteFeatureWidget::updateStats()
{
    if (!m_engine) return;
    
    CompletionStats stats = m_engine->getStats();
    
    m_totalLabel->setText(QString("Total: %1").arg(stats.totalFeatures));
    m_completedLabel->setText(QString("Completed: %1").arg(stats.completedFeatures));
    m_failedLabel->setText(QString("Failed: %1").arg(stats.failedFeatures));
    m_blockedLabel->setText(QString("Blocked: %1").arg(stats.blockedFeatures));
    m_confidenceLabel->setText(QString("Avg Confidence: %1%")
        .arg(QString::number(stats.averageConfidence * 100, 'f', 1)));
    m_complexityLabel->setText(QString("Avg Complexity: %1")
        .arg(QString::number(stats.averageComplexity, 'f', 1)));
    
    m_overallProgress->setValue(static_cast<int>(stats.overallProgress));
    
    m_criticalProgress->setText(QString("🔴 Critical: %1/%2")
        .arg(stats.criticalCompleted).arg(stats.criticalTotal));
    m_highProgress->setText(QString("🟠 High: %1/%2")
        .arg(stats.highCompleted).arg(stats.highTotal));
    m_mediumProgress->setText(QString("🟡 Medium: %1/%2")
        .arg(stats.mediumCompleted).arg(stats.mediumTotal));
    m_lowProgress->setText(QString("🟢 Low: %1/%2")
        .arg(stats.lowCompleted).arg(stats.lowTotal));
    
    m_tokensLabel->setText(QString("Tokens: %1").arg(stats.totalTokensGenerated));
}

void IncompleteFeatureWidget::updateFeatureDetails(int featureId)
{
    if (!m_engine) return;
    
    IncompleteFeature feature = m_engine->getFeature(featureId);
    if (feature.featureId <= 0) return;
    
    // Update detail text
    QString details;
    QTextStream stream(&details);
    
    stream << "Feature #" << feature.featureId << "\n";
    stream << "═══════════════════════════════════════════\n\n";
    stream << "Priority:    " << featurePriorityToString(feature.priority) << "\n";
    stream << "Category:    " << featureCategoryToString(feature.category) << "\n";
    stream << "Status:      " << completionStatusToString(feature.status) << "\n";
    stream << "Source File: " << feature.sourceFile << "\n";
    stream << "Function:    " << feature.functionName << "\n";
    stream << "Line:        " << feature.lineNumber << "\n\n";
    stream << "Description:\n" << feature.description << "\n\n";
    stream << "Confidence:  " << QString::number(feature.confidence * 100, 'f', 1) << "%\n";
    stream << "Complexity:  " << QString::number(feature.complexity, 'f', 1) << "\n";
    stream << "Est. Tokens: " << feature.estimatedTokens << "\n";
    stream << "Est. Time:   " << feature.estimatedMinutes << " min\n\n";
    
    if (!feature.completionHash.isEmpty()) {
        stream << "Crypto Verification:\n";
        stream << "Hash:        " << feature.completionHash.left(32) << "...\n";
        stream << "Entropy:     " << feature.entropyValue << "\n";
        stream << "Penta-Mode:  " << QString::number(feature.pentaModeValue, 'f', 2) << "\n";
    }
    
    if (!feature.errorMessage.isEmpty()) {
        stream << "\n⚠️ Error:\n" << feature.errorMessage << "\n";
    }
    
    m_detailText->setText(details);
    
    // Update code preview
    if (!feature.generatedCode.isEmpty()) {
        m_codePreview->setText(feature.generatedCode);
    } else {
        m_codePreview->setText("// Code not yet generated");
    }
    
    // Update dependency tree
    m_dependencyTree->clear();
    
    QVector<int> deps = m_engine->getDependencies(featureId);
    if (!deps.isEmpty()) {
        QTreeWidgetItem* depsItem = new QTreeWidgetItem(m_dependencyTree);
        depsItem->setText(0, QString("Dependencies (%1)").arg(deps.size()));
        for (int depId : deps) {
            QTreeWidgetItem* depItem = new QTreeWidgetItem(depsItem);
            IncompleteFeature dep = m_engine->getFeature(depId);
            depItem->setText(0, QString("#%1 - %2").arg(depId).arg(dep.description.left(50)));
        }
        depsItem->setExpanded(true);
    }
    
    QVector<int> dependents = m_engine->getDependents(featureId);
    if (!dependents.isEmpty()) {
        QTreeWidgetItem* dependentsItem = new QTreeWidgetItem(m_dependencyTree);
        dependentsItem->setText(0, QString("Dependents (%1)").arg(dependents.size()));
        for (int depId : dependents) {
            QTreeWidgetItem* depItem = new QTreeWidgetItem(dependentsItem);
            IncompleteFeature dep = m_engine->getFeature(depId);
            depItem->setText(0, QString("#%1 - %2").arg(depId).arg(dep.description.left(50)));
        }
        dependentsItem->setExpanded(true);
    }
}

QString IncompleteFeatureWidget::priorityToIcon(FeaturePriority priority)
{
    switch (priority) {
        case FeaturePriority::CRITICAL: return "🔴";
        case FeaturePriority::HIGH: return "🟠";
        case FeaturePriority::MEDIUM: return "🟡";
        case FeaturePriority::LOW: return "🟢";
        default: return "⚪";
    }
}

QString IncompleteFeatureWidget::statusToIcon(CompletionStatus status)
{
    switch (status) {
        case CompletionStatus::NOT_STARTED: return "⬜";
        case CompletionStatus::PARSING: return "📝";
        case CompletionStatus::ANALYZING: return "🔍";
        case CompletionStatus::GENERATING: return "⚙️";
        case CompletionStatus::COMPILING: return "🔨";
        case CompletionStatus::TESTING: return "🧪";
        case CompletionStatus::COMPLETED: return "✅";
        case CompletionStatus::FAILED: return "❌";
        case CompletionStatus::BLOCKED: return "🚫";
        default: return "❓";
    }
}

QColor IncompleteFeatureWidget::priorityToColor(FeaturePriority priority)
{
    switch (priority) {
        case FeaturePriority::CRITICAL: return QColor("#FF4444");
        case FeaturePriority::HIGH: return QColor("#FF8800");
        case FeaturePriority::MEDIUM: return QColor("#FFCC00");
        case FeaturePriority::LOW: return QColor("#44FF44");
        default: return QColor("#888888");
    }
}

QColor IncompleteFeatureWidget::statusToColor(CompletionStatus status)
{
    switch (status) {
        case CompletionStatus::COMPLETED: return QColor("#44FF44");
        case CompletionStatus::FAILED: return QColor("#FF4444");
        case CompletionStatus::BLOCKED: return QColor("#FF8800");
        case CompletionStatus::GENERATING:
        case CompletionStatus::COMPILING:
        case CompletionStatus::TESTING: return QColor("#44AAFF");
        default: return QColor("#888888");
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// SLOTS
// ═══════════════════════════════════════════════════════════════════════════════

void IncompleteFeatureWidget::onStartClicked()
{
    if (!m_engine) return;
    
    int concurrency = m_concurrencySpinBox->value();
    m_engine->setMaxConcurrentCompletions(concurrency);
    
    m_startBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);
    m_pauseBtn->setEnabled(true);
    m_progressLabel->setText("Running...");
    
    emit completionStarted();
    
    // Start in a separate thread
    QtConcurrent::run([this]() {
        m_engine->completeAllFeatures(m_concurrencySpinBox->value());
    });
}

void IncompleteFeatureWidget::onStopClicked()
{
    if (m_engine) {
        m_engine->stopCompletion();
    }
}

void IncompleteFeatureWidget::onPauseClicked()
{
    if (m_engine) {
        m_engine->pauseCompletion();
    }
}

void IncompleteFeatureWidget::onResumeClicked()
{
    if (m_engine) {
        m_engine->resumeCompletion();
    }
}

void IncompleteFeatureWidget::onPriorityChanged(int index)
{
    Q_UNUSED(index);
    populateTable();
}

void IncompleteFeatureWidget::onCategoryChanged(int index)
{
    Q_UNUSED(index);
    populateTable();
}

void IncompleteFeatureWidget::onSearchTextChanged(const QString& text)
{
    Q_UNUSED(text);
    populateTable();
}

void IncompleteFeatureWidget::onFeatureSelected(int row, int column)
{
    Q_UNUSED(column);
    
    QTableWidgetItem* item = m_featureTable->item(row, 0);
    if (item) {
        m_selectedFeatureId = item->data(Qt::UserRole).toInt();
        updateFeatureDetails(m_selectedFeatureId);
        emit featureSelected(m_selectedFeatureId);
    }
}

void IncompleteFeatureWidget::onFeatureDoubleClicked(int row, int column)
{
    Q_UNUSED(column);
    
    QTableWidgetItem* item = m_featureTable->item(row, 0);
    if (item && m_engine) {
        int featureId = item->data(Qt::UserRole).toInt();
        m_engine->completeFeatureAsync(featureId);
    }
}

void IncompleteFeatureWidget::onCompleteSelected()
{
    if (m_selectedFeatureId >= 0 && m_engine) {
        m_engine->completeFeatureAsync(m_selectedFeatureId);
    }
}

void IncompleteFeatureWidget::onExportReport()
{
    if (!m_engine) return;
    
    QString fileName = QFileDialog::getSaveFileName(this, 
        "Export Progress Report", 
        "progress_report.md", 
        "Markdown Files (*.md);;All Files (*)");
    
    if (!fileName.isEmpty()) {
        if (m_engine->exportProgressReport(fileName)) {
            QMessageBox::information(this, "Export Complete", 
                "Progress report exported successfully.");
        } else {
            QMessageBox::warning(this, "Export Failed", 
                "Failed to export progress report.");
        }
    }
}

void IncompleteFeatureWidget::onLoadManifest()
{
    QString fileName = QFileDialog::getOpenFileName(this,
        "Load Incomplete Features Manifest",
        "",
        "Markdown Files (*.md);;All Files (*)");
    
    if (!fileName.isEmpty()) {
        if (loadManifest(fileName)) {
            QMessageBox::information(this, "Manifest Loaded",
                QString("Loaded %1 features from manifest.")
                    .arg(m_engine ? m_engine->getStats().totalFeatures : 0));
        } else {
            QMessageBox::warning(this, "Load Failed",
                "Failed to load manifest file.");
        }
    }
}

void IncompleteFeatureWidget::onManifestLoaded(int count)
{
    m_startBtn->setEnabled(count > 0);
    populateTable();
    updateStats();
    m_progressLabel->setText(QString("Loaded %1 features - Ready").arg(count));
}

void IncompleteFeatureWidget::onFeatureStarted(int featureId)
{
    m_progressLabel->setText(QString("Processing feature #%1...").arg(featureId));
    
    // Update table row
    for (int row = 0; row < m_featureTable->rowCount(); ++row) {
        QTableWidgetItem* item = m_featureTable->item(row, 0);
        if (item && item->data(Qt::UserRole).toInt() == featureId) {
            m_featureTable->item(row, 3)->setText("⚙️ GENERATING");
            m_featureTable->item(row, 3)->setForeground(QColor("#44AAFF"));
            break;
        }
    }
}

void IncompleteFeatureWidget::onFeatureCompleted(int featureId, bool success)
{
    // Update table row
    for (int row = 0; row < m_featureTable->rowCount(); ++row) {
        QTableWidgetItem* item = m_featureTable->item(row, 0);
        if (item && item->data(Qt::UserRole).toInt() == featureId) {
            if (success) {
                m_featureTable->item(row, 3)->setText("✅ COMPLETED");
                m_featureTable->item(row, 3)->setForeground(QColor("#44FF44"));
            } else {
                m_featureTable->item(row, 3)->setText("❌ FAILED");
                m_featureTable->item(row, 3)->setForeground(QColor("#FF4444"));
            }
            break;
        }
    }
    
    updateStats();
    
    // Update details if this is the selected feature
    if (featureId == m_selectedFeatureId) {
        updateFeatureDetails(featureId);
    }
}

void IncompleteFeatureWidget::onFeatureProgress(int featureId, int percent)
{
    // Could update a per-feature progress indicator if needed
    Q_UNUSED(featureId);
    Q_UNUSED(percent);
}

void IncompleteFeatureWidget::onOverallProgress(double percent)
{
    m_overallProgress->setValue(static_cast<int>(percent));
}

void IncompleteFeatureWidget::onErrorOccurred(int featureId, const QString& error)
{
    qWarning() << "[IncompleteFeatureWidget] Error on feature" << featureId << ":" << error;
    
    // Update table row
    for (int row = 0; row < m_featureTable->rowCount(); ++row) {
        QTableWidgetItem* item = m_featureTable->item(row, 0);
        if (item && item->data(Qt::UserRole).toInt() == featureId) {
            m_featureTable->item(row, 3)->setText("❌ FAILED");
            m_featureTable->item(row, 3)->setForeground(QColor("#FF4444"));
            m_featureTable->item(row, 3)->setToolTip(error);
            break;
        }
    }
}
