// Model Download Dialog - User-friendly interface for auto-downloading models
#include "auto_model_downloader.h"
/* Qt removed */
/* Qt removed */
/* Qt removed */
/* Qt removed */
/* Qt removed */
/* Qt removed */
/* Qt removed */
/* Qt removed */

namespace RawrXD {

class ModelDownloadDialog : public QDialog {
    /* /* Q_OBJECT */ */

public:
    explicit ModelDownloadDialog(void* parent = nullptr)
        : QDialog(parent)
        , m_downloader(new AutoModelDownloader(this))
    {
        setWindowTitle("No Models Detected - Download Recommended Model");
        setMinimumSize(600, 400);
        setModal(true);
        
        setupUI();
        loadRecommendedModels();
        setupConnections();
    }

private /* slots */ public:
    void onDownloadClicked() {
        auto currentItem = m_modelList->currentItem();
        if (!currentItem) {
            QMessageBox::warning(this, "No Selection", "Please select a model to download.");
            return;
        }
        
        int modelIndex = m_modelList->currentRow();
        if (modelIndex < 0 || modelIndex >= m_models.size()) return;
        
        const auto& model = m_models[modelIndex];
        
        // Disable UI during download
        m_downloadButton->setEnabled(false);
        m_skipButton->setEnabled(false);
        m_modelList->setEnabled(false);
        
        m_statusLabel->setText("⬇ Downloading " + model.displayName + "...");
        m_progressBar->setVisible(true);
        m_progressBar->setMaximum(static_cast<int>(model.sizeBytes / 1024));  // KB
        m_progressBar->setValue(0);
        
        std::wstring destination = "D:/OllamaModels/" + model.name + ".gguf";
        m_downloader->downloadModel(model, destination);
    }
    
    void onSkipClicked() {
        reject();  // User chose to skip download
    }
    
    void onDownloadProgress(const std::wstring& modelName, qint64 bytesReceived, qint64 bytesTotal) {
        Q_UNUSED(modelName);
        m_progressBar->setMaximum(static_cast<int>(bytesTotal / 1024));
        m_progressBar->setValue(static_cast<int>(bytesReceived / 1024));
        
        double progressPercent = (bytesTotal > 0) ? (bytesReceived * 100.0 / bytesTotal) : 0;
        m_statusLabel->setText(std::wstring("⬇ Downloading... %1 MB / %2 MB (%3%)")
            .arg(bytesReceived / 1024 / 1024)
            .arg(bytesTotal / 1024 / 1024)
            .arg(std::wstring::number(progressPercent, 'f', 1)));
    }
    
    void onDownloadCompleted(const std::wstring& modelName, const std::wstring& filePath) {
        Q_UNUSED(modelName);
        m_statusLabel->setText("✓ Download completed! Saved to: " + filePath);
        m_progressBar->setValue(m_progressBar->maximum());
        
        QMessageBox::information(this, "Success", 
            "Model downloaded successfully!\n\nFile: " + filePath + 
            "\n\nYou can now select this model in the IDE.");
        
        accept();  // Close dialog with success
    }
    
