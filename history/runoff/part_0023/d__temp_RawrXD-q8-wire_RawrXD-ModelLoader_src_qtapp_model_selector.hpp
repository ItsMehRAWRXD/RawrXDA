#pragma once
#include <QWidget>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QMenu>

/**
 * @brief Model Selector - Cursor-style model dropdown in toolbar
 * 
 * Features:
 * - Shows currently loaded model
 * - Dropdown to select from available models
 * - Quick access to load new models
 * - Visual indicator for model status (loaded/loading/error)
 */
class ModelSelector : public QWidget {
    Q_OBJECT

public:
    explicit ModelSelector(QWidget* parent = nullptr);
    
    void addModel(const QString& modelPath, const QString& displayName);
    void setCurrentModel(const QString& displayName);
    void setModelLoading(bool loading);
    void setModelError(const QString& error);
    void clearModels();
    
    QString currentModel() const;
    
signals:
    void modelSelected(const QString& modelPath);
    void loadNewModelRequested();
    void unloadModelRequested();
    void modelInfoRequested();
    
private slots:
    void onModelChanged(int index);
    void showModelMenu();
    
private:
    void setupUI();
    void applyDarkTheme();
    void updateStatus();
    
    QComboBox* m_modelCombo;
    QLabel* m_statusIcon;
    QPushButton* m_menuButton;
    QMenu* m_modelMenu;
    
    enum Status {
        IDLE,
        LOADING,
        LOADED,
        ERROR_STATUS  // Renamed to avoid Windows ERROR macro conflict
    };
    
    Status m_status;
    QString m_errorMessage;
    QHash<QString, QString> m_modelPaths;  // displayName -> fullPath
};
