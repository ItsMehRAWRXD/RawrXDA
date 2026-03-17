#include "ai_digestion_engine.hpp"
#include <algorithm>
#include <random>

// KnowledgeRepresentation implementation
nlohmann::json KnowledgeRepresentation::toJson() const {
    nlohmann::json obj;
    obj["id"] = id;
    obj["content"] = content;
    obj["originalFile"] = originalFile;
    obj["fileType"] = static_cast<int>(fileType);
    obj["metadata"] = metadata;
    
    nlohmann::json tokensArray;
    for (const auto& token : tokens) {
        tokensArray.append(token);
    }
    obj["tokens"] = tokensArray;
    
    nlohmann::json keywordsArray;
    for (const auto& keyword : keywords) {
        keywordsArray.append(keyword);
    }
    obj["keywords"] = keywordsArray;
    
    nlohmann::json functionsArray;
    for (const auto& func : functions) {
        functionsArray.append(func);
    }
    obj["functions"] = functionsArray;
    
    nlohmann::json classesArray;
    for (const auto& cls : classes) {
        classesArray.append(cls);
    }
    obj["classes"] = classesArray;
    
    nlohmann::json variablesArray;
    for (const auto& var : variables) {
        variablesArray.append(var);
    }
    obj["variables"] = variablesArray;
    
    nlohmann::json commentsArray;
    for (const auto& comment : comments) {
        commentsArray.append(comment);
    }
    obj["comments"] = commentsArray;
    
    nlohmann::json weightsObj;
    for (auto it = semanticWeights.begin(); it != semanticWeights.end(); ++it) {
        weightsObj[it.key()] = it.value();
    }
    obj["semanticWeights"] = weightsObj;
    
    obj["timestamp"] = timestamp.toString(ISODate);
    obj["contextWindow"] = contextWindow;
    
    return obj;
}

KnowledgeRepresentation KnowledgeRepresentation::fromJson(const nlohmann::json& obj) {
    KnowledgeRepresentation kr;
    kr.id = obj["id"].toString();
    kr.content = obj["content"].toString();
    kr.originalFile = obj["originalFile"].toString();
    kr.fileType = static_cast<FileType>(obj["fileType"]);
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
    
    kr.timestamp = // DateTime::fromString(obj["timestamp"].toString(), ISODate);
    kr.contextWindow = obj["contextWindow"];
    
    return kr;
}

// AIDigestionDataset implementation
nlohmann::json AIDigestionDataset::toJson() const {
    nlohmann::json obj;
    obj["name"] = name;
    obj["description"] = description;
    obj["created"] = created.toString(ISODate);
    obj["totalTokens"] = totalTokens;
    obj["totalSamples"] = totalSamples;
    obj["statistics"] = statistics;
    
    nlohmann::json samplesArray;
    for (const auto& sample : samples) {
        samplesArray.append(sample.toJson());
    }
    obj["samples"] = samplesArray;
    
    nlohmann::json typeCountsObj;
    for (auto it = fileTypeCounts.begin(); it != fileTypeCounts.end(); ++it) {
        typeCountsObj[std::string::number(static_cast<int>(it.key()))] = it.value();
    }
    obj["fileTypeCounts"] = typeCountsObj;
    
    nlohmann::json domainsArray;
    for (const auto& domain : domains) {
        domainsArray.append(domain);
    }
    obj["domains"] = domainsArray;
    
    return obj;
}

AIDigestionDataset AIDigestionDataset::fromJson(const nlohmann::json& obj) {
    AIDigestionDataset td;
    td.name = obj["name"].toString();
    td.description = obj["description"].toString();
    td.created = // DateTime::fromString(obj["created"].toString(), ISODate);
    td.totalTokens = obj["totalTokens"];
    td.totalSamples = obj["totalSamples"];
    td.statistics = obj["statistics"].toObject();
    
    const auto samplesArray = obj["samples"].toArray();
    for (const auto& sample : samplesArray) {
        td.samples.append(KnowledgeRepresentation::fromJson(sample.toObject()));
    }
    
    const auto typeCountsObj = obj["fileTypeCounts"].toObject();
    for (auto it = typeCountsObj.begin(); it != typeCountsObj.end(); ++it) {
        td.fileTypeCounts[static_cast<FileType>(it.key())] = it.value();
    }
    
    const auto domainsArray = obj["domains"].toArray();
    for (const auto& domain : domainsArray) {
        td.domains.append(domain.toString());
    }
    
    return td;
}

