#include "model_selector.hpp"
#include <QFileInfo>
#include <QToolTip>

ModelSelector::ModelSelector(QWidget* parent)
    : QWidget(parent)
    , m_status(IDLE)
{
    setupUI();
    applyDarkTheme();
}

void ModelSelector::setupUI()
{
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(4);
    
    // Status icon
    m_statusIcon = new QLabel(this);
    m_statusIcon->setFixedSize(16, 16);
    m_statusIcon->setText("◯");  // Empty circle for idle
    m_statusIcon->setToolTip("No model loaded");
    
    // Model dropdown
    m_modelCombo = new QComboBox(this);
    m_modelCombo->setMinimumWidth(200);
    m_modelCombo->setMaximumWidth(350);
    m_modelCombo->setPlaceholderText("Select model...");
    m_modelCombo->addItem("No model loaded");
    
    connect(m_modelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ModelSelector::onModelChanged);
    
    // Menu button (three dots)
    m_menuButton = new QPushButton("⋮", this);
    m_menuButton->setFixedSize(24, 24);
    m_menuButton->setFlat(true);
    m_menuButton->setToolTip("Model actions");
    
    // Model menu
    m_modelMenu = new QMenu(this);
    m_modelMenu->addAction("Load New Model...", this, &ModelSelector::loadNewModelRequested);
    m_modelMenu->addAction("Unload Current Model", this, &ModelSelector::unloadModelRequested);
    m_modelMenu->addSeparator();
    m_modelMenu->addAction("Model Info...", this, &ModelSelector::modelInfoRequested);
    
    connect(m_menuButton, &QPushButton::clicked, this, &ModelSelector::showModelMenu);
    
    layout->addWidget(m_statusIcon);
    layout->addWidget(m_modelCombo, 1);
    layout->addWidget(m_menuButton);
    
    setLayout(layout);
    updateStatus();
}

void ModelSelector::applyDarkTheme()
{
    setStyleSheet(R"(
        ModelSelector {
            background-color: transparent;
        }
        QComboBox {
            background-color: #3c3c3c;
            color: #e0e0e0;
            border: 1px solid #3e3e42;
            border-radius: 3px;
            padding: 4px 8px;
        }
        QComboBox:hover {
            background-color: #464646;
            border: 1px solid #007acc;
        }
        QComboBox::drop-down {
            border: none;
        }
        QComboBox::down-arrow {
            image: none;
            border-left: 4px solid transparent;
            border-right: 4px solid transparent;
            border-top: 5px solid #e0e0e0;
            margin-right: 8px;
        }
        QComboBox QAbstractItemView {
            background-color: #252526;
            color: #e0e0e0;
            selection-background-color: #094771;
            selection-color: #ffffff;
            border: 1px solid #3e3e42;
        }
        QPushButton {
            background-color: transparent;
            color: #e0e0e0;
            border: none;
            font-size: 16px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #3e3e42;
            border-radius: 3px;
        }
        QLabel {
            color: #e0e0e0;
            background-color: transparent;
        }
        QMenu {
            background-color: #252526;
            color: #e0e0e0;
            border: 1px solid #3e3e42;
        }
        QMenu::item {
            padding: 5px 20px;
        }
        QMenu::item:selected {
            background-color: #094771;
        }
    )");
}

void ModelSelector::addModel(const QString& modelPath, const QString& displayName)
{
    // Remove "No model loaded" placeholder if present
    if (m_modelCombo->count() == 1 && m_modelCombo->itemText(0) == "No model loaded") {
        m_modelCombo->clear();
    }
    
    // Check if model already exists
    if (m_modelPaths.contains(displayName)) {
        return;
    }
    
    m_modelPaths[displayName] = modelPath;
    m_modelCombo->addItem(displayName);
    
    // Automatically select if it's the first real model
    if (m_modelCombo->count() == 1) {
        m_modelCombo->setCurrentIndex(0);
    }
}

void ModelSelector::setCurrentModel(const QString& displayName)
{
    int index = m_modelCombo->findText(displayName);
    if (index >= 0) {
        m_modelCombo->setCurrentIndex(index);
        m_status = LOADED;
        updateStatus();
    }
}

void ModelSelector::setModelLoading(bool loading)
{
    m_status = loading ? LOADING : IDLE;
    updateStatus();
}

void ModelSelector::setModelError(const QString& error)
{
    m_status = ERROR_STATUS;
    m_errorMessage = error;
    updateStatus();
}

void ModelSelector::clearModels()
{
    m_modelCombo->clear();
    m_modelCombo->addItem("No model loaded");
    m_modelPaths.clear();
    m_status = IDLE;
    updateStatus();
}

QString ModelSelector::currentModel() const
{
    QString displayName = m_modelCombo->currentText();
    if (displayName == "No model loaded" || displayName.isEmpty()) {
        return QString();
    }
    return m_modelPaths.value(displayName);
}

void ModelSelector::onModelChanged(int index)
{
    if (index < 0 || m_modelCombo->itemText(index) == "No model loaded") {
        return;
    }
    
    QString displayName = m_modelCombo->itemText(index);
    QString modelPath = m_modelPaths.value(displayName);
    
    if (!modelPath.isEmpty()) {
        emit modelSelected(modelPath);
    }
}

void ModelSelector::showModelMenu()
{
    QPoint pos = m_menuButton->mapToGlobal(QPoint(0, m_menuButton->height()));
    m_modelMenu->exec(pos);
}

void ModelSelector::updateStatus()
{
    switch (m_status) {
        case IDLE:
            m_statusIcon->setText("◯");
            m_statusIcon->setStyleSheet("color: #858585;");
            m_statusIcon->setToolTip("No model loaded");
            break;
            
        case LOADING:
            m_statusIcon->setText("◐");
            m_statusIcon->setStyleSheet("color: #007acc;");
            m_statusIcon->setToolTip("Loading model...");
            break;
            
        case LOADED:
            m_statusIcon->setText("●");
            m_statusIcon->setStyleSheet("color: #4ec9b0;");  // Greenish cyan
            m_statusIcon->setToolTip("Model loaded successfully");
            break;
            
        case ERROR_STATUS:
            m_statusIcon->setText("●");
            m_statusIcon->setStyleSheet("color: #f48771;");  // Reddish
            m_statusIcon->setToolTip("ERROR_STATUS: " + m_errorMessage);
            break;
    }
}
