#include "ProjectAnalysisDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QApplication>
#include <QScreen>
#include <QDateTime>
#include <QMessageBox>
#include <QFile>
#include <filesystem>
#include <map>
#include <sstream>
#include <iomanip>

// ProjectAnalysisWorker implementation
void ProjectAnalysisWorker::analyzeProject(const QString& projectPath, bool excludeArtifacts) {
    if (projectPath.isEmpty()) {
        emit error("Project path is empty");
        emit finished();
        return;
    }

    QDir dir(projectPath);
    if (!dir.exists()) {
        emit error("Project directory does not exist: " + projectPath);
        emit finished();
        return;
    }

    try {
        QMap<QString, int> extensionCounts;
        QString results = analyzeDirectory(projectPath, extensionCounts, excludeArtifacts);
        emit resultsReady(results);
    } catch (const std::exception& e) {
        emit error(QString::fromStdString(std::string(e.what())));
    }

    emit finished();
}

QString ProjectAnalysisWorker::analyzeDirectory(const QString& path, QMap<QString, int>& extensionCounts, bool excludeArtifacts) {
    namespace fs = std::filesystem;

    std::stringstream ss;
    ss << "=== Project Analysis Results ===\n";
    ss << "Path: " << path.toStdString() << "\n";
    ss << "Generated: " << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss").toStdString() << "\n\n";

    try {
        int fileCount = 0;
        qint64 totalSize = 0;
        int totalLines = 0;
        QMap<QString, std::pair<int, qint64>> stats; // ext -> (count, size)

        // Artifact extensions to skip
        QSet<QString> artifactExts = {".obj", ".tlog", ".exe", ".dll", ".lib", ".pdb", ".spv"};

        for (const auto& entry : fs::recursive_directory_iterator(path.toStdString())) {
            if (!entry.is_regular_file()) continue;

            QString filePath = QString::fromStdString(entry.path().string());
            QFileInfo fileInfo(filePath);
            QString ext = fileInfo.suffix().isEmpty() ? "(no ext)" : fileInfo.suffix();

            // Skip artifacts if requested
            if (excludeArtifacts && artifactExts.contains("." + ext)) {
                continue;
            }

            qint64 size = fileInfo.size();
            fileCount++;
            totalSize += size;

            // Count extension
            extensionCounts[ext]++;
            auto& stat = stats[ext];
            stat.first++;
            stat.second += size;

            // Count lines for text files
            if (ext == "cpp" || ext == "h" || ext == "hpp" || ext == "c" || ext == "py" || ext == "js" || ext == "txt" || ext == "md") {
                QFile file(filePath);
                if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    int lines = 0;
                    while (!file.atEnd()) {
                        file.readLine();
                        lines++;
                    }
                    totalLines += lines;
                    file.close();
                }
            }
        }

        // Format output
        ss << "Total Files: " << fileCount << "\n";
        ss << "Total Size: " << (totalSize / (1024.0 * 1024.0)) << " MB\n";
        ss << "Total Lines: " << totalLines << "\n\n";

        ss << "=== Extension Breakdown ===\n";
        ss << std::left << std::setw(20) << "Extension" << std::setw(10) << "Count" 
           << std::setw(15) << "Size (MB)" << std::setw(10) << "Percent\n";
        ss << std::string(55, '-') << "\n";

        for (auto it = stats.begin(); it != stats.end(); ++it) {
            QString ext = it.key();
            int count = it.value().first;
            qint64 size = it.value().second;
            double percent = (100.0 * size) / totalSize;

            ss << std::left << std::setw(20) << ext.toStdString()
               << std::setw(10) << count
               << std::setw(15) << std::fixed << std::setprecision(2) << (size / (1024.0 * 1024.0))
               << std::setw(10) << std::fixed << std::setprecision(1) << percent << "%\n";
        }

        ss << "\n=== Top Directories by Size ===\n";
        std::map<std::string, qint64> dirSizes;
        for (const auto& entry : fs::recursive_directory_iterator(path.toStdString())) {
            if (!entry.is_regular_file()) continue;
            auto parentDir = entry.path().parent_path().string();
            dirSizes[parentDir] += entry.file_size();
        }

        std::vector<std::pair<std::string, qint64>> sortedDirs(dirSizes.begin(), dirSizes.end());
        std::sort(sortedDirs.begin(), sortedDirs.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });

        for (size_t i = 0; i < std::min(size_t(10), sortedDirs.size()); ++i) {
            ss << sortedDirs[i].first << ": "
               << std::fixed << std::setprecision(2) << (sortedDirs[i].second / (1024.0 * 1024.0)) << " MB\n";
        }

    } catch (const std::exception& e) {
        ss << "Error during analysis: " << e.what() << "\n";
    }

    return QString::fromStdString(ss.str());
}