// AIDigestionEngine implementation
AIDigestionEngine::AIDigestionEngine()
    
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
    m_progressTimer = new // Timer(this);
    m_progressTimer->setInterval(100); // Update every 100ms  // Signal connection removed\nstatusChanged(m_statusMessage);
    });
    
    // Initialize dataset
    m_dataset.name = "Custom AI Dataset";
    m_dataset.created = // DateTime::currentDateTime();
    m_dataset.totalTokens = 0;
    m_dataset.totalSamples = 0;
    m_dataset.description = "AI model trained from digested content";
    
}

void AIDigestionEngine::setConfig(const DigestionConfig& config) {
    std::mutexLocker locker(&m_mutex);
    m_config = config;
}

DigestionConfig AIDigestionEngine::getConfig() const {
    std::mutexLocker locker(&m_mutex);
    return m_config;
}

void AIDigestionEngine::startDigestion(const std::stringList& inputPaths) {
    if (m_isDigesting) {
        return;
    }


    m_isDigesting = true;
    m_shouldStop = false;
    m_isPaused = false;
    m_progress = 0.0;
    m_processedFiles = 0;
    m_startTime = // DateTime::currentDateTime();
    
    // Collect all files to process
    m_filesToProcess.clear();
    m_totalFiles = 0;
    
    for (const std::string& path : inputPaths) {
        // Info info(path);
        if (info.isFile()) {
            if (shouldProcessFile(path)) {
                m_filesToProcess.append(path);
            }
        } else if (info.isDir()) {
            processDirectory(path);
        }
    }
    
    m_totalFiles = m_filesToProcess.size();
    m_statusMessage = std::string("Starting digestion of %1 files...");
    
    // Start processing in background thread
    m_digestionThread = std::make_unique<std::thread>();
    auto* worker = new DigestionWorker(this);
    worker->);  // Signal connection removed\n});  // Signal connection removed\n  // Signal connection removed\nconnect(worker, &DigestionWorker::error, this, [this](const std::string& error) {
        digestionFailed(error);
        m_isDigesting = false;
    });  // Signal connection removed\nm_digestionThread->start();
    m_progressTimer->start();
    
    statusChanged(m_statusMessage);
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
    statusChanged(m_statusMessage);
}

bool AIDigestionEngine::isDigesting() const {
    return m_isDigesting;
}

double AIDigestionEngine::getProgress() const {
    return m_progress;
}

std::string AIDigestionEngine::getStatusMessage() const {
    return m_statusMessage;
}

int AIDigestionEngine::getProcessedFiles() const {
    return m_processedFiles;
}

int AIDigestionEngine::getTotalFiles() const {
    return m_totalFiles;
}

AIDigestionDataset AIDigestionEngine::getAIDigestionDataset() const {
    std::mutexLocker locker(&m_mutex);
    return m_dataset;
}

bool AIDigestionEngine::saveDataset(const std::string& filePath) const {
    std::mutexLocker locker(&m_mutex);
    
    nlohmann::json doc(m_dataset.toJson());
    
    // File operation removed;
    if (!file.open(std::iostream::WriteOnly)) {
        return false;
    }
    
    file.write(doc.toJson());
    return true;
}

bool AIDigestionEngine::loadDataset(const std::string& filePath) {
    // File operation removed;
    if (!file.open(std::iostream::ReadOnly)) {
        return false;
    }
    
    std::vector<uint8_t> data = file.readAll();
    QJsonParseError error;
    nlohmann::json doc = nlohmann::json::fromJson(data, &error);
    
    if (error.error != QJsonParseError::NoError) {
        return false;
    }
    
    std::mutexLocker locker(&m_mutex);
    m_dataset = AIDigestionDataset::fromJson(doc.object());
    
    return true;
}

FileType AIDigestionEngine::detectFileType(const std::string& filePath) {
    // Info info(filePath);
    std::string extension = info.suffix().toLower();
    
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
    } else if (std::stringList({"cpp", "c", "h", "hpp", "py", "js", "java", "cs", "php", "rb", "go", "rs"}).contains(extension)) {
        return FileType::SourceCode;
    }
    
    return FileType::Unknown;
}

std::string AIDigestionEngine::extractFileContent(const std::string& filePath) {
    // File operation removed;
    if (!file.open(std::iostream::ReadOnly | std::iostream::Text)) {
        return std::string();
    }
    
    std::stringstream stream(&file);
    stream.setEncoding(std::stringConverter::Utf8);
    return stream.readAll();
}

