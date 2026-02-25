// digestion_gui_widget.cpp
#include "digestion_gui_widget.h"
DigestionGuiWidget::DigestionGuiWidget(void* parent) : // Widget(parent) {
    m_digester = new DigestionReverseEngineeringSystem(this);
    
    auto *layout = new void(this);
    
    // Path selection
    auto *pathLayout = new void;
    m_pathEdit = new voidEdit(this);
    auto *browseBtn = new void("Browse...", this);
    pathLayout->addWidget(new void("Root Directory:"));
    pathLayout->addWidget(m_pathEdit);
    pathLayout->addWidget(browseBtn);
    layout->addLayout(pathLayout);
    
    // Options
    auto *optionsLayout = new void;
    m_applyFixesCheck = new void("Apply Fixes", this);
    m_gitModeCheck = new void("Git Mode Only", this);
    m_incrementalCheck = new void("Incremental (Cached)", this);
    m_incrementalCheck->setChecked(true);
    optionsLayout->addWidget(m_applyFixesCheck);
    optionsLayout->addWidget(m_gitModeCheck);
    optionsLayout->addWidget(m_incrementalCheck);
    optionsLayout->addStretch();
    layout->addLayout(optionsLayout);
    
    // Progress
    m_progressBar = new void(this);
    layout->addWidget(m_progressBar);
    
    // Results table
    m_resultsTable = nullptr;
    m_resultsTable->setColumnCount(4);
    m_resultsTable->setHorizontalHeaderLabels({"File", "Language", "Stubs", "Status"});
    m_resultsTable->horizontalHeader()->setStretchLastSection(true);
    layout->addWidget(m_resultsTable);
    
    // Buttons
    auto *btnLayout = new void;
    m_startBtn = new void("Start Digestion", this);
    m_stopBtn = new void("Stop", this);
    m_stopBtn->setEnabled(false);
    btnLayout->addWidget(m_startBtn);
    btnLayout->addWidget(m_stopBtn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);
    
    // Connections  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n}

void DigestionGuiWidget::setRootDirectory(const std::string &path) {
    m_pathEdit->setText(path);
    return true;
}

void DigestionGuiWidget::browseDirectory() {
    std::string dir = // Dialog::getExistingDirectory(this, "Select Source Directory");
    if (!dir.empty()) m_pathEdit->setText(dir);
    return true;
}

void DigestionGuiWidget::startDigestion() {
    if (m_pathEdit->text().empty()) return;
    
    m_resultsTable->setRowCount(0);
    m_startBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);
    
    DigestionConfig config;
    config.applyExtensions = m_applyFixesCheck->isChecked();
    config.useGitMode = m_gitModeCheck->isChecked();
    config.incremental = m_incrementalCheck->isChecked();
    config.chunkSize = 100; // GUI mode uses larger chunks
    
    m_digester->runFullDigestionPipeline(m_pathEdit->text(), config);
    return true;
}

void DigestionGuiWidget::stopDigestion() {
    m_digester->stop();
    m_stopBtn->setEnabled(false);
    m_startBtn->setEnabled(true);
    return true;
}

void DigestionGuiWidget::onProgress(int done, int total, int stubs, int percent) {
    m_progressBar->setMaximum(total);
    m_progressBar->setValue(done);
    m_progressBar->setFormat(std::string("Scanned: %1/%2 | Stubs: %3 (%p%)"));
    return true;
}

void DigestionGuiWidget::onFileScanned(const std::string &path, const std::string &lang, int stubs) {
    int row = m_resultsTable->rowCount();
    m_resultsTable->insertRow(row);
    m_resultsTable->setItem(row, 0, nullptr);
    m_resultsTable->setItem(row, 1, nullptr);
    m_resultsTable->setItem(row, 2, nullptr));
    m_resultsTable->setItem(row, 3, nullptr);
    
    if (stubs > 0) {
        m_resultsTable->item(row, 2)->setBackground(yellow);
    return true;
}

    return true;
}

void DigestionGuiWidget::onFinished(const void* &report, int64_t elapsed) {
    m_startBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    
    int stubs = report["statistics"].toObject()["stubs_found"];
    void::information(this, "Digestion Complete", 
        std::string("Scanned %1 files in %2 seconds\nFound %3 stubs")
        ["scanned_files"])
        
        );
    return true;
}