// ProjectAnalysisDialog implementation
ProjectAnalysisDialog::ProjectAnalysisDialog(QWidget* parent)
    : QDialog(parent), m_worker(nullptr), m_workerThread(nullptr) {
    setWindowTitle("Project Analysis");
    setWindowFlags(Qt::Window | Qt::WindowCloseButtonHint);
    setMinimumWidth(700);
    setMinimumHeight(600);

    setupUI();
    setupConnections();

    // Center on screen (Qt6 compatible)
    if (QScreen* screen = QApplication::primaryScreen()) {
        move(screen->geometry().center() - frameGeometry().center());
    }
}

ProjectAnalysisDialog::~ProjectAnalysisDialog() {
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait();
    }
}

void ProjectAnalysisDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Path selection group
    QGroupBox* pathGroup = new QGroupBox("Project Path", this);
    QHBoxLayout* pathLayout = new QHBoxLayout(pathGroup);
    m_projectPathEdit = new QLineEdit(this);
    m_projectPathEdit->setPlaceholderText("Enter or select project directory...");
    m_browseButton = new QPushButton("Browse...", this);
    pathLayout->addWidget(m_projectPathEdit);
    pathLayout->addWidget(m_browseButton);
    mainLayout->addWidget(pathGroup);

    // Options group
    QGroupBox* optionsGroup = new QGroupBox("Options", this);
    QVBoxLayout* optionsLayout = new QVBoxLayout(optionsGroup);
    m_excludeArtifactsCheckbox = new QCheckBox("Exclude build artifacts (.obj, .dll, .exe, etc.)", this);
    m_excludeArtifactsCheckbox->setChecked(true);
    optionsLayout->addWidget(m_excludeArtifactsCheckbox);
    mainLayout->addWidget(optionsGroup);

    // Analysis controls
    QHBoxLayout* controlLayout = new QHBoxLayout();
    m_analyzeButton = new QPushButton("Analyze Project", this);
    controlLayout->addStretch();
    controlLayout->addWidget(m_analyzeButton);
    mainLayout->addLayout(controlLayout);

    // Progress
    m_progressBar = new QProgressBar(this);
    m_progressBar->setVisible(false);
    m_statusLabel = new QLabel("Ready", this);
    mainLayout->addWidget(m_statusLabel);
    mainLayout->addWidget(m_progressBar);

    // Results display
    QGroupBox* resultsGroup = new QGroupBox("Analysis Results", this);
    QVBoxLayout* resultsLayout = new QVBoxLayout(resultsGroup);
    m_resultsDisplay = new QTextEdit(this);
    m_resultsDisplay->setReadOnly(true);
    resultsLayout->addWidget(m_resultsDisplay);
    mainLayout->addWidget(resultsGroup);

    // Action buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_exportButton = new QPushButton("Export Results", this);
    m_cleanupButton = new QPushButton("Clean Artifacts", this);
    QPushButton* closeButton = new QPushButton("Close", this);
    buttonLayout->addWidget(m_exportButton);
    buttonLayout->addWidget(m_cleanupButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButton);
    mainLayout->addLayout(buttonLayout);

    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

    setLayout(mainLayout);
}

void ProjectAnalysisDialog::setupConnections() {
    connect(m_browseButton, &QPushButton::clicked, this, &ProjectAnalysisDialog::onBrowseClicked);
    connect(m_analyzeButton, &QPushButton::clicked, this, &ProjectAnalysisDialog::onAnalyzeClicked);
    connect(m_exportButton, &QPushButton::clicked, this, &ProjectAnalysisDialog::onExportClicked);
    connect(m_cleanupButton, &QPushButton::clicked, this, &ProjectAnalysisDialog::onCleanupClicked);
}

void ProjectAnalysisDialog::onBrowseClicked() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Project Directory");
    if (!dir.isEmpty()) {
        m_projectPathEdit->setText(dir);
    }
}

