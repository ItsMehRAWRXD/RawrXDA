#include "features_view_menu.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QMenu>
#include <QAction>
#include <QClipboard>
#include <QApplication>
#include <QHeaderView>
#include <QScrollBar>
#include <QSettings>
#include <QDebug>
#include <QDateTime>

FeaturesViewMenu::FeaturesViewMenu(QWidget *parent)
    : QDockWidget("Features Panel", parent)
{
    setObjectName("FeaturesViewMenu");
    setupUI();
    createContextMenu();
    addCategoryNodes();
    
    // Load saved state
    loadState();
    
    qDebug() << "[FeaturesViewMenu] Initialized with hierarchical feature organization";
}

FeaturesViewMenu::~FeaturesViewMenu() {
    saveState();
}

void FeaturesViewMenu::setupUI() {
    QWidget *mainWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(mainWidget);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->setSpacing(5);
    
    // Search and filter bar
    QHBoxLayout *searchLayout = new QHBoxLayout();
    QLabel *searchLabel = new QLabel("Search:");
    m_searchBox = new QLineEdit();
    m_searchBox->setPlaceholderText("Filter features...");
    m_searchBox->setMaximumHeight(28);
    
    connect(m_searchBox, &QLineEdit::textChanged, this, &FeaturesViewMenu::filterFeatures);
    
    searchLayout->addWidget(searchLabel);
    searchLayout->addWidget(m_searchBox);
    mainLayout->addLayout(searchLayout);
    
    // Category filter
    QHBoxLayout *filterLayout = new QHBoxLayout();
    QLabel *filterLabel = new QLabel("Category:");
    m_filterCombo = new QComboBox();
    m_filterCombo->addItem("All Categories", -1);
    m_filterCombo->addItem("Core", static_cast<int>(FeatureCategory::Core));
    m_filterCombo->addItem("AI & ML", static_cast<int>(FeatureCategory::AI));
    m_filterCombo->addItem("Advanced", static_cast<int>(FeatureCategory::Advanced));
    m_filterCombo->addItem("Experimental", static_cast<int>(FeatureCategory::Experimental));
    m_filterCombo->addItem("Utilities", static_cast<int>(FeatureCategory::Utilities));
    m_filterCombo->addItem("Performance", static_cast<int>(FeatureCategory::Performance));
    m_filterCombo->addItem("Debug", static_cast<int>(FeatureCategory::Debug));
    m_filterCombo->setMaximumHeight(28);
    
    filterLayout->addWidget(filterLabel);
    filterLayout->addWidget(m_filterCombo);
    mainLayout->addLayout(filterLayout);
    
    // Tree widget for feature hierarchy
    m_treeWidget = new QTreeWidget();
    m_treeWidget->setColumnCount(3);
    m_treeWidget->setHeaderLabels(QStringList() << "Feature" << "Status" << "Usage");
    m_treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    m_treeWidget->setColumnWidth(0, 250);
    m_treeWidget->setColumnWidth(1, 100);
    m_treeWidget->setColumnWidth(2, 80);
    m_treeWidget->setIndentation(20);
    m_treeWidget->setUniformRowHeights(true);
    m_treeWidget->setStyleSheet(
        "QTreeWidget { border: 1px solid #ccc; border-radius: 4px; }"
        "QTreeWidget::item { padding: 4px; }"
        "QTreeWidget::item:hover { background-color: #f0f0f0; }"
        "QTreeWidget::item:selected { background-color: #0078d4; color: white; }"
    );
    
    connect(m_treeWidget, &QTreeWidget::itemClicked, this, &FeaturesViewMenu::onItemClicked);
    connect(m_treeWidget, &QTreeWidget::itemDoubleClicked, this, &FeaturesViewMenu::onItemDoubleClicked);
    connect(m_treeWidget, &QTreeWidget::itemChanged, this, &FeaturesViewMenu::onItemChanged);
    connect(m_treeWidget, &QTreeWidget::customContextMenuRequested, this, &FeaturesViewMenu::onContextMenu);
    
    mainLayout->addWidget(m_treeWidget, 1);
    
    // Control buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_expandAllBtn = new QPushButton("Expand All");
    m_collapseAllBtn = new QPushButton("Collapse All");
    
    connect(m_expandAllBtn, &QPushButton::clicked, this, &FeaturesViewMenu::expandAllCategories);
    connect(m_collapseAllBtn, &QPushButton::clicked, this, &FeaturesViewMenu::collapseAllCategories);
    
    btnLayout->addWidget(m_expandAllBtn);
    btnLayout->addWidget(m_collapseAllBtn);
    btnLayout->addStretch();
    mainLayout->addLayout(btnLayout);
    
    // Statistics label
    m_statsLabel = new QLabel("Ready");
    m_statsLabel->setStyleSheet("color: #666; font-size: 10px; padding: 4px;");
    mainLayout->addWidget(m_statsLabel);
    
    setWidget(mainWidget);
}