void AIDigestionEngine::startTraining() {
    if (m_isTraining) {
        return;
    }


    if (m_dataset.totalSamples == 0) {
        trainingFailed("No training data available. Please digest content first.");
        return;
    }
    
    m_isTraining = true;
    m_statusMessage = "Preparing training environment...";
    statusChanged(m_statusMessage);
    
    // Prepare training data
    prepareTrainingData();
    
    // Start training in background thread
    m_trainingThread = std::make_unique<std::thread>();
    auto* worker = new TrainingWorker(this);
    worker->);  // Signal connection removed\n});  // Signal connection removed\nstatusChanged(m_statusMessage);
    });  // Signal connection removed\nconnect(worker, &TrainingWorker::error, this, [this](const std::string& error) {
        trainingFailed(error);
        m_isTraining = false;
    });  // Signal connection removed\nm_trainingThread->start();
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
    statusChanged(m_statusMessage);
}

bool AIDigestionEngine::isTraining() const {
    return m_isTraining;
}

void AIDigestionEngine::pauseDigestion() {
    if (m_isDigesting && !m_isPaused) {
        m_isPaused = true;
        m_statusMessage = "Digestion paused";
        statusChanged(m_statusMessage);
    }
}

void AIDigestionEngine::resumeDigestion() {
    if (m_isDigesting && m_isPaused) {
        m_isPaused = false;
        m_pauseCondition.wakeAll();
        m_statusMessage = "Digestion resumed";
        statusChanged(m_statusMessage);
    }
}

void AIDigestionEngine::clearDataset() {
    std::mutexLocker locker(&m_mutex);
    m_dataset.samples.clear();
    m_dataset.totalTokens = 0;
    m_dataset.totalSamples = 0;
    m_dataset.fileTypeCounts.clear();
    m_dataset.domains.clear();
    m_knowledgeBase.clear();
}

void AIDigestionEngine::processFileInternal(const std::string& filePath) {
    if (m_shouldStop) return;
    
    // Wait if paused
    if (m_isPaused) {
        std::mutexLocker locker(&m_mutex);
        m_pauseCondition.wait(&m_mutex);
    }
    
    processFile(filePath);
    
    std::mutexLocker locker(&m_mutex);
    m_processedFiles++;
    m_progress = static_cast<double>(m_processedFiles) / m_totalFiles;
    
    fileProcessed(filePath, m_processedFiles, m_totalFiles);
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
    m_statusMessage = std::string("Digestion completed. Processed %1 files, extracted %2 knowledge samples.")
                      ;
    
    digestionCompleted(m_dataset);
    statusChanged(m_statusMessage);
}

void AIDigestionEngine::onTrainingThreadFinished() {
    m_isTraining = false;
    
    std::string modelPath = // (m_config.outputDirectory).filePath(m_config.modelName + ".gguf");
    m_statusMessage = std::string("Training completed. Model saved to: %1");
    
    trainingCompleted(modelPath);
    statusChanged(m_statusMessage);
}

void AIDigestionEngine::processDirectory(const std::string& dirPath) {
    // DirIterator iterator(dirPath, // Dir::Files, // DirIterator::Subdirectories);
    
    while (iteratorfalse) {
        std::string filePath = iterator;
        if (shouldProcessFile(filePath)) {
            m_filesToProcess.append(filePath);
        }
    }
}

void AIDigestionEngine::processFile(const std::string& filePath) {
    std::chrono::steady_clock timer;
    timer.start();
    
    try {
        std::string content = extractFileContent(filePath);
        if (content.empty() || content.length() < m_config.minContentLength) {
            return;
        }
        
        KnowledgeRepresentation knowledge = analyzeContent(content, filePath);
        if (!knowledge.tokens.empty()) {
            std::mutexLocker locker(&m_mutex);
            m_knowledgeBase.append(knowledge);
            m_dataset.samples.append(knowledge);
            
            knowledgeExtracted(knowledge);
        }


    } catch (const std::exception& e) {
    }
}

KnowledgeRepresentation AIDigestionEngine::analyzeContent(const std::string& content, const std::string& filePath) {
    FileType fileType = detectFileType(filePath);
    
    KnowledgeRepresentation knowledge;
    knowledge.id = generateUniqueId();
    knowledge.originalFile = filePath;
    knowledge.fileType = fileType;
    knowledge.timestamp = // DateTime::currentDateTime();
    
    // Preprocess content
    std::string processedContent = preprocessContent(content, fileType);
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
    knowledge.metadata["fileSize"] = // FileInfo: filePath).size();
    knowledge.metadata["extractionMode"] = static_cast<int>(m_config.mode);
    knowledge.metadata["timestamp"] = knowledge.timestamp.toString(ISODate);
    
    return knowledge;
}

