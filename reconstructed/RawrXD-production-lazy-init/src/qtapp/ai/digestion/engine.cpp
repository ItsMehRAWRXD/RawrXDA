#include "ai_digestion_engine.hpp"
#include <QtCore/QDebug>
#include <QtCore/QTextStream>
#include <QtCore/QStringConverter>
#include <QtCore/QRegularExpression>
#include <QtCore/QCryptographicHash>
#include <QtCore/QStandardPaths>
#include <QtCore/QProcess>
#include <QtCore/QUuid>
#include <QtCore/QDirIterator>
#include <QtCore/QElapsedTimer>
#include <algorithm>
#include <random>

// KnowledgeRepresentation implementation
QJsonObject KnowledgeRepresentation::toJson() const {
    QJsonObject obj;
    obj["id"] = id;
    obj["content"] = content;
    obj["originalFile"] = originalFile;
    obj["fileType"] = static_cast<int>(fileType);
    obj["metadata"] = metadata;
    
    QJsonArray tokensArray;
    for (const auto& token : tokens) {
        tokensArray.append(token);
    }
    obj["tokens"] = tokensArray;
    
    QJsonArray keywordsArray;
    for (const auto& keyword : keywords) {
        keywordsArray.append(keyword);
    }
    obj["keywords"] = keywordsArray;
    
    QJsonArray functionsArray;
    for (const auto& func : functions) {
        functionsArray.append(func);
    }
    obj["functions"] = functionsArray;
    
    QJsonArray classesArray;
    for (const auto& cls : classes) {
        classesArray.append(cls);
    }
    obj["classes"] = classesArray;
    
    QJsonArray variablesArray;
    for (const auto& var : variables) {
        variablesArray.append(var);
    }
    obj["variables"] = variablesArray;
    
    QJsonArray commentsArray;
    for (const auto& comment : comments) {
        commentsArray.append(comment);
    }
    obj["comments"] = commentsArray;
    
    QJsonObject weightsObj;
    for (auto it = semanticWeights.begin(); it != semanticWeights.end(); ++it) {
        weightsObj[it.key()] = it.value();
    }
    obj["semanticWeights"] = weightsObj;
    
    obj["timestamp"] = timestamp.toString(Qt::ISODate);
    obj["contextWindow"] = contextWindow;
    
    return obj;
}

KnowledgeRepresentation KnowledgeRepresentation::fromJson(const QJsonObject& obj) {
    KnowledgeRepresentation kr;
    kr.id = obj["id"].toString();
    kr.content = obj["content"].toString();
    kr.originalFile = obj["originalFile"].toString();
    kr.fileType = static_cast<FileType>(obj["fileType"].toInt());
    kr.metadata = obj["metadata"].toObject();
    
    const auto tokensArray = obj["tokens"].toArray();
    for (const auto& token : tokensArray) {
        kr.tokens.append(token.toString());
    }
    
    const auto keywordsArray = obj["keywords"].toArray();
    for (const auto& keyword : keywordsArray) {
        kr.keywords.append(keyword.toString());
    }
    
    const auto functionsArray = obj["functions"].toArray();
    for (const auto& func : functionsArray) {
        kr.functions.append(func.toString());
    }
    
    const auto classesArray = obj["classes"].toArray();
    for (const auto& cls : classesArray) {
        kr.classes.append(cls.toString());
    }
    
    const auto variablesArray = obj["variables"].toArray();
    for (const auto& var : variablesArray) {
        kr.variables.append(var.toString());
    }
    
    const auto commentsArray = obj["comments"].toArray();
    for (const auto& comment : commentsArray) {
        kr.comments.append(comment.toString());
    }
    
    const auto weightsObj = obj["semanticWeights"].toObject();
    for (auto it = weightsObj.begin(); it != weightsObj.end(); ++it) {
        kr.semanticWeights[it.key()] = it.value().toDouble();
    }
    
    kr.timestamp = QDateTime::fromString(obj["timestamp"].toString(), Qt::ISODate);
    kr.contextWindow = obj["contextWindow"].toInt();
    
    return kr;
}

// TrainingDataset implementation
QJsonObject TrainingDataset::toJson() const {
    QJsonObject obj;
    obj["name"] = name;
    obj["description"] = description;
    obj["created"] = created.toString(Qt::ISODate);
    obj["totalTokens"] = totalTokens;
    obj["totalSamples"] = totalSamples;
    obj["statistics"] = statistics;
    
    QJsonArray samplesArray;
    for (const auto& sample : samples) {
        samplesArray.append(sample.toJson());
    }
    obj["samples"] = samplesArray;
    
    QJsonObject typeCountsObj;
    for (auto it = fileTypeCounts.begin(); it != fileTypeCounts.end(); ++it) {
        typeCountsObj[QString::number(static_cast<int>(it.key()))] = it.value();
    }
    obj["fileTypeCounts"] = typeCountsObj;
    
    QJsonArray domainsArray;
    for (const auto& domain : domains) {
        domainsArray.append(domain);
    }
    obj["domains"] = domainsArray;
    
    return obj;
}

TrainingDataset TrainingDataset::fromJson(const QJsonObject& obj) {
    TrainingDataset td;
    td.name = obj["name"].toString();
    td.description = obj["description"].toString();
    td.created = QDateTime::fromString(obj["created"].toString(), Qt::ISODate);
    td.totalTokens = obj["totalTokens"].toInt();
    td.totalSamples = obj["totalSamples"].toInt();
    td.statistics = obj["statistics"].toObject();
    
    const auto samplesArray = obj["samples"].toArray();
    for (const auto& sample : samplesArray) {
        td.samples.append(KnowledgeRepresentation::fromJson(sample.toObject()));
    }
    
    const auto typeCountsObj = obj["fileTypeCounts"].toObject();
    for (auto it = typeCountsObj.begin(); it != typeCountsObj.end(); ++it) {
        td.fileTypeCounts[static_cast<FileType>(it.key().toInt())] = it.value().toInt();
    }
    
    const auto domainsArray = obj["domains"].toArray();
    for (const auto& domain : domainsArray) {
        td.domains.append(domain.toString());
    }
    
    return td;
}