void FeaturesViewMenu::createContextMenu() {
    // Context menu actions defined in onContextMenu
}

void FeaturesViewMenu::addCategoryNodes() {
    // Add category tree items
    QStringList categories = {"Core Features", "AI & Machine Learning", "Advanced Features", 
                              "Experimental", "Utilities", "Performance", "Debug Tools"};
    
    for (int i = 0; i < categories.size(); ++i) {
        QTreeWidgetItem *categoryItem = new QTreeWidgetItem(m_treeWidget);
        categoryItem->setText(0, categories[i]);
        categoryItem->setIcon(0, QIcon(":/icons/folder.png"));
        categoryItem->setFont(0, QFont("Segoe UI", 10, QFont::Bold));
        categoryItem->setData(0, Qt::UserRole, "category");
        categoryItem->setExpanded(true);
        
        auto category = static_cast<FeatureCategory>(i);
        m_categories[category] = {categories[i], categoryItem, {}, true};
    }
}

void FeaturesViewMenu::registerFeature(const Feature &feature) {
    m_features[feature.id] = feature;
    m_enabledFeatures.insert(feature.id);
    
    // Add to tree
    auto categoryItem = getCategoryItem(feature.category);
    if (!categoryItem) return;
    
    QTreeWidgetItem *featureItem = new QTreeWidgetItem(categoryItem);
    featureItem->setText(0, feature.name);
    featureItem->setText(1, statusToString(feature.status));
    featureItem->setText(2, "0");
    featureItem->setData(0, Qt::UserRole, feature.id);
    featureItem->setCheckState(0, feature.enabled ? Qt::Checked : Qt::Unchecked);
    featureItem->setIcon(0, getStatusIcon(feature.status));
    
    m_itemMap[feature.id] = featureItem;
    m_categories[feature.category].features.insert(feature.id);
    
    qDebug() << "[FeaturesViewMenu] Registered feature:" << feature.name << "(" << feature.id << ")";
}

void FeaturesViewMenu::enableFeature(const QString &featureId, bool enable) {
    if (!m_features.contains(featureId)) return;
    
    m_features[featureId].enabled = enable;
    if (enable) {
        m_enabledFeatures.insert(featureId);
    } else {
        m_enabledFeatures.remove(featureId);
    }
    
    if (auto item = m_itemMap.value(featureId)) {
        m_suppressSignals = true;
        item->setCheckState(0, enable ? Qt::Checked : Qt::Unchecked);
        m_suppressSignals = false;
    }
    
    emit featureToggled(featureId, enable);
    logFeatureMetrics(featureId);
}

bool FeaturesViewMenu::isFeatureEnabled(const QString &featureId) const {
    return m_enabledFeatures.contains(featureId);
}

void FeaturesViewMenu::toggleFeatureVisibility(const QString &categoryId, bool visible) {
    // Find category and toggle all its features
    for (auto it = m_categories.begin(); it != m_categories.end(); ++it) {
        if (it.value().categoryName == categoryId) {
            for (const auto &featureId : it.value().features) {
                if (auto item = m_itemMap.value(featureId)) {
                    item->setHidden(!visible);
                }
            }
            emit categoryToggled(static_cast<int>(it.key()), visible);
            break;
        }
    }
}

void FeaturesViewMenu::recordFeatureUsage(const QString &featureId, qint64 timeMs) {
    if (!m_features.contains(featureId)) return;
    
    auto &metrics = m_metrics[featureId];
    metrics.totalTimeMs += timeMs;
    metrics.callCount++;
    metrics.lastCallTime = QDateTime::currentMSecsSinceEpoch();
    metrics.peakTimeMs = qMax(metrics.peakTimeMs, timeMs);
    
    m_features[featureId].usageCount = metrics.callCount;
    m_features[featureId].totalTimeMs = metrics.totalTimeMs;
    
    if (auto item = m_itemMap.value(featureId)) {
        item->setText(2, QString::number(metrics.callCount));
    }
}

qint64 FeaturesViewMenu::getFeatureExecutionTime(const QString &featureId) const {
    return m_metrics.value(featureId).totalTimeMs;
}

int FeaturesViewMenu::getFeatureUsageCount(const QString &featureId) const {
    return m_metrics.value(featureId).callCount;
}