// Content analysis methods implementation
std::stringList AIDigestionEngine::tokenizeContent(const std::string& content, FileType type) {
    std::string cleaned = preprocessContent(content, type);
    std::stringList tokens;
    
    // Simple tokenization based on whitespace and punctuation
    std::regex tokenRegex(R"(\b\w+\b)");
    auto matches = tokenRegex;
    
    while (matchesfalse) {
        auto match = matches;
        std::string token = match"".toLower();
        if (token.length() > 2) {  // Filter out very short tokens
            tokens << token;
        }
    }
    
    // Remove duplicates while preserving order
    std::stringList uniqueTokens;
    std::set<std::string> seen;
    for (const std::string& token : tokens) {
        if (!seen.contains(token)) {
            seen.insert(token);
            uniqueTokens << token;
        }
    }
    
    return uniqueTokens;
}

std::stringList AIDigestionEngine::extractKeywords(const std::string& content, FileType type) {
    std::stringList keywords;
    
    switch (type) {
        case FileType::CPlusPlus: {
            std::regex cppRegex("\\b(class|struct|namespace|template|virtual|override|const|static|public|private|protected)\\b");
            auto matches = cppRegex;
            while (matchesfalse) {
                auto match = matches;
                keywords << match"";
            }
            break;
        }
            
        case FileType::Python: {
            std::regex pyRegex("\\b(def|class|import|from|if|elif|else|for|while|try|except|finally|with|async|await)\\b");
            auto matches = pyRegex;
            while (matchesfalse) {
                auto match = matches;
                keywords << match"";
            }
            break;
        }
            
        case FileType::Assembly: {
            std::regex asmRegex("\\b(mov|add|sub|mul|div|cmp|jmp|je|jne|call|ret|push|pop|lea)\\b", std::regex::CaseInsensitiveOption);
            auto matches = asmRegex;
            while (matchesfalse) {
                auto match = matches;
                keywords << match"";
            }
            break;
        }
            
        case FileType::JavaScript: {
            std::regex jsRegex("\\b(function|const|let|var|if|else|for|while|class|extends|async|await|Promise)\\b");
            auto matches = jsRegex;
            while (matchesfalse) {
                auto match = matches;
                keywords << match"";
            }
            break;
        }
            
        default: {
            // Generic keyword extraction
            std::regex genericRegex("\\b[A-Z][a-zA-Z0-9_]*\\b");
            auto matches = genericRegex;
            while (matchesfalse) {
                auto match = matches;
                keywords << match"";
            }
            break;
        }
    }
    
    keywords.removeDuplicates();
    return keywords;
}

std::stringList AIDigestionEngine::extractComments(const std::string& content, FileType type) {
    std::stringList comments;
    
    if (type == FileType::CPlusPlus || type == FileType::JavaScript) {
        // Single line comments
        std::regex singleLineRegex("//\\s*(.*)");
        auto singleMatches = singleLineRegex;
        while (singleMatchesfalse) {
            auto match = singleMatches;
            comments << match"";
        }
        
        // Multi-line comments
        std::regex multiLineRegex("/\\*\\s*(.*?)\\s*\\*/", std::regex::DotMatchesEverythingOption);
        auto multiMatches = multiLineRegex;
        while (multiMatchesfalse) {
            auto match = multiMatches;
            comments << match"";
        }
    } else if (type == FileType::Python) {
        // Single line comments
        std::regex pythonRegex("#\\s*(.*)");
        auto pyMatches = pythonRegex;
        while (pyMatchesfalse) {
            auto match = pyMatches;
            comments << match"";
        }
        
        // Triple-quoted strings (docstrings)
        std::regex docstringRegex("\\\"\\\"\\\"(.*?)\\\"\\\"\\\"", std::regex::DotMatchesEverythingOption);
        auto docMatches = docstringRegex;
        while (docMatchesfalse) {
            auto match = docMatches;
            comments << match"";
        }
    } else if (type == FileType::Assembly) {
        // Semicolon comments
        std::regex asmRegex(";\\s*(.*)");
        auto asmMatches = asmRegex;
        while (asmMatchesfalse) {
            auto match = asmMatches;
            comments << match"";
        }
    } else {
        // Try to extract any comment-like patterns
        std::regex genericRegex("[#;]\\s*(.*)");
        auto genericMatches = genericRegex;
        while (genericMatchesfalse) {
            auto match = genericMatches;
            comments << match"";
        }
    }
    
    // Clean up comments
    std::stringList cleanedComments;
    for (std::string comment : comments) {
        comment = comment.trimmed();
        if (comment.length() > 5) {  // Filter out very short comments
            cleanedComments << comment;
        }
    }
    
    return cleanedComments;
}

