/**
 * @file design_to_code_widget.h
 * @brief Header for DesignToCodeWidget - design system viewer and component library browser
 */

#pragma once

#include <QWidget>
#include <QMap>
#include <QString>
#include <QList>
#include <QJsonObject>
#include <QThread>

class QVBoxLayout;
class QHBoxLayout;
class QSplitter;
class QTreeWidget;
class QTreeWidgetItem;
class QListWidget;
class QTextEdit;
class QTabWidget;
class QPushButton;
class QLabel;
class QComboBox;
class QLineEdit;
class QGroupBox;
class QScrollArea;
class QGridLayout;

struct DesignComponent {
    QString id;
    QString name;
    QString category;
    QString description;
    QJsonObject properties;
    QJsonObject styles;
    QString previewImage;
    QString codeSnippet;
    QList<QString> tags;
};

struct DesignSystem {
    QString name;
    QString version;
    QMap<QString, DesignComponent> components;
    QMap<QString, QString> categories;
    QString theme;
};

class CodeGenerator : public QObject {
    Q_OBJECT
public:
    explicit CodeGenerator(QObject* parent = nullptr);
    ~CodeGenerator() override = default;

    void generateCode(const DesignComponent& component, const QString& targetFramework, const QJsonObject& options);

signals:
    void codeGenerated(const QString& code, const QString& language);
    void generationError(const QString& error);

private:
    QString generateQtCode(const DesignComponent& component, const QJsonObject& options);
    QString generateHtmlCode(const DesignComponent& component, const QJsonObject& options);
    QString generateReactCode(const DesignComponent& component, const QJsonObject& options);
    QString generateFlutterCode(const DesignComponent& component, const QJsonObject& options);
};

class DesignToCodeWidget : public QWidget {
    Q_OBJECT
public:
    explicit DesignToCodeWidget(QWidget* parent = nullptr);
    ~DesignToCodeWidget() override;

    // Design system management
    bool loadDesignSystem(const QString& filePath);
    bool saveDesignSystem(const QString& filePath = QString());
    void createNewDesignSystem(const QString& name);

    // Component management
    void addComponent(const DesignComponent& component);
    void removeComponent(const QString& componentId);
    void updateComponent(const QString& componentId, const DesignComponent& component);

    // Code generation
    void generateCodeForComponent(const QString& componentId);
    void generateCodeForSelection();

    // Import/Export
    void importFromFigma(const QString& figmaFile);
    void exportToFramework(const QString& framework);

signals:
    void designSystemLoaded(const QString& name);
    void designSystemSaved(const QString& path);
    void componentSelected(const QString& componentId);
    void codeGenerated(const QString& code, const QString& language);
    void error(const QString& message);

public slots:
    void refresh();
    void onCodeGenerated(const QString& code, const QString& language);
    void onGenerationError(const QString& error);

private slots:
    void onNewDesignSystem();
    void onOpenDesignSystem();
    void onSaveDesignSystem();
    void onImportDesign();
    void onExportCode();
    void onAddComponent();
    void onEditComponent();
    void onDeleteComponent();
    void onComponentSelected(QTreeWidgetItem* item, int column);
    void onCategoryChanged(const QString& category);
    void onSearchTextChanged(const QString& text);
    void onFrameworkChanged(const QString& framework);
    void onGenerateCode();
    void onPreviewComponent();

private:
    void setupUI();
    void setupToolbar();
    void setupMainArea();
    void setupSidebar();
    void setupConnections();

    void updateComponentTree();
    void updateComponentList();
    void updateComponentPreview();
    void updateCodePreview();
    void updateCategories();
    void updateDesignSystemInfo();
    void showContextMenu(const QPoint& pos);

    void loadComponentDetails(const QString& componentId);
    void showComponentProperties(const DesignComponent& component);
    void showComponentStyles(const DesignComponent& component);

    QString getComponentPreviewHtml(const DesignComponent& component) const;
    QPixmap generateComponentPreview(const DesignComponent& component) const;

    void saveState();
    void restoreState();

    // UI components
    QVBoxLayout* mainLayout_;
    QWidget* toolbarWidget_;
    QSplitter* mainSplitter_;

    // Toolbar
    QPushButton* newDesignBtn_;
    QPushButton* openDesignBtn_;
    QPushButton* saveDesignBtn_;
    QPushButton* importBtn_;
    QPushButton* exportBtn_;
    QLineEdit* searchEdit_;
    QComboBox* categoryCombo_;
    QComboBox* frameworkCombo_;

    // Main area
    QSplitter* contentSplitter_;
    QTreeWidget* componentTree_;
    QListWidget* componentList_;
    QTabWidget* detailsTabs_;

    // Component details tabs
    QWidget* propertiesTab_;
    QWidget* stylesTab_;
    QWidget* codeTab_;
    QWidget* previewTab_;

    // Property editors
    QTextEdit* propertiesEditor_;
    QTextEdit* stylesEditor_;
    QTextEdit* codeEditor_;
    QScrollArea* previewArea_;

    // Sidebar
    QGroupBox* designSystemInfo_;
    QLabel* systemNameLabel_;
    QLabel* systemVersionLabel_;
    QLabel* componentCountLabel_;
    QListWidget* categoryList_;

    // Code generator
    CodeGenerator* codeGenerator_;
    QThread* generatorThread_;

    // Data
    DesignSystem currentDesignSystem_;
    QString currentDesignSystemPath_;
    QString selectedComponentId_;
    QString currentFramework_;
    QStringList supportedFrameworks_;
};