// AIDigestionEngine implementation
AIDigestionEngine::AIDigestionEngine(QObject* parent)
    : QObject(parent)
    , m_isDigesting(false)
    , m_isTraining(false)
    , m_isPaused(false)
    , m_shouldStop(false)
    , m_progress(0.0)
    , m_processedFiles(0)
    , m_totalFiles(0)
    , m_progressTimer(nullptr)
{
    initializeEngine();
}

AIDigestionEngine::~AIDigestionEngine() {
    stopDigestion();
    stopTraining();
    
    if (m_progressTimer) {
        m_progressTimer->stop();
        delete m_progressTimer;
    }
}

void AIDigestionEngine::initializeEngine() {
    // Initialize progress timer
    m_progressTimer = new QTimer(this);
    m_progressTimer->setInterval(100); // Update every 100ms
    connect(m_progressTimer, &QTimer::timeout, this, [this]() {
        emit progressChanged(m_progress);
        emit statusChanged(m_statusMessage);
    });
    
    // Initialize dataset
    m_dataset.name = "Custom AI Dataset";
    m_dataset.created = QDateTime::currentDateTime();
    m_dataset.totalTokens = 0;
    m_dataset.totalSamples = 0;
    m_dataset.description = "AI model trained from digested content";
    
    qDebug() << "AIDigestionEngine initialized";
}

void AIDigestionEngine::setConfig(const DigestionConfig& config) {
    QMutexLocker locker(&m_mutex);
    m_config = config;
}

DigestionConfig AIDigestionEngine::getConfig() const {
    QMutexLocker locker(&m_mutex);
    return m_config;
}

void AIDigestionEngine::startDigestion(const QStringList& inputPaths) {
    if (m_isDigesting) {
        qWarning() << "Digestion already in progress";
        return;
    }

    qInfo() << "Starting AIDigestionEngine::startDigestion with" << inputPaths.size() << "input paths";
    
    m_isDigesting = true;
    m_shouldStop = false;
    m_isPaused = false;
    m_progress = 0.0;
    m_processedFiles = 0;
    m_startTime = QDateTime::currentDateTime();
    
    // Collect all files to process
    m_filesToProcess.clear();
    m_totalFiles = 0;
    
    for (const QString& path : inputPaths) {
        QFileInfo info(path);
        if (info.isFile()) {
            if (shouldProcessFile(path)) {
                m_filesToProcess.append(path);
            }
        } else if (info.isDir()) {
            processDirectory(path);
        }
    }
    
    m_totalFiles = m_filesToProcess.size();
    qInfo() << "Found" << m_totalFiles << "valid files for digestion";
    m_statusMessage = QString("Starting digestion of %1 files...").arg(m_totalFiles);
    
    // Start processing in background thread
    m_digestionThread = std::make_unique<QThread>();
    auto* worker = new DigestionWorker(this);
    worker->moveToThread(m_digestionThread.get());
    
    connect(m_digestionThread.get(), &QThread::started, worker, [worker, this]() {
        worker->processFiles(m_filesToProcess, m_config);
    });
    
    connect(worker, &DigestionWorker::fileProcessed, this, &AIDigestionEngine::processFileInternal);
    connect(worker, &DigestionWorker::finished, this, &AIDigestionEngine::onDigestionThreadFinished);
    connect(worker, &DigestionWorker::error, this, [this](const QString& error) {
        qCritical() << "Digestion failed:" << error;
        emit digestionFailed(error);
        m_isDigesting = false;
    });
    
    connect(m_digestionThread.get(), &QThread::finished, worker, &QObject::deleteLater);
    
    m_digestionThread->start();
    m_progressTimer->start();
    
    emit statusChanged(m_statusMessage);
}

void AIDigestionEngine::stopDigestion() {
    if (!m_isDigesting) return;
    
    m_shouldStop = true;
    m_isDigesting = false;
    
    if (m_digestionThread && m_digestionThread->isRunning()) {
        m_digestionThread->quit();
        if (!m_digestionThread->wait(5000)) {
            m_digestionThread->terminate();
            m_digestionThread->wait(2000);
        }
    }
    
    if (m_progressTimer) {
        m_progressTimer->stop();
    }
    
    m_statusMessage = "Digestion stopped";
    emit statusChanged(m_statusMessage);
}

bool AIDigestionEngine::isDigesting() const {
    return m_isDigesting;
}

double AIDigestionEngine::getProgress() const {
    return m_progress;
}

QString AIDigestionEngine::getStatusMessage() const {
    return m_statusMessage;
}

int AIDigestionEngine::getProcessedFiles() const {
    return m_processedFiles;
}

int AIDigestionEngine::getTotalFiles() const {
    return m_totalFiles;
}

TrainingDataset AIDigestionEngine::getTrainingDataset() const {
    QMutexLocker locker(&m_mutex);
    return m_dataset;
}

bool AIDigestionEngine::saveDataset(const QString& filePath) const {
    QMutexLocker locker(&m_mutex);
    
    QJsonDocument doc(m_dataset.toJson());
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Cannot open file for writing:" << filePath;
        return false;
    }
    
    file.write(doc.toJson());
    return true;
}

bool AIDigestionEngine::loadDataset(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open file for reading:" << filePath;
        return false;
    }
    
    QByteArray data = file.readAll();
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "JSON parse error:" << error.errorString();
        return false;
    }
    
    QMutexLocker locker(&m_mutex);
    m_dataset = TrainingDataset::fromJson(doc.object());
    
    return true;
}

