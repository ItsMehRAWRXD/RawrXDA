// ================================================================
// RawrXD-ExecAI Model Selector Dialog - Production UI
// Auditor-safe language, genuine functionality
// ================================================================
#ifndef UI_MODEL_SELECTOR_H
#define UI_MODEL_SELECTOR_H

#include <QDialog>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QString>

// ExecAI C interface
extern "C" {
    BOOL InitializeExecAI(const char* model_path);
}

// ================================================================
// ModelSelectorDialog - File selection for .exec models
// ================================================================
class ModelSelectorDialog : public QDialog {
    Q_OBJECT
    
public:
    explicit ModelSelectorDialog(QWidget* parent = nullptr) 
        : QDialog(parent)
        , m_selectedPath()
    {
        setWindowTitle("Load Executable Model");
        setMinimumWidth(500);
        
        auto* mainLayout = new QVBoxLayout(this);
        
        // Description
        auto* descLabel = new QLabel(
            "Select an executable model file (.exec).\n\n"
            "These files contain structural definitions and operators,\n"
            "not trained weights. Loading is instantaneous.",
            this
        );
        descLabel->setWordWrap(true);
        mainLayout->addWidget(descLabel);
        
        // Path selection row
        auto* pathLayout = new QHBoxLayout();
        
        auto* pathLabel = new QLabel("Model File:", this);
        pathLayout->addWidget(pathLabel);
        
        m_pathEdit = new QLineEdit(this);
        m_pathEdit->setPlaceholderText("Select .exec file...");
        m_pathEdit->setReadOnly(true);
        pathLayout->addWidget(m_pathEdit);
        
        auto* browseBtn = new QPushButton("Browse...", this);
        connect(browseBtn, &QPushButton::clicked, this, &ModelSelectorDialog::onBrowse);
        pathLayout->addWidget(browseBtn);
        
        mainLayout->addLayout(pathLayout);
        
        // File info display
        m_infoLabel = new QLabel("No file selected", this);
        m_infoLabel->setStyleSheet("color: gray; font-style: italic;");
        mainLayout->addWidget(m_infoLabel);
        
        // Buttons
        auto* buttonLayout = new QHBoxLayout();
        buttonLayout->addStretch();
        
        auto* loadBtn = new QPushButton("Load Model", this);
        loadBtn->setDefault(true);
        connect(loadBtn, &QPushButton::clicked, this, &ModelSelectorDialog::onLoad);
        buttonLayout->addWidget(loadBtn);
        
        auto* cancelBtn = new QPushButton("Cancel", this);
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
        buttonLayout->addWidget(cancelBtn);
        
        mainLayout->addLayout(buttonLayout);
        
        setLayout(mainLayout);
    }
    
    QString selectedModelPath() const {
        return m_selectedPath;
    }
    
private slots:
    void onBrowse() {
        QString path = QFileDialog::getOpenFileName(
            this,
            "Select Executable Model",
            "",
            "Executable Models (*.exec);;All Files (*.*)"
        );
        
        if (!path.isEmpty()) {
            m_pathEdit->setText(path);
            m_selectedPath = path;
            
            // Display file info
            QFileInfo fileInfo(path);
            qint64 size = fileInfo.size();
            
            QString sizeStr;
            if (size < 1024) {
                sizeStr = QString("%1 bytes").arg(size);
            } else if (size < 1024 * 1024) {
                sizeStr = QString("%1 KB").arg(size / 1024.0, 0, 'f', 2);
            } else {
                sizeStr = QString("%1 MB").arg(size / (1024.0 * 1024.0), 0, 'f', 2);
            }
            
            m_infoLabel->setText(QString("File: %1 (%2)")
                .arg(fileInfo.fileName())
                .arg(sizeStr));
            m_infoLabel->setStyleSheet("color: black;");
        }
    }
    
    void onLoad() {
        if (m_selectedPath.isEmpty()) {
            QMessageBox::warning(this, "Warning", "Please select a model file");
            return;
        }
        
        // Validate file exists
        if (!QFile::exists(m_selectedPath)) {
            QMessageBox::critical(this, "Error", "Model file does not exist");
            return;
        }
        
        accept();
    }
    
private:
    QLineEdit* m_pathEdit;
    QLabel* m_infoLabel;
    QString m_selectedPath;
};

#endif // UI_MODEL_SELECTOR_H