void FeaturesViewMenu::expandCategory(FeatureCategory category, bool expand) {
    auto it = m_categories.find(category);
    if (it != m_categories.end()) {
        it.value().treeItem->setExpanded(expand);
        it.value().expanded = expand;
    }
}

void FeaturesViewMenu::expandAllCategories() {
    m_treeWidget->expandAll();
    for (auto &cat : m_categories) {
        cat.expanded = true;
    }
}

void FeaturesViewMenu::collapseAllCategories() {
    m_treeWidget->collapseAll();
    for (auto &cat : m_categories) {
        cat.expanded = false;
    }
}

void FeaturesViewMenu::filterFeatures(const QString &searchText) {
    m_currentFilter = searchText.toLower();
    
    for (auto &cat : m_categories) {
        bool categoryVisible = false;
        for (const auto &featureId : cat.features) {
            const auto &feature = m_features[featureId];
            bool matches = feature.name.toLower().contains(m_currentFilter) ||
                          feature.description.toLower().contains(m_currentFilter) ||
                          feature.id.toLower().contains(m_currentFilter);
            
            if (auto item = m_itemMap.value(featureId)) {
                item->setHidden(!matches);
                categoryVisible = categoryVisible || matches;
            }
        }
        cat.treeItem->setHidden(!categoryVisible && !m_currentFilter.isEmpty());
    }
}

void FeaturesViewMenu::clearFilter() {
    m_searchBox->clear();
    m_filterCombo->setCurrentIndex(0);
    filterFeatures("");
}

void FeaturesViewMenu::saveState() {
    QSettings settings("RawrXD", "FeaturesPanel");
    settings.beginGroup("Features");
    
    for (auto it = m_enabledFeatures.begin(); it != m_enabledFeatures.end(); ++it) {
        settings.setValue(*it, true);
    }
    
    settings.endGroup();
    settings.beginGroup("Categories");
    
    for (auto &cat : m_categories) {
        settings.setValue(cat.categoryName, cat.expanded);
    }
    
    settings.endGroup();
    qDebug() << "[FeaturesViewMenu] State saved to registry";
}

void FeaturesViewMenu::loadState() {
    QSettings settings("RawrXD", "FeaturesPanel");
    settings.beginGroup("Features");
    
    QStringList keys = settings.allKeys();
    for (const auto &key : keys) {
        if (settings.value(key, false).toBool()) {
            m_enabledFeatures.insert(key);
        }
    }
    
    settings.endGroup();
    settings.beginGroup("Categories");
    
    for (auto &cat : m_categories) {
        bool expanded = settings.value(cat.categoryName, true).toBool();
        cat.treeItem->setExpanded(expanded);
        cat.expanded = expanded;
    }
    
    settings.endGroup();
    qDebug() << "[FeaturesViewMenu] State loaded from registry";
}

void FeaturesViewMenu::onItemClicked(QTreeWidgetItem *item, int column) {
    if (m_suppressSignals) return;
    
    QString featureId = item->data(0, Qt::UserRole).toString();
    if (featureId.isEmpty() || featureId == "category") return;
    
    emit featureClicked(featureId);
    logFeatureMetrics(featureId);
}

void FeaturesViewMenu::onItemDoubleClicked(QTreeWidgetItem *item, int column) {
    QString featureId = item->data(0, Qt::UserRole).toString();
    if (featureId.isEmpty() || featureId == "category") return;
    
    emit featureDoubleClicked(featureId);
    recordFeatureUsage(featureId, 0);
}

void FeaturesViewMenu::onItemChanged(QTreeWidgetItem *item, int column) {
    if (m_suppressSignals || column != 0) return;
    
    QString featureId = item->data(0, Qt::UserRole).toString();
    if (featureId.isEmpty() || featureId == "category") return;
    
    bool checked = item->checkState(0) == Qt::Checked;
    enableFeature(featureId, checked);
}

void FeaturesViewMenu::onContextMenu(const QPoint &pos) {
    QTreeWidgetItem *item = m_treeWidget->itemAt(pos);
    if (!item) return;
    
    QString featureId = item->data(0, Qt::UserRole).toString();
    if (featureId.isEmpty() || featureId == "category") return;
    
    QMenu menu;
    
    QAction *statsAction = menu.addAction("View Statistics");
    QAction *copyAction = menu.addAction("Copy Feature Info");
    menu.addSeparator();
    QAction *toggleAction = menu.addAction("Toggle Feature");
    
    connect(statsAction, &QAction::triggered, this, &FeaturesViewMenu::onShowStats);
    connect(copyAction, &QAction::triggered, this, &FeaturesViewMenu::onCopyFeatureInfo);
    connect(toggleAction, &QAction::triggered, this, &FeaturesViewMenu::onToggleCategory);
    
    menu.exec(m_treeWidget->mapToGlobal(pos));
}