FileType AIDigestionEngine::detectFileType(const QString& filePath) {
    QFileInfo info(filePath);
    QString extension = info.suffix().toLower();
    
    if (extension == "cpp" || extension == "cxx" || extension == "cc" || extension == "c" || extension == "hpp" || extension == "h") {
        return FileType::CPlusPlus;
    } else if (extension == "py" || extension == "pyx" || extension == "pyi") {
        return FileType::Python;
    } else if (extension == "js" || extension == "jsx" || extension == "ts" || extension == "tsx") {
        return FileType::JavaScript;
    } else if (extension == "asm" || extension == "s" || extension == "inc") {
        return FileType::Assembly;
    } else if (extension == "md" || extension == "markdown") {
        return FileType::Markdown;
    } else if (extension == "html" || extension == "htm") {
        return FileType::HTML;
    } else if (extension == "xml") {
        return FileType::XML;
    } else if (extension == "json") {
        return FileType::JSON;
    } else if (extension == "txt" || extension == "text") {
        return FileType::PlainText;
    } else if (extension == "exe" || extension == "dll" || extension == "so" || extension == "dylib") {
        return FileType::Binary;
    } else if (extension == "rst" || extension == "doc" || extension == "docx" || extension == "pdf") {
        return FileType::Documentation;
    } else if (QStringList({"cpp", "c", "h", "hpp", "py", "js", "java", "cs", "php", "rb", "go", "rs"}).contains(extension)) {
        return FileType::SourceCode;
    }
    
    return FileType::Unknown;
}

QString AIDigestionEngine::extractFileContent(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Cannot read file:" << filePath;
        return QString();
    }
    
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    return stream.readAll();
}

void AIDigestionEngine::startTraining() {
    if (m_isTraining) {
        qWarning() << "Training already in progress";
        return;
    }
    
    qInfo() << "Starting training with" << m_dataset.totalSamples << "samples and" << m_dataset.totalTokens << "tokens";

    if (m_dataset.totalSamples == 0) {
        emit trainingFailed("No training data available. Please digest content first.");
        return;
    }
    
    m_isTraining = true;
    m_statusMessage = "Preparing training environment...";
    emit statusChanged(m_statusMessage);
    
    // Prepare training data
    prepareTrainingData();
    
    // Start training in background thread
    m_trainingThread = std::make_unique<QThread>();
    auto* worker = new TrainingWorker(this);
    worker->moveToThread(m_trainingThread.get());
    
    connect(m_trainingThread.get(), &QThread::started, worker, [worker, this]() {
        worker->startTraining(m_dataset, m_config);
    });
    
    connect(worker, &TrainingWorker::trainingProgress, this, [this](double progress) {
        m_statusMessage = QString("Training progress: %1%").arg(QString::number(progress * 100, 'f', 1));
        emit statusChanged(m_statusMessage);
    });
    
    connect(worker, &TrainingWorker::finished, this, &AIDigestionEngine::onTrainingThreadFinished);
    connect(worker, &TrainingWorker::error, this, [this](const QString& error) {
        qCritical() << "Training failed:" << error;
        emit trainingFailed(error);
        m_isTraining = false;
    });
    
    connect(m_trainingThread.get(), &QThread::finished, worker, &QObject::deleteLater);
    
    m_trainingThread->start();
}

void AIDigestionEngine::stopTraining() {
    if (!m_isTraining) return;
    
    m_isTraining = false;
    
    if (m_trainingThread && m_trainingThread->isRunning()) {
        m_trainingThread->quit();
        if (!m_trainingThread->wait(10000)) {
            m_trainingThread->terminate();
            m_trainingThread->wait(5000);
        }
    }
    
    m_statusMessage = "Training stopped";
    emit statusChanged(m_statusMessage);
}

bool AIDigestionEngine::isTraining() const {
    return m_isTraining;
}

void AIDigestionEngine::pauseDigestion() {
    if (m_isDigesting && !m_isPaused) {
        m_isPaused = true;
        m_statusMessage = "Digestion paused";
        emit statusChanged(m_statusMessage);
    }
}

void AIDigestionEngine::resumeDigestion() {
    if (m_isDigesting && m_isPaused) {
        m_isPaused = false;
        m_pauseCondition.wakeAll();
        m_statusMessage = "Digestion resumed";
        emit statusChanged(m_statusMessage);
    }
}

void AIDigestionEngine::clearDataset() {
    QMutexLocker locker(&m_mutex);
    m_dataset.samples.clear();
    m_dataset.totalTokens = 0;
    m_dataset.totalSamples = 0;
    m_dataset.fileTypeCounts.clear();
    m_dataset.domains.clear();
    m_knowledgeBase.clear();
}

void AIDigestionEngine::processFileInternal(const QString& filePath) {
    if (m_shouldStop) return;
    
    // Wait if paused
    if (m_isPaused) {
        QMutexLocker locker(&m_mutex);
        m_pauseCondition.wait(&m_mutex);
    }
    
    processFile(filePath);
    
    QMutexLocker locker(&m_mutex);
    m_processedFiles++;
    m_progress = static_cast<double>(m_processedFiles) / m_totalFiles;
    
    emit fileProcessed(filePath, m_processedFiles, m_totalFiles);
}

void AIDigestionEngine::onDigestionThreadFinished() {
    m_isDigesting = false;
    if (m_progressTimer) {
        m_progressTimer->stop();
    }
    
    // Update dataset statistics
    m_dataset.totalSamples = m_knowledgeBase.size();
    m_dataset.totalTokens = 0;
    for (const auto& kb : m_knowledgeBase) {
        m_dataset.totalTokens += kb.tokens.size();
    }
    
    m_dataset.statistics = generateStatistics();
    m_statusMessage = QString("Digestion completed. Processed %1 files, extracted %2 knowledge samples.")
                      .arg(m_processedFiles).arg(m_dataset.totalSamples);
    
    emit digestionCompleted(m_dataset);
    emit statusChanged(m_statusMessage);
}