std::map<std::string, double> AIDigestionEngine::calculateSemanticWeights(const std::stringList& tokens) {
    std::map<std::string, double> weights;
    std::map<std::string, int> frequencies;
    
    // Calculate token frequencies
    for (const std::string& token : tokens) {
        frequencies[token]++;
    }
    
    // Calculate TF-IDF-like weights
    int totalTokens = tokens.size();
    int uniqueTokens = frequencies.size();
    
    for (auto it = frequencies.begin(); it != frequencies.end(); ++it) {
        const std::string& token = it.key();
        int freq = it.value();
        
        // Term frequency
        double tf = static_cast<double>(freq) / totalTokens;
        
        // Inverse document frequency (simplified)
        double idf = qLn(static_cast<double>(uniqueTokens) / (1.0 + freq));
        
        // Apply domain-specific boosts
        double boost = 1.0;
        if (token.contains(std::regex(R"(\b(function|class|method|variable|parameter)\b)"))) {
            boost = 1.5;  // Boost programming concepts
        }
        if (token.length() > 8) {
            boost *= 1.2;  // Boost longer, more specific terms
        }
        
        weights[token] = tf * idf * boost;
    }
    
    return weights;
}

std::string AIDigestionEngine::preprocessContent(const std::string& content, FileType type) {
    std::string processed = content;
    
    // Remove excessive whitespace
    processed = processed.simplified();
    
    // Remove string literals and character constants for better analysis
    switch (type) {
        case FileType::CPlusPlus:
        case FileType::C:
        case FileType::JavaScript:
            processed = processed.remove(std::regex(R"("(?:[^"\\]|\\.)*")"));  // String literals
            processed = processed.remove(std::regex(R"('(?:[^'\\]|\\.)*')"));  // Character constants
            break;
            
        case FileType::Python:
            processed = processed.remove(std::regex(R"(""".*?""")"));  // Triple-quoted strings
            processed = processed.remove(std::regex(R"("(?:[^"\\]|\\.)*")"));  // String literals
            processed = processed.remove(std::regex(R"('(?:[^'\\]|\\.)*')"));  // Character constants
            break;
            
        case FileType::Assembly:
            // Remove string constants in assembly
            processed = processed.remove(std::regex(R"(db\s+['""][^'"]*['""])"));
            break;
            
        default:
            break;
    }
    
    // Normalize line endings
    processed = processed.replace("\r\n", "\n").replace("\r", "\n");
    
    return processed;
}

std::string AIDigestionEngine::generateUniqueId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

bool AIDigestionEngine::shouldProcessFile(const std::string& filePath) {
    // Info info(filePath);
    
    // Skip if file is too large (> 10MB)
    if (info.size() > 10 * 1024 * 1024) {
        return false;
    }
    
    // Skip binary files
    std::string suffix = info.suffix().toLower();
    std::stringList binaryExtensions = {"exe", "dll", "so", "dylib", "obj", "o", "a", "lib", "bin", "zip", "tar", "gz"};
    if (binaryExtensions.contains(suffix)) {
        return false;
    }
    
    // Skip hidden files and directories
    if (info.fileName().startsWith('.')) {
        return false;
    }
    
    // Skip common non-code directories
    std::string path = info.string();
    std::stringList skipDirs = {"node_modules", ".git", ".svn", "build", "Debug", "Release", "__pycache__"};
    for (const std::string& skipDir : skipDirs) {
        if (path.contains("" + skipDir + "")) {
            return false;
        }
    }
    
    return true;
}

nlohmann::json AIDigestionEngine::generateStatistics() {
    nlohmann::json stats;
    
    stats["total_files"] = m_totalFiles;
    stats["processed_files"] = m_processedFiles;
    stats["knowledge_entries"] = m_knowledgeBase.size();
    stats["vocabulary_size"] = m_vocabulary.size();
    stats["processing_time_ms"] = m_startTime.msecsTo(// DateTime::currentDateTime());
    
    // File type statistics
    nlohmann::json fileTypes;
    for (auto it = m_fileTypeStats.begin(); it != m_fileTypeStats.end(); ++it) {
        std::string typeName;
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
    nlohmann::json domains;
    for (auto it = m_domainStats.begin(); it != m_domainStats.end(); ++it) {
        domains[it.key()] = it.value();
    }
    stats["domains"] = domains;
    
    return stats;
}

void AIDigestionEngine::prepareTrainingData() {
    m_dataset = AIDigestionDataset();
    m_dataset.samples.clear();
    
    for (const KnowledgeRepresentation& knowledge : m_knowledgeBase) {
        // Generate instruction-response pairs from knowledge
        nlohmann::json prompts = generateTrainingPrompts(knowledge);
        
        for (const void*& promptValue : prompts) {
            nlohmann::json prompt = promptValue.toObject();
            
            KnowledgeRepresentation sample;
            sample.id = generateUniqueId();
            sample.content = prompt["instruction"].toString() + " " + 
                           prompt["input"].toString() + " " + 
                           prompt["output"].toString();
            sample.originalFile = knowledge.originalFile;
            sample.metadata = knowledge.metadata;
            sample.fileType = knowledge.fileType;
            sample.timestamp = // DateTime::currentDateTime();
            
            m_dataset.samples.append(sample);
        }
    }
    
    // Update dataset metadata
    m_dataset.totalSamples = m_dataset.samples.size();
    m_dataset.totalTokens = 0;
    
    for (const KnowledgeRepresentation& sample : m_dataset.samples) {
        m_dataset.totalTokens += sample.content.split(' ', SkipEmptyParts).size();
    }
    
}

// Specialized content extractors
KnowledgeRepresentation AIDigestionEngine::extractFromSourceCode(const std::string& content, const std::string& filePath) {
    KnowledgeRepresentation knowledge;
    knowledge.id = generateUniqueId();
    knowledge.originalFile = filePath;
    knowledge.timestamp = // DateTime::currentDateTime();
    
    // Info info(filePath);
    FileType fileType = detectFileType(filePath);
    knowledge.fileType = fileType;
    
    // Extract functions/methods
    std::stringList functions;
    switch (fileType) {
        case FileType::CPlusPlus:
        case FileType::C: {
            std::regex funcRegex(R"(\b\w+\s+(\w+)\s*\([^)]*\)\s*\{)", std::regex::MultilineOption);
            auto matches = funcRegex;
            while (matchesfalse) {
                auto match = matches;
                functions << match"";
            }
            break;
        }
        case FileType::Python: {
            std::regex pyRegex(R"(def\s+(\w+)\s*\()", std::regex::MultilineOption);
            auto matches = pyRegex;
            while (matchesfalse) {
                auto match = matches;
                functions << match"";
            }
            break;
        }
        case FileType::JavaScript: {
            std::regex jsRegex(R"(function\s+(\w+)\s*\()", std::regex::MultilineOption);
            auto matches = jsRegex;
            while (matchesfalse) {
                auto match = matches;
                functions << match"";
            }
            break;
        }
        default:
            break;
    }
    
    // Extract classes/structures
    std::stringList classes;
    if (fileType == FileType::CPlusPlus || fileType == FileType::C) {
        std::regex classRegex(R"(\b(?:class|struct)\s+(\w+))", std::regex::MultilineOption);
        auto matches = classRegex;
        while (matchesfalse) {
            auto match = matches;
            classes << match"";
        }
    } else if (fileType == FileType::Python) {
        std::regex pyClassRegex(R"(class\s+(\w+)\s*[:\(])", std::regex::MultilineOption);
        auto matches = pyClassRegex;
        while (matchesfalse) {
            auto match = matches;
            classes << match"";
        }
    }
    
    knowledge.content = std::string("File: %1\nFunctions: %2\nClasses: %3")
                            )
                            )
                            );
    knowledge.tokens = tokenizeContent(content, fileType);
    knowledge.keywords = extractKeywords(content, fileType);
    knowledge.semanticWeights = calculateSemanticWeights(knowledge.tokens);
    
    knowledge.metadata["file_type"] = static_cast<int>(fileType);
    knowledge.metadata["function_count"] = functions.size();
    knowledge.metadata["class_count"] = classes.size();
    knowledge.metadata["line_count"] = content.count('\n') + 1;
    
    return knowledge;
}

KnowledgeRepresentation AIDigestionEngine::extractFromDocumentation(const std::string& content, const std::string& filePath) {
    KnowledgeRepresentation knowledge;
    knowledge.id = generateUniqueId();
    knowledge.originalFile = filePath;
    knowledge.fileType = FileType::Documentation;
    knowledge.timestamp = // DateTime::currentDateTime();
    
    // Extract headers/sections
    std::stringList headers;
    std::regex headerRegex("^#+\\s+(.+)$", std::regex::MultilineOption);
    auto headerMatches = headerRegex;
    while (headerMatchesfalse) {
        auto match = headerMatches;
        headers << match"";
    }
    
    // Extract code blocks
    std::stringList codeBlocks;
    std::regex codeRegex("```[\\w]*\\n(.*?)\\n```", 
                                std::regex::DotMatchesEverythingOption | std::regex::MultilineOption);
    auto codeMatches = codeRegex;
    while (codeMatchesfalse) {
        auto match = codeMatches;
        codeBlocks << match"";
    }
    
    knowledge.content = std::string("Documentation: %1\nSections: %2\nCode examples: %3")
                            .baseName())
                            )
                            ));
    
    knowledge.tokens = tokenizeContent(content, FileType::Documentation);
    knowledge.keywords = extractKeywords(content, FileType::Documentation);
    knowledge.semanticWeights = calculateSemanticWeights(knowledge.tokens);
    
    knowledge.metadata["header_count"] = headers.size();
    knowledge.metadata["code_block_count"] = codeBlocks.size();
    knowledge.metadata["word_count"] = content.split(std::regex("\\s+"), SkipEmptyParts).size();
    
    return knowledge;
}

KnowledgeRepresentation AIDigestionEngine::extractFromAssembly(const std::string& content, const std::string& filePath) {
    KnowledgeRepresentation knowledge;
    knowledge.id = generateUniqueId();
    knowledge.originalFile = filePath;
    knowledge.fileType = FileType::Assembly;
    knowledge.timestamp = // DateTime::currentDateTime();
    
    // Extract labels/functions
    std::stringList labels;
    std::regex labelRegex(R"(^(\w+):)", std::regex::MultilineOption);
    auto labelMatches = labelRegex;
    while (labelMatchesfalse) {
        auto match = labelMatches;
        labels << match"";
    }
    
    // Extract instructions
    std::stringList instructions;
    std::regex instrRegex(R"(\b(mov|add|sub|mul|div|cmp|jmp|je|jne|jz|jnz|call|ret|push|pop|lea|xor|and|or|not|shl|shr)\b)", 
                                std::regex::CaseInsensitiveOption);
    auto instrMatches = instrRegex;
    while (instrMatchesfalse) {
        auto match = instrMatches;
        instructions << match"";
    }
    
    // Count instruction frequencies
    std::map<std::string, int> instrFreq;
    for (const std::string& instr : instructions) {
        instrFreq[instr.toLower()]++;
    }
    
    knowledge.content = std::string("Assembly file: %1\nLabels: %2\nUnique instructions: %3")
                            .baseName())
                            )
                            .join(", "));
    
    knowledge.tokens = tokenizeContent(content, FileType::Assembly);
    knowledge.keywords = extractKeywords(content, FileType::Assembly);
    knowledge.semanticWeights = calculateSemanticWeights(knowledge.tokens);
    
    knowledge.metadata["label_count"] = labels.size();
    knowledge.metadata["instruction_count"] = instructions.size();
    knowledge.metadata["unique_instructions"] = instrFreq.size();
    
    return knowledge;
}

KnowledgeRepresentation AIDigestionEngine::extractFromCPlusPlus(const std::string& content, const std::string& filePath) {
    KnowledgeRepresentation knowledge;
    knowledge.id = generateUniqueId();
    knowledge.originalFile = filePath;
    knowledge.fileType = FileType::CPlusPlus;
    knowledge.timestamp = // DateTime::currentDateTime();
    
    // Extract includes
    std::stringList includes;
    std::regex includeRegex(R"(#include\s*[<"]([^>"]+)[>"])", std::regex::MultilineOption);
    auto includeMatches = includeRegex;
    while (includeMatchesfalse) {
        auto match = includeMatches;
        includes << match"";
    }
    
    // Extract namespaces
    std::stringList namespaces;
    std::regex nsRegex(R"(namespace\s+(\w+))", std::regex::MultilineOption);
    auto nsMatches = nsRegex;
    while (nsMatchesfalse) {
        auto match = nsMatches;
        namespaces << match"";
    }
    
    // Extract templates
    std::stringList templates;
    std::regex templateRegex(R"(template\s*<[^>]+>\s*(?:class|struct)\s+(\w+))", std::regex::MultilineOption);
    auto templateMatches = templateRegex;
    while (templateMatchesfalse) {
        auto match = templateMatches;
        templates << match"";
    }
    
    knowledge.content = std::string("C++ file: %1\nIncludes: %2\nNamespaces: %3\nTemplates: %4")
                            .baseName())
                            )
                            )
                            );
    
    knowledge.tokens = tokenizeContent(content, FileType::CPlusPlus);
    knowledge.keywords = extractKeywords(content, FileType::CPlusPlus);
    knowledge.semanticWeights = calculateSemanticWeights(knowledge.tokens);
    
    knowledge.metadata["include_count"] = includes.size();
    knowledge.metadata["namespace_count"] = namespaces.size();
    knowledge.metadata["template_count"] = templates.size();
    knowledge.metadata["complexity_score"] = (double)calculateComplexityScore(content);
    
    return knowledge;
}

KnowledgeRepresentation AIDigestionEngine::extractFromPython(const std::string& content, const std::string& filePath) {
    KnowledgeRepresentation knowledge;
    knowledge.id = generateUniqueId();
    knowledge.originalFile = filePath;
    knowledge.fileType = FileType::Python;
    knowledge.timestamp = // DateTime::currentDateTime();
    
    // Extract imports  
    std::stringList imports;
    std::regex importRegex(R"((?:from\s+(\w+(?:\.\w+)*)\s+)?import\s+([^\n]+))", std::regex::MultilineOption);
    auto importMatches = importRegex;
    while (importMatchesfalse) {
        auto match = importMatches;
        std::string importStr = match"".empty() ? match"" : match"" + "." + match"";
        imports << importStr;
    }
    
    // Extract decorators
    std::stringList decorators;
    std::regex decoratorRegex(R"(@(\w+))", std::regex::MultilineOption);
    auto decoratorMatches = decoratorRegex;
    while (decoratorMatchesfalse) {
        auto match = decoratorMatches;
        decorators << match"";
    }
    
    // Extract class methods
    std::stringList methods;
    std::regex methodRegex(R"(def\s+(\w+)\s*\(self)", std::regex::MultilineOption);
    auto methodMatches = methodRegex;
    while (methodMatchesfalse) {
        auto match = methodMatches;
        methods << match"";
    }
    
    knowledge.content = std::string("Python file: %1\nImports: %2\nDecorators: %3\nMethods: %4")
                            .baseName())
                            )
                            )
                            );
    
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
double AIDigestionEngine::calculateComplexityScore(const std::string& content) {
    double score = 0.0;
    
    // Count control structures
    int controlStructures = content.count(std::regex("\\b(if|for|while|switch|try|catch)\\b"));
    score += controlStructures * 0.5;
    
    // Count nested braces (approximation of nesting depth)
    int braceDepth = 0, maxDepth = 0;
    for (char c : content) {
        if (c == '{') {
            braceDepth++;
            maxDepth = qMax(maxDepth, braceDepth);
        } else if (c == '}') {
            braceDepth--;
        }
    }
    score += maxDepth * 1.0;
    
    // Count template usage
    int templateCount = content.count(std::regex("template\\s*<"));
    score += templateCount * 2.0;
    
    return score;
}

int AIDigestionEngine::calculateIndentationComplexity(const std::string& content) {
    std::stringList lines = content.split('\n');
    int maxIndentation = 0;
    
    for (const std::string& line : lines) {
        int indentLevel = 0;
        for (char c : line) {
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
    m_randomGenerator.seed(// DateTime::currentSecsSinceEpoch());
    
    // Clear any existing data
    m_knowledgeBase.clear();
    m_dataset = AIDigestionDataset();
    
    // Set up default configuration
    m_config.maxFileSize = 10 * 1024 * 1024; // 10MB
    m_config.excludePatterns = std::stringList() << "*.log" << "*.tmp" << "*.cache" 
                                           << ".git/*" << ".svn/*" << "node_modules/*"
                                           << "build/*" << "bin/*" << "obj/*";
    m_config.includePatterns = std::stringList() << "*.cpp" << "*.hpp" << "*.c" << "*.h"
                                           << "*.py" << "*.js" << "*.ts" << "*.asm"
                                           << "*.md" << "*.txt" << "*.json";
    
}nlohmann::json AIDigestionEngine::generateTrainingPrompts(const KnowledgeRepresentation& knowledge) { return nlohmann::json(); }