void FeaturesViewMenu::onCopyFeatureInfo() {
    if (m_treeWidget->currentItem()) {
        QString info = m_treeWidget->currentItem()->text(0) + " - " +
                      m_treeWidget->currentItem()->text(1) + " - " +
                      m_treeWidget->currentItem()->text(2) + " uses";
        QApplication::clipboard()->setText(info);
        m_statsLabel->setText("Feature info copied!");
    }
}

void FeaturesViewMenu::onShowStats() {
    QTreeWidgetItem *item = m_treeWidget->currentItem();
    if (!item) return;
    
    QString featureId = item->data(0, Qt::UserRole).toString();
    if (featureId.isEmpty() || !m_metrics.contains(featureId)) return;
    
    const auto &metrics = m_metrics[featureId];
    QString stats = QString("Calls: %1 | Total: %2ms | Peak: %3ms | Avg: %4ms")
        .arg(metrics.callCount)
        .arg(metrics.totalTimeMs)
        .arg(metrics.peakTimeMs)
        .arg(metrics.callCount > 0 ? metrics.totalTimeMs / metrics.callCount : 0);
    
    m_statsLabel->setText(stats);
}

void FeaturesViewMenu::onToggleCategory() {
    QTreeWidgetItem *item = m_treeWidget->currentItem();
    if (!item) return;
    
    QString featureId = item->data(0, Qt::UserRole).toString();
    if (!featureId.isEmpty()) {
        bool currentlyEnabled = item->checkState(0) == Qt::Checked;
        item->setCheckState(0, currentlyEnabled ? Qt::Unchecked : Qt::Checked);
    }
}

QTreeWidgetItem* FeaturesViewMenu::getCategoryItem(FeatureCategory category) {
    auto it = m_categories.find(category);
    return it != m_categories.end() ? it.value().treeItem : nullptr;
}

QString FeaturesViewMenu::statusToString(FeatureStatus status) const {
    switch (status) {
        case FeatureStatus::Stable: return "Stable";
        case FeatureStatus::Beta: return "Beta";
        case FeatureStatus::Experimental: return "Experimental";
        case FeatureStatus::Deprecated: return "Deprecated";
        case FeatureStatus::Disabled: return "Disabled";
    }
    return "Unknown";
}

QString FeaturesViewMenu::categoryToString(FeatureCategory category) const {
    switch (category) {
        case FeatureCategory::Core: return "Core";
        case FeatureCategory::AI: return "AI & ML";
        case FeatureCategory::Advanced: return "Advanced";
        case FeatureCategory::Experimental: return "Experimental";
        case FeatureCategory::Utilities: return "Utilities";
        case FeatureCategory::Performance: return "Performance";
        case FeatureCategory::Debug: return "Debug";
    }
    return "Unknown";
}

QIcon FeaturesViewMenu::getStatusIcon(FeatureStatus status) const {
    // In production, load actual icons from resources
    // For now return empty icon
    return QIcon();
}

void FeaturesViewMenu::updateFeatureItem(QTreeWidgetItem *item, const Feature &feature) {
    if (!item) return;
    
    item->setText(0, feature.name);
    item->setText(1, statusToString(feature.status));
    item->setIcon(0, getStatusIcon(feature.status));
    item->setCheckState(0, feature.enabled ? Qt::Checked : Qt::Unchecked);
}

void FeaturesViewMenu::logFeatureMetrics(const QString &featureId) {
    if (!m_metrics.contains(featureId)) return;
    
    const auto &metrics = m_metrics[featureId];
    qDebug() << "[FeaturesViewMenu] Feature:" << featureId 
             << "Calls:" << metrics.callCount 
             << "Total:" << metrics.totalTimeMs << "ms"
             << "Peak:" << metrics.peakTimeMs << "ms";
}

void FeaturesViewMenu::populateCategory(FeatureCategory category) {
    // Dynamically populate category based on registered features
}

void FeaturesViewMenu::registerFeature(const QString &id, const QString &name, const QString &description, FeatureCategory category, bool enabled) {
    Feature feature;
    feature.id = id;
    feature.name = name;
    feature.description = description;
    feature.category = category;
    feature.enabled = enabled;
    feature.status = FeatureStatus::Stable;
    feature.version = "1.0.0";
    
    registerFeature(feature);
}