void AIDigestionEngine::onTrainingThreadFinished() {
    m_isTraining = false;
    
    QString modelPath = QDir(m_config.outputDirectory).filePath(m_config.modelName + ".gguf");
    m_statusMessage = QString("Training completed. Model saved to: %1").arg(modelPath);
    
    emit trainingCompleted(modelPath);
    emit statusChanged(m_statusMessage);
}

void AIDigestionEngine::processDirectory(const QString& dirPath) {
    QDirIterator iterator(dirPath, QDir::Files, QDirIterator::Subdirectories);
    
    while (iterator.hasNext()) {
        QString filePath = iterator.next();
        if (shouldProcessFile(filePath)) {
            m_filesToProcess.append(filePath);
        }
    }
}

void AIDigestionEngine::processFile(const QString& filePath) {
    QElapsedTimer timer;
    timer.start();
    
    try {
        QString content = extractFileContent(filePath);
        if (content.isEmpty() || content.length() < m_config.minContentLength) {
            return;
        }
        
        KnowledgeRepresentation knowledge = analyzeContent(content, filePath);
        if (!knowledge.tokens.isEmpty()) {
            QMutexLocker locker(&m_mutex);
            m_knowledgeBase.append(knowledge);
            m_dataset.samples.append(knowledge);
            
            emit knowledgeExtracted(knowledge);
        }
        
        qInfo() << "Processed" << filePath << "in" << timer.elapsed() << "ms";
        
    } catch (const std::exception& e) {
        qWarning() << "Error processing file" << filePath << ":" << e.what();
    }
}

KnowledgeRepresentation AIDigestionEngine::analyzeContent(const QString& content, const QString& filePath) {
    FileType fileType = detectFileType(filePath);
    
    KnowledgeRepresentation knowledge;
    knowledge.id = generateUniqueId();
    knowledge.originalFile = filePath;
    knowledge.fileType = fileType;
    knowledge.timestamp = QDateTime::currentDateTime();
    
    // Preprocess content
    QString processedContent = preprocessContent(content, fileType);
    knowledge.content = processedContent;
    
    // Extract different types of information based on file type
    switch (fileType) {
        case FileType::Assembly:
            knowledge = extractFromAssembly(processedContent, filePath);
            break;
        case FileType::CPlusPlus:
            knowledge = extractFromCPlusPlus(processedContent, filePath);
            break;
        case FileType::Python:
            knowledge = extractFromPython(processedContent, filePath);
            break;
        case FileType::SourceCode:
            knowledge = extractFromSourceCode(processedContent, filePath);
            break;
        case FileType::Documentation:
        case FileType::Markdown:
            knowledge = extractFromDocumentation(processedContent, filePath);
            break;
        default:
            // Generic extraction
            knowledge.tokens = tokenizeContent(processedContent, fileType);
            knowledge.keywords = extractKeywords(processedContent, fileType);
            if (m_config.extractComments) {
                knowledge.comments = extractComments(processedContent, fileType);
            }
            break;
    }
    
    // Calculate semantic weights
    knowledge.semanticWeights = calculateSemanticWeights(knowledge.tokens);
    knowledge.contextWindow = qMin(knowledge.tokens.size(), m_config.maxTokens);
    
    // Set metadata
    knowledge.metadata["fileSize"] = QFileInfo(filePath).size();
    knowledge.metadata["extractionMode"] = static_cast<int>(m_config.mode);
    knowledge.metadata["timestamp"] = knowledge.timestamp.toString(Qt::ISODate);
    
    return knowledge;
}

// Content analysis methods implementation
QStringList AIDigestionEngine::tokenizeContent(const QString& content, FileType type) {
    QString cleaned = preprocessContent(content, type);
    QStringList tokens;
    
    // Simple tokenization based on whitespace and punctuation
    QRegularExpression tokenRegex(R"(\b\w+\b)");
    auto matches = tokenRegex.globalMatch(cleaned);
    
    while (matches.hasNext()) {
        auto match = matches.next();
        QString token = match.captured(0).toLower();
        if (token.length() > 2) {  // Filter out very short tokens
            tokens << token;
        }
    }
    
    // Remove duplicates while preserving order
    QStringList uniqueTokens;
    QSet<QString> seen;
    for (const QString& token : tokens) {
        if (!seen.contains(token)) {
            seen.insert(token);
            uniqueTokens << token;
        }
    }
    
    return uniqueTokens;
}

QStringList AIDigestionEngine::extractKeywords(const QString& content, FileType type) {
    QStringList keywords;
    
    switch (type) {
        case FileType::CPlusPlus: {
            QRegularExpression cppRegex("\\b(class|struct|namespace|template|virtual|override|const|static|public|private|protected)\\b");
            auto matches = cppRegex.globalMatch(content);
            while (matches.hasNext()) {
                auto match = matches.next();
                keywords << match.captured(0);
            }
            break;
        }
            
        case FileType::Python: {
            QRegularExpression pyRegex("\\b(def|class|import|from|if|elif|else|for|while|try|except|finally|with|async|await)\\b");
            auto matches = pyRegex.globalMatch(content);
            while (matches.hasNext()) {
                auto match = matches.next();
                keywords << match.captured(0);
            }
            break;
        }
            
        case FileType::Assembly: {
            QRegularExpression asmRegex("\\b(mov|add|sub|mul|div|cmp|jmp|je|jne|call|ret|push|pop|lea)\\b", QRegularExpression::CaseInsensitiveOption);
            auto matches = asmRegex.globalMatch(content);
            while (matches.hasNext()) {
                auto match = matches.next();
                keywords << match.captured(0);
            }
            break;
        }
            
        case FileType::JavaScript: {
            QRegularExpression jsRegex("\\b(function|const|let|var|if|else|for|while|class|extends|async|await|Promise)\\b");
            auto matches = jsRegex.globalMatch(content);
            while (matches.hasNext()) {
                auto match = matches.next();
                keywords << match.captured(0);
            }
            break;
        }
            
        default: {
            // Generic keyword extraction
            QRegularExpression genericRegex("\\b[A-Z][a-zA-Z0-9_]*\\b");
            auto matches = genericRegex.globalMatch(content);
            while (matches.hasNext()) {
                auto match = matches.next();
                keywords << match.captured(0);
            }
            break;
        }
    }
    
    keywords.removeDuplicates();
    return keywords;
}

