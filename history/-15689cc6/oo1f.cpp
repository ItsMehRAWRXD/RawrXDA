#include "features_view_menu.h"
#include <iostream>
#include <chrono>

FeaturesViewMenu::FeaturesViewMenu() {
    setupUI();
    addCategoryNodes();
    loadState();
    std::cout << "[FeaturesViewMenu] Initialized" << std::endl;
}

FeaturesViewMenu::~FeaturesViewMenu() { saveState(); }

void FeaturesViewMenu::setupUI() {
    // Minimal headless UI: nothing to allocate, state maintained in-memory
}

void FeaturesViewMenu::createContextMenu() {}

void FeaturesViewMenu::addCategoryNodes() {
    m_categories[FeatureCategory::Core] = "Core Features";
    m_categories[FeatureCategory::AI] = "AI & Machine Learning";
    m_categories[FeatureCategory::Advanced] = "Advanced Features";
    m_categories[FeatureCategory::Experimental] = "Experimental";
    m_categories[FeatureCategory::Utilities] = "Utilities";
    m_categories[FeatureCategory::Performance] = "Performance";
    m_categories[FeatureCategory::Debug] = "Debug Tools";
}

void FeaturesViewMenu::registerFeature(const Feature &feature) {
    m_features[feature.id] = feature;
    m_enabledFeatures.insert(feature.id);
    m_itemMap[feature.id] = feature.name;
    std::cout << "[Features] Registered: " << feature.name << " (" << feature.id << ")" << std::endl;
}

void FeaturesViewMenu::enableFeature(const std::string &featureId, bool enable) {
    auto it = m_features.find(featureId);
    if (it == m_features.end()) return;
    it->second.enabled = enable;
    if (enable) m_enabledFeatures.insert(featureId); else m_enabledFeatures.erase(featureId);
    if (m_featureToggled) m_featureToggled(featureId, enable);
    logFeatureMetrics(featureId);
}

bool FeaturesViewMenu::isFeatureEnabled(const std::string &featureId) const {
    return m_enabledFeatures.find(featureId) != m_enabledFeatures.end();
}

void FeaturesViewMenu::toggleFeatureVisibility(const std::string &categoryId, bool visible) {
    for (auto &it : m_categories) {
        if (it.second == categoryId) {
            if (m_categoryToggled) m_categoryToggled(static_cast<int>(it.first), visible);
            break;
        }
    }
}

void FeaturesViewMenu::recordFeatureUsage(const std::string &featureId, long long timeMs) {
    auto it = m_features.find(featureId);
    if (it == m_features.end()) return;
    auto &metrics = m_metrics[featureId];
    metrics.totalTimeMs += timeMs;
    metrics.callCount++;
    metrics.lastCallTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    metrics.peakTimeMs = std::max(metrics.peakTimeMs, timeMs);
    it->second.usageCount = metrics.callCount;
    it->second.totalTimeMs = metrics.totalTimeMs;
}

long long FeaturesViewMenu::getFeatureExecutionTime(const std::string &featureId) const {
    auto it = m_metrics.find(featureId);
    return it == m_metrics.end() ? 0 : it->second.totalTimeMs;
}

int FeaturesViewMenu::getFeatureUsageCount(const std::string &featureId) const {
    auto it = m_metrics.find(featureId);
    return it == m_metrics.end() ? 0 : it->second.callCount;
}

void FeaturesViewMenu::expandCategory(FeatureCategory category, bool /*expand*/) {}

void FeaturesViewMenu::expandAllCategories() {}

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