    void onDownloadFailed(const std::wstring& modelName, const std::wstring& error) {
        m_statusLabel->setText("✗ Download failed: " + error);
        m_progressBar->setVisible(false);
        
        // Re-enable UI
        m_downloadButton->setEnabled(true);
        m_skipButton->setEnabled(true);
        m_modelList->setEnabled(true);
        
        QMessageBox::critical(this, "Download Failed",
            "Failed to download " + modelName + "\n\nError: " + error);
    }

private:
    void setupUI() {
        void* mainLayout = new QVBoxLayout(this);
        
        // Header
        void* headerLabel = new QLabel(
            "<h2>Welcome to RawrXD IDE!</h2>"
            "<p>No local models were detected. Would you like to download a small, fast model to get started?</p>",
            this);
        headerLabel->setWordWrap(true);
        headerLabel->setStyleSheet("color: #d4d4d4; margin-bottom: 10px;");
        mainLayout->addWidget(headerLabel);
        
        // Model list
        void* listLabel = new QLabel("📦 <b>Recommended Models:</b>", this);
        listLabel->setStyleSheet("color: #4ec9b0;");
        mainLayout->addWidget(listLabel);
        
        m_modelList = new QListWidget(this);
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
        mainLayout->addWidget(m_modelList);
        
        // Progress bar
        m_progressBar = new QProgressBar(this);
        m_progressBar->setVisible(false);
        m_progressBar->setStyleSheet(
            "QProgressBar {"
            "  border: 2px solid #3c3c3c;"
            "  border-radius: 5px;"
            "  text-align: center;"
            "}"
            "QProgressBar::chunk {"
            "  background-color: #16825d;"
            "}"
        );
        mainLayout->addWidget(m_progressBar);
        
        // Status label
        m_statusLabel = new QLabel("Select a model and click Download", this);
        m_statusLabel->setStyleSheet("color: #888888; font-style: italic;");
        mainLayout->addWidget(m_statusLabel);
        
        // Buttons
        void* buttonLayout = new QHBoxLayout();
        buttonLayout->addStretch();
        
        m_skipButton = new QPushButton("Skip (I'll add models manually)", this);
        m_skipButton->setStyleSheet(
            "QPushButton {"
            "  background-color: #3c3c3c;"
            "  color: #d4d4d4;"
            "  padding: 8px 16px;"
            "  border: none;"
            "  border-radius: 4px;"
            "}"
            "QPushButton:hover { background-color: #4c4c4c; }"
        );
        buttonLayout->addWidget(m_skipButton);
        
        m_downloadButton = new QPushButton("⬇ Download Selected Model", this);
        m_downloadButton->setStyleSheet(
            "QPushButton {"
            "  background-color: #0e639c;"
            "  color: white;"
            "  padding: 8px 16px;"
            "  border: none;"
            "  border-radius: 4px;"
            "  font-weight: bold;"
            "}"
            "QPushButton:hover { background-color: #1177bb; }"
            "QPushButton:disabled { background-color: #3c3c3c; }"
        );
        buttonLayout->addWidget(m_downloadButton);
        
        mainLayout->addLayout(buttonLayout);
        
        setStyleSheet("QDialog { background-color: #252526; }");
    }
    
    void setupConnections() {
        connect(m_downloadButton, &QPushButton::clicked, this, &ModelDownloadDialog::onDownloadClicked);
        connect(m_skipButton, &QPushButton::clicked, this, &ModelDownloadDialog::onSkipClicked);
        
        connect(m_downloader, &AutoModelDownloader::downloadProgress,
                this, &ModelDownloadDialog::onDownloadProgress);
        connect(m_downloader, &AutoModelDownloader::downloadCompleted,
                this, &ModelDownloadDialog::onDownloadCompleted);
        connect(m_downloader, &AutoModelDownloader::downloadFailed,
                this, &ModelDownloadDialog::onDownloadFailed);
    }
    
    void loadRecommendedModels() {
        m_models = m_downloader->getRecommendedModels();
        
        for (const auto& model : m_models) {
            std::wstring itemText = std::wstring("%1\n   %2\n   Size: %3 MB")
                .arg(model.displayName)
                .arg(model.description)
                .arg(model.sizeBytes / 1024 / 1024);
            
            void* item = new QListWidgetItem(itemText);
            if (model.isDefault) {
                item->setBackground(QColor(30, 100, 60, 50));  // Slight green tint
                item->setIcon(QIcon());  // Could add a star icon
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
    
    void* m_modelList{nullptr};
    void* m_progressBar{nullptr};
    void* m_statusLabel{nullptr};
    void* m_downloadButton{nullptr};
    void* m_skipButton{nullptr};
    
    AutoModelDownloader* m_downloader{nullptr};
    std::vector<ModelDownloadInfo> m_models;
};

} // namespace RawrXD