QStringList AIDigestionEngine::extractComments(const QString& content, FileType type) {
    QStringList comments;
    
    if (type == FileType::CPlusPlus || type == FileType::JavaScript) {
        // Single line comments
        QRegularExpression singleLineRegex("//\\s*(.*)");
        auto singleMatches = singleLineRegex.globalMatch(content);
        while (singleMatches.hasNext()) {
            auto match = singleMatches.next();
            comments << match.captured(1);
        }
        
        // Multi-line comments
        QRegularExpression multiLineRegex("/\\*\\s*(.*?)\\s*\\*/", QRegularExpression::DotMatchesEverythingOption);
        auto multiMatches = multiLineRegex.globalMatch(content);
        while (multiMatches.hasNext()) {
            auto match = multiMatches.next();
            comments << match.captured(1);
        }
    } else if (type == FileType::Python) {
        // Single line comments
        QRegularExpression pythonRegex("#\\s*(.*)");
        auto pyMatches = pythonRegex.globalMatch(content);
        while (pyMatches.hasNext()) {
            auto match = pyMatches.next();
            comments << match.captured(1);
        }
        
        // Triple-quoted strings (docstrings)
        QRegularExpression docstringRegex("\\\"\\\"\\\"(.*?)\\\"\\\"\\\"", QRegularExpression::DotMatchesEverythingOption);
        auto docMatches = docstringRegex.globalMatch(content);
        while (docMatches.hasNext()) {
            auto match = docMatches.next();
            comments << match.captured(1);
        }
    } else if (type == FileType::Assembly) {
        // Semicolon comments
        QRegularExpression asmRegex(";\\s*(.*)");
        auto asmMatches = asmRegex.globalMatch(content);
        while (asmMatches.hasNext()) {
            auto match = asmMatches.next();
            comments << match.captured(1);
        }
    } else {
        // Try to extract any comment-like patterns
        QRegularExpression genericRegex("[#;]\\s*(.*)");
        auto genericMatches = genericRegex.globalMatch(content);
        while (genericMatches.hasNext()) {
            auto match = genericMatches.next();
            comments << match.captured(1);
        }
    }
    
    // Clean up comments
    QStringList cleanedComments;
    for (QString comment : comments) {
        comment = comment.trimmed();
        if (comment.length() > 5) {  // Filter out very short comments
            cleanedComments << comment;
        }
    }
    
    return cleanedComments;
}

QHash<QString, double> AIDigestionEngine::calculateSemanticWeights(const QStringList& tokens) {
    QHash<QString, double> weights;
    QHash<QString, int> frequencies;
    
    // Calculate token frequencies
    for (const QString& token : tokens) {
        frequencies[token]++;
    }
    
    // Calculate TF-IDF-like weights
    int totalTokens = tokens.size();
    int uniqueTokens = frequencies.size();
    
    for (auto it = frequencies.begin(); it != frequencies.end(); ++it) {
        const QString& token = it.key();
        int freq = it.value();
        
        // Term frequency
        double tf = static_cast<double>(freq) / totalTokens;
        
        // Inverse document frequency (simplified)
        double idf = qLn(static_cast<double>(uniqueTokens) / (1.0 + freq));
        
        // Apply domain-specific boosts
        double boost = 1.0;
        if (token.contains(QRegularExpression(R"(\b(function|class|method|variable|parameter)\b)"))) {
            boost = 1.5;  // Boost programming concepts
        }
        if (token.length() > 8) {
            boost *= 1.2;  // Boost longer, more specific terms
        }
        
        weights[token] = tf * idf * boost;
    }
    
    return weights;
}

