#pragma once
#include <QDockWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QMap>
#include <QSet>
#include <memory>

class FeatureCategory;
class FeatureItem;

/**
 * @class FeaturesViewMenu
 * @brief Production-ready hierarchical features view with toggable sub-menus
 * 
 * Features:
 * - Organized feature hierarchy (Core, AI, Advanced, Experimental)
 * - Toggle visibility of feature categories
 * - Enable/disable individual features
 * - Performance metrics per feature
 * - Status indicators (beta, stable, experimental)
 * - Search/filter capabilities
 * - Persistent state (registry/config file)
 */
class FeaturesViewMenu : public QDockWidget {
    Q_OBJECT

public:
    enum class FeatureStatus {
        Stable,
        Beta,
        Experimental,
        Deprecated,
        Disabled
    };

    enum class FeatureCategory {
        Core,
        AI,
        Advanced,
        Experimental,
        Utilities,
        Performance,
        Debug
    };

    struct Feature {
        QString id;
        QString name;
        QString description;
        FeatureStatus status;
        FeatureCategory category;
        bool enabled;
        bool visible;
        int usageCount;
        qint64 totalTimeMs;
        QStringList dependencies;
        QString version;
    };

    explicit FeaturesViewMenu(QWidget *parent = nullptr);
    ~FeaturesViewMenu();

    // Feature management
    void registerFeature(const Feature &feature);
    void enableFeature(const QString &featureId, bool enable);
    bool isFeatureEnabled(const QString &featureId) const;
    void toggleFeatureVisibility(const QString &categoryId, bool visible);
    
    // Statistics
    void recordFeatureUsage(const QString &featureId, qint64 timeMs);
    qint64 getFeatureExecutionTime(const QString &featureId) const;
    int getFeatureUsageCount(const QString &featureId) const;
    
    // Category management
    void expandCategory(FeatureCategory category, bool expand = true);
    void collapseAllCategories();
    void expandAllCategories();
    
    // Search and filtering
    void filterFeatures(const QString &searchText);
    void clearFilter();
    
    // Persistence
    void saveState();
    void loadState();

signals:
    void featureToggled(const QString &featureId, bool enabled);
    void categoryToggled(FeatureCategory category, bool visible);
    void featureClicked(const QString &featureId);
    void featureDoubleClicked(const QString &featureId);

private slots:
    void onItemClicked(QTreeWidgetItem *item, int column);
    void onItemDoubleClicked(QTreeWidgetItem *item, int column);
    void onItemChanged(QTreeWidgetItem *item, int column);
    void onContextMenu(const QPoint &pos);
    void onCopyFeatureInfo();
    void onShowStats();
    void onToggleCategory();

private:
    struct CategoryNode {
        QString categoryName;
        QTreeWidgetItem *treeItem;
        QSet<QString> features;
        bool expanded;
    };

    void setupUI();
    void createContextMenu();
    void addCategoryNodes();
    void populateCategory(FeatureCategory category);
    QTreeWidgetItem* getCategoryItem(FeatureCategory category);
    QString statusToString(FeatureStatus status) const;
    QString categoryToString(FeatureCategory category) const;
    QIcon getStatusIcon(FeatureStatus status) const;
    void updateFeatureItem(QTreeWidgetItem *item, const Feature &feature);
    void logFeatureMetrics(const QString &featureId);

    // UI Components
    QTreeWidget *m_treeWidget;
    class QLineEdit *m_searchBox;
    class QLabel *m_statsLabel;
    class QPushButton *m_expandAllBtn;
    class QPushButton *m_collapseAllBtn;
    class QComboBox *m_filterCombo;

    // Data storage
    QMap<QString, Feature> m_features;
    QMap<FeatureCategory, CategoryNode> m_categories;
    QMap<QString, QTreeWidgetItem*> m_itemMap;
    QSet<QString> m_enabledFeatures;
    
    // Metrics
    struct FeatureMetrics {
        qint64 totalTimeMs = 0;
        int callCount = 0;
        qint64 lastCallTime = 0;
        qint64 peakTimeMs = 0;
    };
    QMap<QString, FeatureMetrics> m_metrics;
    
    // UI state
    bool m_suppressSignals = false;
    QString m_currentFilter;
};
