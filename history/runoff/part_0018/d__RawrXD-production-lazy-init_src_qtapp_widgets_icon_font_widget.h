/**
 * @file icon_font_widget.h
 * @brief Full Icon Font Browser Widget for RawrXD IDE
 * @author RawrXD Team
 */

#pragma once

#include <QWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QToolBar>
#include <QPushButton>
#include <QSpinBox>
#include <QSettings>
#include <QMap>
#include <QFont>

/**
 * @brief Structure for icon info
 */
struct IconInfo {
    QString name;
    QString unicode;
    QStringList categories;
    QStringList aliases;
    QString fontFamily;
};

/**
 * @brief Structure for icon font set
 */
struct IconFontSet {
    QString name;
    QString fontFamily;
    QString prefix;
    QVector<IconInfo> icons;
};

/**
 * @brief Full Icon Font Browser Widget
 * 
 * Features:
 * - Browse multiple icon font sets (Font Awesome, Material Icons, etc.)
 * - Search icons by name, alias, or category
 * - Preview icons at different sizes
 * - Copy icon code in multiple formats (Unicode, CSS class, HTML)
 * - Recent and favorite icons
 * - Category filtering
 * - Custom icon font loading
 */
class IconFontWidget : public QWidget {
    Q_OBJECT

public:
    enum OutputFormat {
        Unicode,        // \uf000
        HtmlEntity,     // &#xf000;
        CssClass,       // fa fa-icon
        HtmlTag,        // <i class="fa fa-icon"></i>
        QtUnicode,      // QChar(0xf000)
        SvgPath         // SVG path data (if available)
    };
    Q_ENUM(OutputFormat)

    explicit IconFontWidget(QWidget* parent = nullptr);
    ~IconFontWidget();

    // Icon management
    void loadIconFontSet(const IconFontSet& fontSet);
    void loadFontAwesome();
    void loadMaterialIcons();
    void loadCustomFont(const QString& fontPath, const QString& jsonPath);
    
    // Search and filter
    void setSearchText(const QString& text);
    void setCategory(const QString& category);
    
    // Output
    void setOutputFormat(OutputFormat format);
    OutputFormat getOutputFormat() const { return m_outputFormat; }
    
    QString getSelectedIconCode() const;
    QString getSelectedIconName() const;

signals:
    void iconSelected(const QString& name, const QString& unicode);
    void iconDoubleClicked(const QString& code);
    void iconCopied(const QString& code);

public slots:
    void copySelectedIcon();
    void insertSelectedIcon();
    void addToFavorites();
    void refreshIconList();

private slots:
    void onSearchTextChanged(const QString& text);
    void onCategoryChanged(int index);
    void onFontSetChanged(int index);
    void onIconClicked(QListWidgetItem* item);
    void onIconDoubleClicked(QListWidgetItem* item);
    void onSizeChanged(int size);
    void onOutputFormatChanged(int index);

private:
    void setupUI();
    void setupToolbar();
    void setupIconGrid();
    void setupPreview();
    void connectSignals();
    
    void populateIconList();
    void populateCategories();
    void filterIcons();
    void updatePreview(const IconInfo& icon);
    
    QString formatIcon(const IconInfo& icon, OutputFormat format) const;
    void loadBuiltinFonts();
    void saveRecentIcons();
    void loadRecentIcons();
    void saveFavorites();
    void loadFavorites();

private:
    // UI Components
    QToolBar* m_toolbar;
    QLineEdit* m_searchEdit;
    QComboBox* m_fontSetCombo;
    QComboBox* m_categoryCombo;
    QComboBox* m_outputFormatCombo;
    QSpinBox* m_sizeSpin;
    
    QListWidget* m_iconList;
    
    // Preview panel
    QLabel* m_previewLabel;
    QLabel* m_iconNameLabel;
    QLabel* m_unicodeLabel;
    QLabel* m_categoriesLabel;
    QPushButton* m_copyBtn;
    QPushButton* m_insertBtn;
    QPushButton* m_favoriteBtn;
    
    // Recent and favorites
    QListWidget* m_recentList;
    QListWidget* m_favoritesList;
    
    // State
    QVector<IconFontSet> m_fontSets;
    int m_currentFontSetIndex = 0;
    QString m_searchText;
    QString m_currentCategory;
    OutputFormat m_outputFormat = CssClass;
    int m_iconSize = 24;
    
    QVector<IconInfo> m_filteredIcons;
    IconInfo m_selectedIcon;
    QStringList m_recentIcons;
    QStringList m_favoriteIcons;
    
    QSettings* m_settings;
};