QString AIDigestionEngine::preprocessContent(const QString& content, FileType type) {
    QString processed = content;
    
    // Remove excessive whitespace
    processed = processed.simplified();
    
    // Remove string literals and character constants for better analysis
    switch (type) {
        case FileType::CPlusPlus:
        case FileType::C:
        case FileType::JavaScript:
            processed = processed.remove(QRegularExpression(R"("(?:[^"\\]|\\.)*")"));  // String literals
            processed = processed.remove(QRegularExpression(R"('(?:[^'\\]|\\.)*')"));  // Character constants
            break;
            
        case FileType::Python:
            processed = processed.remove(QRegularExpression(R"(""".*?""")"));  // Triple-quoted strings
            processed = processed.remove(QRegularExpression(R"("(?:[^"\\]|\\.)*")"));  // String literals
            processed = processed.remove(QRegularExpression(R"('(?:[^'\\]|\\.)*')"));  // Character constants
            break;
            
        case FileType::Assembly:
            // Remove string constants in assembly
            processed = processed.remove(QRegularExpression(R"(db\s+['""][^'"]*['""])"));
            break;
            
        default:
            break;
    }
    
    // Normalize line endings
    processed = processed.replace("\r\n", "\n").replace("\r", "\n");
    
    return processed;
}

QString AIDigestionEngine::generateUniqueId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

bool AIDigestionEngine::shouldProcessFile(const QString& filePath) {
    QFileInfo info(filePath);
    
    // Skip if file is too large (> 10MB)
    if (info.size() > 10 * 1024 * 1024) {
        return false;
    }
    
    // Skip binary files
    QString suffix = info.suffix().toLower();
    QStringList binaryExtensions = {"exe", "dll", "so", "dylib", "obj", "o", "a", "lib", "bin", "zip", "tar", "gz"};
    if (binaryExtensions.contains(suffix)) {
        return false;
    }
    
    // Skip hidden files and directories
    if (info.fileName().startsWith('.')) {
        return false;
    }
    
    // Skip common non-code directories
    QString path = info.absoluteFilePath();
    QStringList skipDirs = {"node_modules", ".git", ".svn", "build", "Debug", "Release", "__pycache__"};
    for (const QString& skipDir : skipDirs) {
        if (path.contains(QDir::separator() + skipDir + QDir::separator())) {
            return false;
        }
    }
    
    return true;
}

QJsonObject AIDigestionEngine::generateStatistics() {
    QJsonObject stats;
    
    stats["total_files"] = m_totalFiles;
    stats["processed_files"] = m_processedFiles;
    stats["knowledge_entries"] = m_knowledgeBase.size();
    stats["vocabulary_size"] = m_vocabulary.size();
    stats["processing_time_ms"] = m_startTime.msecsTo(QDateTime::currentDateTime());
    
    // File type statistics
    QJsonObject fileTypes;
    for (auto it = m_fileTypeStats.begin(); it != m_fileTypeStats.end(); ++it) {
        QString typeName;
        switch (it.key()) {
            case FileType::CPlusPlus: typeName = "cpp"; break;
            case FileType::Python: typeName = "python"; break;
            case FileType::Assembly: typeName = "assembly"; break;
            case FileType::JavaScript: typeName = "javascript"; break;
            case FileType::Documentation: typeName = "documentation"; break;
            default: typeName = "other"; break;
        }
        fileTypes[typeName] = it.value();
    }
    stats["file_types"] = fileTypes;
    
    // Domain statistics
    QJsonObject domains;
    for (auto it = m_domainStats.begin(); it != m_domainStats.end(); ++it) {
        domains[it.key()] = it.value();
    }
    stats["domains"] = domains;
    
    return stats;
}

void AIDigestionEngine::prepareTrainingData() {
    m_dataset = TrainingDataset();
    m_dataset.samples.clear();
    
    for (const KnowledgeRepresentation& knowledge : m_knowledgeBase) {
        // Generate instruction-response pairs from knowledge
        QJsonArray prompts = generateTrainingPrompts(knowledge);
        
        for (const QJsonValue& promptValue : prompts) {
            QJsonObject prompt = promptValue.toObject();
            
            KnowledgeRepresentation sample;
            sample.id = generateUniqueId();
            sample.content = prompt["instruction"].toString() + " " + 
                           prompt["input"].toString() + " " + 
                           prompt["output"].toString();
            sample.originalFile = knowledge.originalFile;
            sample.metadata = knowledge.metadata;
            sample.fileType = knowledge.fileType;
            sample.timestamp = QDateTime::currentDateTime();
            
            m_dataset.samples.append(sample);
        }
    }
    
    // Update dataset metadata
    m_dataset.totalSamples = m_dataset.samples.size();
    m_dataset.totalTokens = 0;
    
    for (const KnowledgeRepresentation& sample : m_dataset.samples) {
        m_dataset.totalTokens += sample.content.split(' ', Qt::SkipEmptyParts).size();
    }
    
    qInfo() << "Prepared training dataset with" << m_dataset.totalSamples << "samples and" << m_dataset.totalTokens << "tokens";
}

// Specialized content extractors
KnowledgeRepresentation AIDigestionEngine::extractFromSourceCode(const QString& content, const QString& filePath) {
    KnowledgeRepresentation knowledge;
    knowledge.id = generateUniqueId();
    knowledge.originalFile = filePath;
    knowledge.timestamp = QDateTime::currentDateTime();
    
    QFileInfo info(filePath);
    FileType fileType = detectFileType(filePath);
    knowledge.fileType = fileType;
    
    // Extract functions/methods
    QStringList functions;
    switch (fileType) {
        case FileType::CPlusPlus:
        case FileType::C: {
            QRegularExpression funcRegex(R"(\b\w+\s+(\w+)\s*\([^)]*\)\s*\{)", QRegularExpression::MultilineOption);
            auto matches = funcRegex.globalMatch(content);
            while (matches.hasNext()) {
                auto match = matches.next();
                functions << match.captured(1);
            }
            break;
        }
        case FileType::Python: {
            QRegularExpression pyRegex(R"(def\s+(\w+)\s*\()", QRegularExpression::MultilineOption);
            auto matches = pyRegex.globalMatch(content);
            while (matches.hasNext()) {
                auto match = matches.next();
                functions << match.captured(1);
            }
            break;
        }
        case FileType::JavaScript: {
            QRegularExpression jsRegex(R"(function\s+(\w+)\s*\()", QRegularExpression::MultilineOption);
            auto matches = jsRegex.globalMatch(content);
            while (matches.hasNext()) {
                auto match = matches.next();
                functions << match.captured(1);
            }
            break;
        }
        default:
            break;
    }
    
    // Extract classes/structures
    QStringList classes;
    if (fileType == FileType::CPlusPlus || fileType == FileType::C) {
        QRegularExpression classRegex(R"(\b(?:class|struct)\s+(\w+))", QRegularExpression::MultilineOption);
        auto matches = classRegex.globalMatch(content);
        while (matches.hasNext()) {
            auto match = matches.next();
            classes << match.captured(1);
        }
    } else if (fileType == FileType::Python) {
        QRegularExpression pyClassRegex(R"(class\s+(\w+)\s*[:\(])", QRegularExpression::MultilineOption);
        auto matches = pyClassRegex.globalMatch(content);
        while (matches.hasNext()) {
            auto match = matches.next();
            classes << match.captured(1);
        }
    }
    
    knowledge.content = QString("File: %1\nFunctions: %2\nClasses: %3")
                            .arg(info.fileName())
                            .arg(functions.join(", "))
                            .arg(classes.join(", "));
    knowledge.tokens = tokenizeContent(content, fileType);
    knowledge.keywords = extractKeywords(content, fileType);
    knowledge.semanticWeights = calculateSemanticWeights(knowledge.tokens);
    
    knowledge.metadata["file_type"] = static_cast<int>(fileType);
    knowledge.metadata["function_count"] = functions.size();
    knowledge.metadata["class_count"] = classes.size();
    knowledge.metadata["line_count"] = content.count('\n') + 1;
    
    return knowledge;
}

KnowledgeRepresentation AIDigestionEngine::extractFromDocumentation(const QString& content, const QString& filePath) {
    KnowledgeRepresentation knowledge;
    knowledge.id = generateUniqueId();
    knowledge.originalFile = filePath;
    knowledge.fileType = FileType::Documentation;
    knowledge.timestamp = QDateTime::currentDateTime();
    
    // Extract headers/sections
    QStringList headers;
    QRegularExpression headerRegex("^#+\\s+(.+)$", QRegularExpression::MultilineOption);
    auto headerMatches = headerRegex.globalMatch(content);
    while (headerMatches.hasNext()) {
        auto match = headerMatches.next();
        headers << match.captured(1);
    }
    
    // Extract code blocks
    QStringList codeBlocks;
    QRegularExpression codeRegex("```[\\w]*\\n(.*?)\\n```", 
                                QRegularExpression::DotMatchesEverythingOption | QRegularExpression::MultilineOption);
    auto codeMatches = codeRegex.globalMatch(content);
    while (codeMatches.hasNext()) {
        auto match = codeMatches.next();
        codeBlocks << match.captured(1);
    }
    
    knowledge.content = QString("Documentation: %1\nSections: %2\nCode examples: %3")
                            .arg(QFileInfo(filePath).baseName())
                            .arg(headers.join(", "))
                            .arg(QString::number(codeBlocks.size()));
    
    knowledge.tokens = tokenizeContent(content, FileType::Documentation);
    knowledge.keywords = extractKeywords(content, FileType::Documentation);
    knowledge.semanticWeights = calculateSemanticWeights(knowledge.tokens);
    
    knowledge.metadata["header_count"] = headers.size();
    knowledge.metadata["code_block_count"] = codeBlocks.size();
    knowledge.metadata["word_count"] = content.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts).size();
    
    return knowledge;
}

KnowledgeRepresentation AIDigestionEngine::extractFromAssembly(const QString& content, const QString& filePath) {
    KnowledgeRepresentation knowledge;
    knowledge.id = generateUniqueId();
    knowledge.originalFile = filePath;
    knowledge.fileType = FileType::Assembly;
    knowledge.timestamp = QDateTime::currentDateTime();
    
    // Extract labels/functions
    QStringList labels;
    QRegularExpression labelRegex(R"(^(\w+):)", QRegularExpression::MultilineOption);
    auto labelMatches = labelRegex.globalMatch(content);
    while (labelMatches.hasNext()) {
        auto match = labelMatches.next();
        labels << match.captured(1);
    }
    
    // Extract instructions
    QStringList instructions;
    QRegularExpression instrRegex(R"(\b(mov|add|sub|mul|div|cmp|jmp|je|jne|jz|jnz|call|ret|push|pop|lea|xor|and|or|not|shl|shr)\b)", 
                                QRegularExpression::CaseInsensitiveOption);
    auto instrMatches = instrRegex.globalMatch(content);
    while (instrMatches.hasNext()) {
        auto match = instrMatches.next();
        instructions << match.captured(1);
    }
    
    // Count instruction frequencies
    QHash<QString, int> instrFreq;
    for (const QString& instr : instructions) {
        instrFreq[instr.toLower()]++;
    }
    
    knowledge.content = QString("Assembly file: %1\nLabels: %2\nUnique instructions: %3")
                            .arg(QFileInfo(filePath).baseName())
                            .arg(labels.join(", "))
                            .arg(instrFreq.keys().join(", "));
    
    knowledge.tokens = tokenizeContent(content, FileType::Assembly);
    knowledge.keywords = extractKeywords(content, FileType::Assembly);
    knowledge.semanticWeights = calculateSemanticWeights(knowledge.tokens);
    
    knowledge.metadata["label_count"] = labels.size();
    knowledge.metadata["instruction_count"] = instructions.size();
    knowledge.metadata["unique_instructions"] = instrFreq.size();
    
    return knowledge;
}

KnowledgeRepresentation AIDigestionEngine::extractFromCPlusPlus(const QString& content, const QString& filePath) {
    KnowledgeRepresentation knowledge;
    knowledge.id = generateUniqueId();
    knowledge.originalFile = filePath;
    knowledge.fileType = FileType::CPlusPlus;
    knowledge.timestamp = QDateTime::currentDateTime();
    
    // Extract includes
    QStringList includes;
    QRegularExpression includeRegex(R"(#include\s*[<"]([^>"]+)[>"])", QRegularExpression::MultilineOption);
    auto includeMatches = includeRegex.globalMatch(content);
    while (includeMatches.hasNext()) {
        auto match = includeMatches.next();
        includes << match.captured(1);
    }
    
    // Extract namespaces
    QStringList namespaces;
    QRegularExpression nsRegex(R"(namespace\s+(\w+))", QRegularExpression::MultilineOption);
    auto nsMatches = nsRegex.globalMatch(content);
    while (nsMatches.hasNext()) {
        auto match = nsMatches.next();
        namespaces << match.captured(1);
    }
    
    // Extract templates
    QStringList templates;
    QRegularExpression templateRegex(R"(template\s*<[^>]+>\s*(?:class|struct)\s+(\w+))", QRegularExpression::MultilineOption);
    auto templateMatches = templateRegex.globalMatch(content);
    while (templateMatches.hasNext()) {
        auto match = templateMatches.next();
        templates << match.captured(1);
    }
    
    knowledge.content = QString("C++ file: %1\nIncludes: %2\nNamespaces: %3\nTemplates: %4")
                            .arg(QFileInfo(filePath).baseName())
                            .arg(includes.join(", "))
                            .arg(namespaces.join(", "))
                            .arg(templates.join(", "));
    
    knowledge.tokens = tokenizeContent(content, FileType::CPlusPlus);
    knowledge.keywords = extractKeywords(content, FileType::CPlusPlus);
    knowledge.semanticWeights = calculateSemanticWeights(knowledge.tokens);
    
    knowledge.metadata["include_count"] = includes.size();
    knowledge.metadata["namespace_count"] = namespaces.size();
    knowledge.metadata["template_count"] = templates.size();
    knowledge.metadata["complexity_score"] = (double)calculateComplexityScore(content);
    
    return knowledge;
}

KnowledgeRepresentation AIDigestionEngine::extractFromPython(const QString& content, const QString& filePath) {
    KnowledgeRepresentation knowledge;
    knowledge.id = generateUniqueId();
    knowledge.originalFile = filePath;
    knowledge.fileType = FileType::Python;
    knowledge.timestamp = QDateTime::currentDateTime();
    
    // Extract imports  
    QStringList imports;
    QRegularExpression importRegex(R"((?:from\s+(\w+(?:\.\w+)*)\s+)?import\s+([^\n]+))", QRegularExpression::MultilineOption);
    auto importMatches = importRegex.globalMatch(content);
    while (importMatches.hasNext()) {
        auto match = importMatches.next();
        QString importStr = match.captured(1).isEmpty() ? match.captured(2) : match.captured(1) + "." + match.captured(2);
        imports << importStr;
    }
    
    // Extract decorators
    QStringList decorators;
    QRegularExpression decoratorRegex(R"(@(\w+))", QRegularExpression::MultilineOption);
    auto decoratorMatches = decoratorRegex.globalMatch(content);
    while (decoratorMatches.hasNext()) {
        auto match = decoratorMatches.next();
        decorators << match.captured(1);
    }
    
    // Extract class methods
    QStringList methods;
    QRegularExpression methodRegex(R"(def\s+(\w+)\s*\(self)", QRegularExpression::MultilineOption);
    auto methodMatches = methodRegex.globalMatch(content);
    while (methodMatches.hasNext()) {
        auto match = methodMatches.next();
        methods << match.captured(1);
    }
    
    knowledge.content = QString("Python file: %1\nImports: %2\nDecorators: %3\nMethods: %4")
                            .arg(QFileInfo(filePath).baseName())
                            .arg(imports.join(", "))
                            .arg(decorators.join(", "))
                            .arg(methods.join(", "));
    
    knowledge.tokens = tokenizeContent(content, FileType::Python);
    knowledge.keywords = extractKeywords(content, FileType::Python);
    knowledge.semanticWeights = calculateSemanticWeights(knowledge.tokens);
    
    knowledge.metadata["import_count"] = imports.size();
    knowledge.metadata["decorator_count"] = decorators.size();
    knowledge.metadata["method_count"] = methods.size();
    knowledge.metadata["indentation_level"] = (int)calculateIndentationComplexity(content);
    
    return knowledge;
}

// Helper methods
double AIDigestionEngine::calculateComplexityScore(const QString& content) {
    double score = 0.0;
    
    // Count control structures
    int controlStructures = content.count(QRegularExpression("\\b(if|for|while|switch|try|catch)\\b"));
    score += controlStructures * 0.5;
    
    // Count nested braces (approximation of nesting depth)
    int braceDepth = 0, maxDepth = 0;
    for (QChar c : content) {
        if (c == '{') {
            braceDepth++;
            maxDepth = qMax(maxDepth, braceDepth);
        } else if (c == '}') {
            braceDepth--;
        }
    }
    score += maxDepth * 1.0;
    
    // Count template usage
    int templateCount = content.count(QRegularExpression("template\\s*<"));
    score += templateCount * 2.0;
    
    return score;
}

int AIDigestionEngine::calculateIndentationComplexity(const QString& content) {
    QStringList lines = content.split('\n');
    int maxIndentation = 0;
    
    for (const QString& line : lines) {
        int indentLevel = 0;
        for (QChar c : line) {
            if (c == ' ') indentLevel++;
            else if (c == '\t') indentLevel += 4;
            else break;
        }
        maxIndentation = qMax(maxIndentation, indentLevel / 4); // Assume 4-space indents
    }
    
    return maxIndentation;
}

void AIDigestionEngine::initialize() {
    // Initialize random number generator
    m_randomGenerator.seed(QDateTime::currentSecsSinceEpoch());
    
    // Clear any existing data
    m_knowledgeBase.clear();
    m_dataset = TrainingDataset();
    
    // Set up default configuration
    m_config.maxFileSize = 10 * 1024 * 1024; // 10MB
    m_config.excludePatterns = QStringList() << "*.log" << "*.tmp" << "*.cache" 
                                           << ".git/*" << ".svn/*" << "node_modules/*"
                                           << "build/*" << "bin/*" << "obj/*";
    m_config.includePatterns = QStringList() << "*.cpp" << "*.hpp" << "*.c" << "*.h"
                                           << "*.py" << "*.js" << "*.ts" << "*.asm"
                                           << "*.md" << "*.txt" << "*.json";
    
    qInfo() << "AIDigestionEngine initialized with default configuration";
}QJsonArray AIDigestionEngine::generateTrainingPrompts(const KnowledgeRepresentation& knowledge) { return QJsonArray(); }
