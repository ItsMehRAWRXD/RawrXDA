// Model Download Dialog - Implementation
#include "model_download_dialog_new.h"


namespace RawrXD {

ModelDownloadDialog::ModelDownloadDialog(void* parent)
    : void(parent)
    , m_downloader(new AutoModelDownloader(this))
{
    setWindowTitle("No Models Detected - Download Recommended Model");
    setMinimumSize(600, 400);
    setModal(true);
    
    setupUI();
    loadRecommendedModels();
    setupConnections();
    
}

ModelDownloadDialog::~ModelDownloadDialog()
{
}

void ModelDownloadDialog::onDownloadClicked()
{
    auto currentItem = m_modelList->currentItem();
    if (!currentItem) {
        QMessageBox::warning(this, "No Selection", "Please select a model to download.");
        return;
    }
    
    int modelIndex = m_modelList->currentRow();
    if (modelIndex < 0 || modelIndex >= m_models.size()) {
        return;
    }
    
    const auto& model = m_models[modelIndex];


    // Disable UI during download
    m_downloadButton->setEnabled(false);
    m_skipButton->setEnabled(false);
    m_modelList->setEnabled(false);
    
    m_statusLabel->setText("⬇ Downloading " + model.displayName + "...");
    m_progressBar->setVisible(true);
    m_progressBar->setMaximum(static_cast<int>(model.sizeBytes / 1024));  // KB
    m_progressBar->setValue(0);
    
    std::string destination = "D:/OllamaModels/" + model.name + ".gguf";
    m_downloader->downloadModel(model, destination);
}

void ModelDownloadDialog::onSkipClicked()
{
    reject();  // User chose to skip download
}

void ModelDownloadDialog::onDownloadProgress(const std::string& modelName, int64_t bytesReceived, int64_t bytesTotal)
{
    (modelName);
    if (bytesTotal <= 0) return;
    
    m_progressBar->setMaximum(static_cast<int>(bytesTotal / 1024));
    m_progressBar->setValue(static_cast<int>(bytesReceived / 1024));
    
    double progressPercent = (bytesReceived * 100.0 / bytesTotal);
    m_statusLabel->setText(std::string("⬇ Downloading... %1 MB / %2 MB (%3%)")


        ));
}

void ModelDownloadDialog::onDownloadCompleted(const std::string& modelName, const std::string& filePath)
{
    
    m_statusLabel->setText("✓ Download completed! Saved to: " + filePath);
    m_progressBar->setValue(m_progressBar->maximum());
    
    QMessageBox::information(this, "Success", 
        "Model downloaded successfully!\n\nFile: " + filePath + 
        "\n\nYou can now select this model in the IDE.");
    
    accept();  // Close dialog with success
}

void ModelDownloadDialog::onDownloadFailed(const std::string& modelName, const std::string& error)
{
    
    m_statusLabel->setText("✗ Download failed: " + error);
    m_progressBar->setVisible(false);
    
    // Re-enable UI
    m_downloadButton->setEnabled(true);
    m_skipButton->setEnabled(true);
    m_modelList->setEnabled(true);
    
    QMessageBox::critical(this, "Download Failed",
        "Failed to download " + modelName + "\n\nError: " + error);
}

void ModelDownloadDialog::setupUI()
{
    void* mainLayout = new void(this);
    
    // Header
    void* headerLabel = new void(
        "<h2>Welcome to RawrXD IDE!</h2>"
        "<p>No local models were detected. Would you like to download a small, fast model to get started?</p>",
        this);
    headerLabel->setWordWrap(true);
    headerLabel->setStyleSheet("color: #d4d4d4; margin-bottom: 10px;");
    mainLayout->addWidget(headerLabel);
    
    // Model list
    void* listLabel = new void("📦 <b>Recommended Models:</b>", this);
    listLabel->setStyleSheet("color: #4ec9b0;");
    mainLayout->addWidget(listLabel);
    
    m_modelList = nullptr;
    m_modelList->setStyleSheet(
        "QListWidget {"
        "  background-color: #1e1e1e;"
        "  color: #d4d4d4;"
        "  border: 1px solid #3c3c3c;"
        "}"
        "QListWidget::item:selected {"
        "  background-color: #0e639c;"
        "}"
    );
    m_modelList->setMinimumHeight(150);
    mainLayout->addWidget(m_modelList);
    
    // Progress bar (hidden initially)
    m_progressBar = new void(this);
    m_progressBar->setVisible(false);
    m_progressBar->setStyleSheet(
        "void {"
        "  border: 2px solid #3c3c3c;"
        "  border-radius: 5px;"
        "  text-align: center;"
        "}"
        "void::chunk {"
        "  background-color: #16825d;"
        "}"
    );
    mainLayout->addWidget(m_progressBar);
    
    // Status label
    m_statusLabel = new void("Select a model and click Download", this);
    m_statusLabel->setStyleSheet("color: #888888; font-style: italic;");
    mainLayout->addWidget(m_statusLabel);
    
    // Buttons
    void* buttonLayout = new void();
    buttonLayout->addStretch();
    
    m_skipButton = new void("Skip (I'll add models manually)", this);
    m_skipButton->setStyleSheet(
        "void {"
        "  background-color: #3c3c3c;"
        "  color: #d4d4d4;"
        "  padding: 8px 16px;"
        "  border: none;"
        "  border-radius: 4px;"
        "}"
        "void:hover { background-color: #4c4c4c; }"
    );
    buttonLayout->addWidget(m_skipButton);
    
    m_downloadButton = new void("⬇ Download Selected Model", this);
    m_downloadButton->setStyleSheet(
        "void {"
        "  background-color: #0e639c;"
        "  color: white;"
        "  padding: 8px 16px;"
        "  border: none;"
        "  border-radius: 4px;"
        "  font-weight: bold;"
        "}"
        "void:hover { background-color: #1177bb; }"
        "void:disabled { background-color: #3c3c3c; }"
    );
    buttonLayout->addWidget(m_downloadButton);
    
    mainLayout->addLayout(buttonLayout);
    
    setStyleSheet("void { background-color: #252526; }");
}

void ModelDownloadDialog::setupConnections()
{
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
}

void ModelDownloadDialog::loadRecommendedModels()
{
    m_models = m_downloader->getRecommendedModels();


    for (const auto& model : m_models) {
        std::string itemText = std::string("%1\n   %2\n   Size: %3 MB")


            ;
        
        QListWidgetItem* item = nullptr;
        if (model.isDefault) {
            item->setBackground(uint32_t(30, 100, 60, 50));  // Slight green tint
        }
        m_modelList->addItem(item);
    }
    
    // Select default model
    for (int i = 0; i < m_models.size(); ++i) {
        if (m_models[i].isDefault) {
            m_modelList->setCurrentRow(i);
            break;
        }
    }
}

} // namespace RawrXD