void ProjectAnalysisDialog::onAnalyzeClicked() {
    QString projectPath = m_projectPathEdit->text();
    if (projectPath.isEmpty()) {
        QMessageBox::warning(this, "Warning", "Please select a project directory");
        return;
    }

    enableAnalysisControls(false);
    m_progressBar->setVisible(true);
    m_statusLabel->setText("Analyzing project...");

    // Create worker thread if not exists
    if (!m_workerThread) {
        m_worker = new ProjectAnalysisWorker();
        m_workerThread = new QThread(this);
        m_worker->moveToThread(m_workerThread);

        connect(m_worker, &ProjectAnalysisWorker::progress, this, &ProjectAnalysisDialog::onAnalysisProgress);
        connect(m_worker, &ProjectAnalysisWorker::resultsReady, this, &ProjectAnalysisDialog::onAnalysisResults);
        connect(m_worker, &ProjectAnalysisWorker::finished, this, &ProjectAnalysisDialog::onAnalysisFinished);
        connect(m_worker, &ProjectAnalysisWorker::error, this, &ProjectAnalysisDialog::onAnalysisError);
    }

    m_lastProjectPath = projectPath;

    // Start analysis
    QMetaObject::invokeMethod(m_worker, [this, projectPath]() {
        m_worker->analyzeProject(projectPath, m_excludeArtifactsCheckbox->isChecked());
    }, Qt::QueuedConnection);

    m_workerThread->start();
}

void ProjectAnalysisDialog::onAnalysisProgress(int current, int total) {
    m_progressBar->setMaximum(total);
    m_progressBar->setValue(current);
}

void ProjectAnalysisDialog::onAnalysisResults(const QString& results) {
    m_resultsDisplay->setText(results);
    m_lastResults = results;
}

void ProjectAnalysisDialog::onAnalysisFinished() {
    m_statusLabel->setText("Analysis complete");
    m_progressBar->setVisible(false);
    enableAnalysisControls(true);
}

void ProjectAnalysisDialog::onAnalysisError(const QString& errorMsg) {
    m_statusLabel->setText("Error: " + errorMsg);
    m_progressBar->setVisible(false);
    enableAnalysisControls(true);
    QMessageBox::critical(this, "Analysis Error", errorMsg);
}

void ProjectAnalysisDialog::onExportClicked() {
    if (m_lastResults.isEmpty()) {
        QMessageBox::warning(this, "Warning", "No results to export");
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(this, "Export Analysis Results", "",
                                                     "Text Files (*.txt);;All Files (*)");
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(m_lastResults.toUtf8());
            file.close();
            QMessageBox::information(this, "Success", "Results exported to: " + fileName);
        }
    }
}

void ProjectAnalysisDialog::onCleanupClicked() {
    if (m_lastProjectPath.isEmpty()) {
        QMessageBox::warning(this, "Warning", "No project loaded");
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(this, "Cleanup",
                                                               "Remove build artifacts (.obj, .exe, .dll, etc.)?\n\nThis cannot be undone!",
                                                               QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        try {
            namespace fs = std::filesystem;
            QSet<QString> artifactExts = {".obj", ".tlog", ".exe", ".dll", ".lib", ".pdb", ".spv", ".ilk"};
            int removed = 0;

            for (const auto& entry : fs::recursive_directory_iterator(m_lastProjectPath.toStdString())) {
                if (!entry.is_regular_file()) continue;

                QString filePath = QString::fromStdString(entry.path().string());
                QFileInfo fileInfo(filePath);
                QString ext = "." + fileInfo.suffix();

                if (artifactExts.contains(ext)) {
                    if (QFile::remove(filePath)) {
                        removed++;
                    }
                }
            }

            QMessageBox::information(this, "Cleanup Complete", QString("Removed %1 artifact files").arg(removed));
        } catch (const std::exception& e) {
            QMessageBox::critical(this, "Cleanup Error", QString::fromStdString(std::string(e.what())));
        }
    }
}

void ProjectAnalysisDialog::enableAnalysisControls(bool enable) {
    m_projectPathEdit->setEnabled(enable);
    m_browseButton->setEnabled(enable);
    m_analyzeButton->setEnabled(enable);
    m_excludeArtifactsCheckbox->setEnabled(enable);
}
