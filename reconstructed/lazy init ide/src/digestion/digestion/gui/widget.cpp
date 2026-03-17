// digestion_gui_widget.cpp
#include "digestion_gui_widget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QHeaderView>
#include <QLabel>

DigestionGuiWidget::DigestionGuiWidget(QWidget *parent) : QWidget(parent) {
    m_digester = new DigestionReverseEngineeringSystem(this);
    
    auto *layout = new QVBoxLayout(this);
    
    // Path selection
    auto *pathLayout = new QHBoxLayout;
    m_pathEdit = new QLineEdit(this);
    auto *browseBtn = new QPushButton("Browse...", this);
    pathLayout->addWidget(new QLabel("Root Directory:"));
    pathLayout->addWidget(m_pathEdit);
    pathLayout->addWidget(browseBtn);
    layout->addLayout(pathLayout);
    
    // Options
    auto *optionsLayout = new QHBoxLayout;
    m_applyFixesCheck = new QCheckBox("Apply Fixes", this);
    m_gitModeCheck = new QCheckBox("Git Mode Only", this);
    m_incrementalCheck = new QCheckBox("Incremental (Cached)", this);
    m_incrementalCheck->setChecked(true);
    optionsLayout->addWidget(m_applyFixesCheck);
    optionsLayout->addWidget(m_gitModeCheck);
    optionsLayout->addWidget(m_incrementalCheck);
    optionsLayout->addStretch();
    layout->addLayout(optionsLayout);
    
    // Progress
    m_progressBar = new QProgressBar(this);
    layout->addWidget(m_progressBar);
    
    // Results table
    m_resultsTable = new QTableWidget(this);
    m_resultsTable->setColumnCount(4);
    m_resultsTable->setHorizontalHeaderLabels({"File", "Language", "Stubs", "Status"});
    m_resultsTable->horizontalHeader()->setStretchLastSection(true);
    layout->addWidget(m_resultsTable);
    
    // Buttons
    auto *btnLayout = new QHBoxLayout;
    m_startBtn = new QPushButton("Start Digestion", this);
    m_stopBtn = new QPushButton("Stop", this);
    m_stopBtn->setEnabled(false);
    btnLayout->addWidget(m_startBtn);
    btnLayout->addWidget(m_stopBtn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);
    
    // Connections
    connect(browseBtn, &QPushButton::clicked, this, &DigestionGuiWidget::browseDirectory);
    connect(m_startBtn, &QPushButton::clicked, this, &DigestionGuiWidget::startDigestion);
    connect(m_stopBtn, &QPushButton::clicked, this, &DigestionGuiWidget::stopDigestion);
    connect(m_digester, &DigestionReverseEngineeringSystem::progressUpdate, 
            this, &DigestionGuiWidget::onProgress);
    connect(m_digester, &DigestionReverseEngineeringSystem::fileScanned, 
            this, &DigestionGuiWidget::onFileScanned);
    connect(m_digester, &DigestionReverseEngineeringSystem::pipelineFinished, 
            this, &DigestionGuiWidget::onFinished);
}

void DigestionGuiWidget::setRootDirectory(const QString &path) {
    m_pathEdit->setText(path);
}

void DigestionGuiWidget::browseDirectory() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Source Directory");
    if (!dir.isEmpty()) m_pathEdit->setText(dir);
}

void DigestionGuiWidget::startDigestion() {
    if (m_pathEdit->text().isEmpty()) return;
    
    m_resultsTable->setRowCount(0);
    m_startBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);
    
    DigestionConfig config;
    config.applyExtensions = m_applyFixesCheck->isChecked();
    config.useGitMode = m_gitModeCheck->isChecked();
    config.incremental = m_incrementalCheck->isChecked();
    config.chunkSize = 100; // GUI mode uses larger chunks
    
    m_digester->runFullDigestionPipeline(m_pathEdit->text(), config);
}

void DigestionGuiWidget::stopDigestion() {
    m_digester->stop();
    m_stopBtn->setEnabled(false);
    m_startBtn->setEnabled(true);
}

void DigestionGuiWidget::onProgress(int done, int total, int stubs, int percent) {
    m_progressBar->setMaximum(total);
    m_progressBar->setValue(done);
    m_progressBar->setFormat(QString("Scanned: %1/%2 | Stubs: %3 (%p%)").arg(done).arg(total).arg(stubs));
}

void DigestionGuiWidget::onFileScanned(const QString &path, const QString &lang, int stubs) {
    int row = m_resultsTable->rowCount();
    m_resultsTable->insertRow(row);
    m_resultsTable->setItem(row, 0, new QTableWidgetItem(path));
    m_resultsTable->setItem(row, 1, new QTableWidgetItem(lang));
    m_resultsTable->setItem(row, 2, new QTableWidgetItem(QString::number(stubs)));
    m_resultsTable->setItem(row, 3, new QTableWidgetItem(stubs > 0 ? "Found" : "Clean"));
    
    if (stubs > 0) {
        m_resultsTable->item(row, 2)->setBackground(Qt::yellow);
    }
}

void DigestionGuiWidget::onFinished(const QJsonObject &report, qint64 elapsed) {
    m_startBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    
    int stubs = report["statistics"].toObject()["stubs_found"].toInt();
    QMessageBox::information(this, "Digestion Complete", 
        QString("Scanned %1 files in %2 seconds\nFound %3 stubs")
        .arg(report["statistics"].toObject()["scanned_files"].toInt())
        .arg(elapsed / 1000.0, 0, 'f', 2)
        .arg(stubs));
}
