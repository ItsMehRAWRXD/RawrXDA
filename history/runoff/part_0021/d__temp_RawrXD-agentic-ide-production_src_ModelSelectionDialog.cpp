#include "ModelSelectionDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QDebug>

ModelSelectionDialog::ModelSelectionDialog(ModelLoaderBridge* loader, QWidget* parent)
    : QDialog(parent), m_loader(loader) {
    
    setWindowTitle("Select GGUF Model");
    setMinimumWidth(500);
    setMinimumHeight(400);
    
    setupUI();
    populateModelList();
    
    // Connect model loaded signal
    connect(m_loader, &ModelLoaderBridge::modelLoaded,
            this, &ModelSelectionDialog::onModelLoaded);
}

void ModelSelectionDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout;
    
    // Status label
    m_statusLabel = new QLabel("Available Models:");
    mainLayout->addWidget(m_statusLabel);
    
    // Model list
    m_modelList = new QListWidget;
    mainLayout->addWidget(m_modelList);
    
    // Button layout
    QHBoxLayout* buttonLayout = new QHBoxLayout;
    
    m_loadButton = new QPushButton("Load Selected");
    m_refreshButton = new QPushButton("Refresh");
    m_okButton = new QPushButton("OK");
    
    buttonLayout->addWidget(m_loadButton);
    buttonLayout->addWidget(m_refreshButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_okButton);
    
    mainLayout->addLayout(buttonLayout);
    setLayout(mainLayout);
    
    // Connections
    connect(m_loadButton, &QPushButton::clicked, this, &ModelSelectionDialog::onLoadModel);
    connect(m_refreshButton, &QPushButton::clicked, this, &ModelSelectionDialog::onRefreshList);
    connect(m_okButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_modelList, &QListWidget::itemDoubleClicked, this, &ModelSelectionDialog::onModelDoubleClicked);
}

void ModelSelectionDialog::populateModelList() {
    m_modelList->clear();
    
    auto models = m_loader->getLoadedModels();
    for (const auto& model : models) {
        QString displayText = QString("%1 (%2 MB)")
            .arg(model.name)
            .arg(model.sizeBytes / (1024 * 1024));
        
        QListWidgetItem* item = new QListWidgetItem(displayText);
        m_modelList->addItem(item);
    }
    
    m_statusLabel->setText(QString("Available Models: %1").arg(models.size()));
}

void ModelSelectionDialog::onLoadModel() {
    QListWidgetItem* selected = m_modelList->currentItem();
    if (!selected) {
        QMessageBox::warning(this, "Error", "Please select a model first");
        return;
    }
    
    m_selectedModel = selected->text().split(" (")[0];
    m_loader->loadModelAsync(m_selectedModel);
}

void ModelSelectionDialog::onModelDoubleClicked(QListWidgetItem* item) {
    m_selectedModel = item->text().split(" (")[0];
    m_loader->loadModelAsync(m_selectedModel);
}

void ModelSelectionDialog::onRefreshList() {
    populateModelList();
}

void ModelSelectionDialog::onModelLoaded(const QString& name, uint64_t sizeBytes) {
    if (name == m_selectedModel) {
        m_modelLoaded = true;
        m_statusLabel->setText(QString("Loaded: %1 (%2 MB)").arg(name).arg(sizeBytes / (1024 * 1024)));
        QMessageBox::information(this, "Success", QString("Model loaded: %1").arg(name));
    }
}

QString ModelSelectionDialog::getSelectedModel() const {
    return m_selectedModel;
}

bool ModelSelectionDialog::isModelLoaded() const {
    return m_modelLoaded;
}
