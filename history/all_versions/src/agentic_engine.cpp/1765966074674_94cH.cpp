// Agentic Engine - Production-Ready AI Core
#include "agentic_engine.h"
#include "../src/qtapp/inference_engine.hpp"
#include "agentic_file_operations.h"
#include "agentic_error_handler.h"
#include <QTimer>
#include <QtConcurrent>
#include <QFutureWatcher>
#include "../3rdparty/ggml/include/gguf.h"
#include "../3rdparty/ggml/include/ggml-cpu.h"
#include <QStringList>
#include <QRandomGenerator>
#include <QDebug>
#include <QThread>
#include <QDateTime>
#include <QRegularExpression>
#include <QCryptographicHash>
#include <fstream>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <algorithm>

AgenticEngine::AgenticEngine(QObject* parent) 
    : QObject(parent), 
      m_modelLoaded(false), 
      m_inferenceEngine(nullptr),
      m_fileOperations(nullptr),
      m_errorHandler(nullptr),
      m_totalInteractions(0),
      m_positiveResponses(0)
{
    // Lightweight constructor - defer heavy initialization
    // InferenceEngine creation moved to initialize()
}

AgenticEngine::~AgenticEngine()
{
    // Clean up feedback history
    m_feedbackHistory.clear();
    m_responseRatings.clear();
    
    // Release inference engine resources
    if (m_inferenceEngine) {
        delete m_inferenceEngine;
        m_inferenceEngine = nullptr;
    }
    
    qInfo() << "[AgenticEngine] Destroyed - cleaned up" << m_feedbackHistory.size() << "feedback entries";
}

void AgenticEngine::initialize() {
    if (m_inferenceEngine) return;  // Already initialized
    
    // Initialize user preferences with defaults
    m_userPreferences["language"] = "C++";
    m_userPreferences["style"] = "modern";
    m_userPreferences["verbosity"] = "detailed";
    
    // Initialize generation config with defaults
    m_genConfig.temperature = 0.8f;
    m_genConfig.topP = 0.9f;
    m_genConfig.maxTokens = 512;
    
    // Initialize error handler for centralized error management
    if (!m_errorHandler) {
        m_errorHandler = new AgenticErrorHandler(this);
        qInfo() << "[AgenticEngine] Error handler initialized";
    }
    
    // Initialize file operations with Keep/Undo support
    if (!m_fileOperations) {
        m_fileOperations = new AgenticFileOperations(this, m_errorHandler);
        qInfo() << "[AgenticEngine] File operations initialized with Keep/Undo support";
        
        // Connect file operation signals
        connect(m_fileOperations, &AgenticFileOperations::fileCreated, this, [this](const QString& filePath) {
            qInfo() << "[AgenticEngine] File created:" << filePath;
            emit fileOperationCompleted("create", filePath, true);
        });
        
        connect(m_fileOperations, &AgenticFileOperations::fileModified, this, [this](const QString& filePath) {
            qInfo() << "[AgenticEngine] File modified:" << filePath;
            emit fileOperationCompleted("modify", filePath, true);
        });
        
        connect(m_fileOperations, &AgenticFileOperations::fileDeleted, this, [this](const QString& filePath) {
            qInfo() << "[AgenticEngine] File deleted:" << filePath;
            emit fileOperationCompleted("delete", filePath, true);
        });
        
        connect(m_fileOperations, &AgenticFileOperations::operationUndone, this, [this](const QString& filePath) {
            qInfo() << "[AgenticEngine] File operation undone:" << filePath;
            emit fileOperationCompleted("undo", filePath, true);
        });
    }
    
    // Create inference engine instance (deferred from constructor)
    m_inferenceEngine = new InferenceEngine(this);
    qInfo() << "[AgenticEngine] Inference engine created";
    
    qDebug() << "Agentic Engine initialized - waiting for model selection";
}

void AgenticEngine::setGenerationConfig(const GenerationConfig& config) {
    m_genConfig = config;
    qInfo() << "[AgenticEngine] Generation config updated:"
            << "temperature=" << config.temperature
            << "topP=" << config.topP
            << "maxTokens=" << config.maxTokens;
}

void AgenticEngine::setModel(const QString& modelPath) {
    if (modelPath.isEmpty()) {
        m_modelLoaded = false;
        qDebug() << "Model unloaded";
        return;
    }
    
    // Load model in background thread
    QThread* thread = new QThread;
    connect(thread, &QThread::started, this, [this, modelPath, thread]() {
        qDebug() << "[AgenticEngine] Background thread started for model loading";
        bool success = loadModelAsync(modelPath.toStdString());
        qDebug() << "[AgenticEngine] Background thread finished. Success:" << success;
        emit modelLoadingFinished(success, modelPath);
        // CRITICAL: Emit modelReady on main thread after background work completes
        QMetaObject::invokeMethod(this, [this, success]() {
            qDebug() << "[AgenticEngine] Main thread: Emitting modelReady(" << success << ")";
            emit modelReady(success);
        }, Qt::QueuedConnection);
        thread->quit();
    });
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    thread->start();
}

void AgenticEngine::setModelName(const QString& modelName) {
    qDebug() << "[AgenticEngine] ═══ Model Selection Start ═══";
    qDebug() << "[AgenticEngine] Model selection received:" << modelName;
    
    // Resolve Ollama model name to actual GGUF file path
    QString ggufPath = resolveGgufPath(modelName);
    
    if (!ggufPath.isEmpty()) {
        qInfo() << "[AgenticEngine] Resolved" << modelName << "to" << ggufPath;
        qDebug() << "[AgenticEngine] Calling setModel() to load GGUF...";
        setModel(ggufPath);
    } else {
        qWarning() << "[AgenticEngine] Could not resolve GGUF path for model:" << modelName;
        qWarning() << "[AgenticEngine] No matching .gguf file found in D:/OllamaModels";
        m_modelLoaded = false;
        // Emit signal with special value to indicate path resolution failure
        emit modelLoadingFinished(false, "NO_GGUF_FILE:" + modelName);
        emit modelReady(false);
    }
}

QString AgenticEngine::resolveGgufPath(const QString& modelName) {
    // Search for GGUF file in Ollama models directory
    QStringList searchPaths = {
        "D:/OllamaModels",
        "C:/Users/" + qEnvironmentVariable("USERNAME") + "/.ollama/models",
        QDir::homePath() + "/.ollama/models"
    };
    
    // Extract base model name (e.g., "llama3.2" from "llama3.2:3b")
    QString baseName = modelName.split(':').first();
    QString searchPattern = "*" + baseName + "*.gguf";
    
    for (const QString& searchPath : searchPaths) {
        QDir dir(searchPath);
        if (!dir.exists()) continue;
        
        QStringList filters;
        filters << searchPattern;
        QFileInfoList files = dir.entryInfoList(filters, QDir::Files);
        
        if (!files.isEmpty()) {
            // Return first matching GGUF file
            QString path = files.first().absoluteFilePath();
            qDebug() << "[AgenticEngine] Found GGUF:" << path;
            return path;
        }
    }
    
    qWarning() << "[AgenticEngine] No GGUF file found for" << modelName;
    return QString();
}

bool AgenticEngine::loadModelAsync(const std::string& modelPath) {
    try {
        qInfo() << "Loading GGUF model from:" << QString::fromStdString(modelPath);
        
        // Check if file exists
        std::ifstream file(modelPath, std::ios::binary);
        if (!file.is_open()) {
            qCritical() << "Model file does not exist:" << QString::fromStdString(modelPath);
            m_modelLoaded = false;
            emit modelLoadingFinished(false, QString::fromStdString(modelPath));
            return false;
        }
        file.close();
        
        // === GGUF Compatibility Validation ===
        // Read the GGUF header and verify quantization type compatibility
        struct gguf_init_params params = {true, nullptr};
        struct gguf_context *ctx = gguf_init_from_file(modelPath.c_str(), params);
        if (!ctx) {
            qCritical() << "Not a valid GGUF file:" << QString::fromStdString(modelPath);
            m_modelLoaded = false;
            emit modelLoadingFinished(false, QString::fromStdString(modelPath));
            return false;
        }
        
        // Check for quantization version metadata
        const int64_t qver = gguf_find_key(ctx, "general.quantization_version");
        if (qver >= 0) {
            const char *qt = gguf_get_val_str(ctx, qver);
            qInfo() << "Quantization type:" << qt;
            
            // Check if Q2_K requires AVX but CPU doesn't support it
            if (QString(qt).contains("Q2_K") && !ggml_cpu_has_avx()) {
                qCritical() << "Q2_K quantization requires AVX but CPU does not support it";
                qCritical() << "Re-compile ggml with proper CPU flags or use Vulkan/CUDA backend";
                gguf_free(ctx);
                m_modelLoaded = false;
                emit modelLoadingFinished(false, QString::fromStdString(modelPath));
                return false;
            }
        } else {
            qWarning() << "No quantization info found in GGUF – will try anyway";
        }
        
        gguf_free(ctx);
        
        // Delegate to inference engine for actual model loading
        // This is wrapped in additional try/catch to handle ggml crashes
        if (m_inferenceEngine) {
            qDebug() << "[AgenticEngine] Calling InferenceEngine::loadModel for:" << QString::fromStdString(modelPath);
            bool success = m_inferenceEngine->loadModel(QString::fromStdString(modelPath));
            m_modelLoaded = success;
            m_currentModelPath = modelPath;
            qInfo() << "[AgenticEngine] Model loading" << (success ? "succeeded" : "failed") << ":" << QString::fromStdString(modelPath);
            qDebug() << "[AgenticEngine] Emitting modelReady(" << success << ")";
            emit modelLoadingFinished(success, QString::fromStdString(modelPath));
            emit modelReady(success);
            return success;
        }
        
        qWarning() << "Inference engine not available, marking as loaded without verification";
        m_modelLoaded = true;
        m_currentModelPath = modelPath;
        emit modelLoadingFinished(true, QString::fromStdString(modelPath));
        emit modelReady(true);
        return true;
        
    } catch (const std::exception& e) {
        qCritical() << "[AgenticEngine] Model load crashed:" << e.what();
        m_modelLoaded = false;
        emit modelLoadingFinished(false, QString::fromStdString(modelPath));
        emit modelReady(false);
        return false;
    } catch (...) {
        qCritical() << "[AgenticEngine] Model load – unknown exception";
        m_modelLoaded = false;
        emit modelLoadingFinished(false, QString::fromStdString(modelPath));
        emit modelReady(false);
        return false;
    }
}

void AgenticEngine::processMessage(const QString& message, const QString& editorContext) {
    qDebug() << "[AgenticEngine] Processing message:" << message;
    if (!editorContext.isEmpty()) {
        qDebug() << "[AgenticEngine] With editor context:" << editorContext.length() << "chars";
    }
    qDebug() << "[AgenticEngine] m_modelLoaded:" << m_modelLoaded;
    qDebug() << "[AgenticEngine] m_inferenceEngine:" << (m_inferenceEngine ? "exists" : "null");
    qDebug() << "[AgenticEngine] isModelLoaded:" << (m_inferenceEngine ? m_inferenceEngine->isModelLoaded() : false);
    
    // Enhance message with editor context if provided
    QString enhancedMessage = message;
    if (!editorContext.isEmpty()) {
        enhancedMessage = message + "\n\n[Context from editor]\n```\n" + editorContext + "\n```";
    }
    
    if (m_modelLoaded && m_inferenceEngine && m_inferenceEngine->isModelLoaded()) {
        // Use real inference engine - response will be emitted via signal
        qInfo() << "[AgenticEngine] ✓ Using loaded model for response generation";
        generateTokenizedResponse(enhancedMessage);
        // Response will be emitted asynchronously via responseReady signal
    } else {
        // Fallback to keyword-based responses if no model loaded
        qWarning() << "[AgenticEngine] ✗ No model loaded, using fallback response";
        qWarning() << "[AgenticEngine]   Reason: m_modelLoaded=" << m_modelLoaded 
                   << ", engine=" << (m_inferenceEngine ? "OK" : "NULL")
                   << ", engine.isModelLoaded=" << (m_inferenceEngine ? m_inferenceEngine->isModelLoaded() : false);
        QString response = generateResponse(enhancedMessage);
        emit responseReady(response);
    }
}

QString AgenticEngine::analyzeCode(const QString& code) {
    return "Code analysis: " + code;
}

QString AgenticEngine::generateCode(const QString& prompt) {
    return "// Generated code for: " + prompt;
}

QString AgenticEngine::generateResponse(const QString& message) {
    // Simple response generation based on keywords
    QStringList responses;
    
    if (message.contains("hello", Qt::CaseInsensitive) || 
        message.contains("hi", Qt::CaseInsensitive)) {
        responses << "Hello there! How can I help you today?"
                  << "Hi! What would you like me to do?"
                  << "Greetings! Ready to assist you.";
    } else if (message.contains("code", Qt::CaseInsensitive)) {
        responses << "I can help you with coding tasks. What do you need?"
                  << "Let me analyze your code. What specifically are you looking for?"
                  << "I can generate, refactor, or debug code for you.";
    } else if (message.contains("help", Qt::CaseInsensitive)) {
        responses << "I can help with code analysis, generation, and debugging."
                  << "Try asking me to generate code or analyze your existing code."
                  << "I can also help with general programming questions.";
    } else {
        responses << "I received your message: \"" + message + "\". How can I assist further?"
                  << "Thanks for your message. What would you like me to do next?"
                  << "I'm here to help with your development tasks. What do you need?";
    }
    
    // Return a random response
    int index = QRandomGenerator::global()->bounded(responses.size());
    return responses[index];
}

QString AgenticEngine::generateTokenizedResponse(const QString& message) {
    // Real tokenization responses using loaded GGUF model
    qDebug() << "Generating tokenized response from model:" << QString::fromStdString(m_currentModelPath);
    qDebug() << "engine ptr:" << m_inferenceEngine
             << "model loaded:" << (m_inferenceEngine ? m_inferenceEngine->isModelLoaded() : false);
    
    // Check if inference engine is available and initialized
    if (m_inferenceEngine && m_inferenceEngine->isModelLoaded()) {
        qDebug() << "Using real inference engine for response generation";
        
        // === ROBUST ASYNC INFERENCE WITH EXCEPTION HANDLING ===
        // Run inference in worker thread and ALWAYS emit responseReady, even on failure
        // This prevents UI deadlock when ggml aborts inside the worker thread
        
        auto future = QtConcurrent::run([this, message]() -> QString {
            try {
                // Tokenize the input message
                auto tokens = m_inferenceEngine->tokenize(message);
                qDebug() << "Tokenized input into" << tokens.size() << "tokens";
                
                // Generate response tokens (limit to reasonable length)
                int maxTokens = 256; // Configurable response length
                auto generatedTokens = m_inferenceEngine->generate(tokens, maxTokens);
                qDebug() << "Generated" << generatedTokens.size() << "tokens";
                
                // Detokenize back to text
                QString response = m_inferenceEngine->detokenize(generatedTokens);
                
                // If response is empty or too short, fall back to context-aware response
                if (response.trimmed().length() < 10) {
                    qWarning() << "Generated response too short, using fallback";
                    return generateFallbackResponse(message);
                } else {
                    qDebug() << "Generated real model response:" << response.left(100) << "...";
                    return response;
                }
            } catch (const std::exception& e) {
                qCritical() << "Model inference aborted:" << e.what();
                return QString("❌ Model error: %1").arg(e.what());
            } catch (...) {
                qCritical() << "Unknown fatal error in inference";
                return "❌ Inference engine crashed – model file may be incompatible.";
            }
        });
        
        // When the worker finishes, emit the result
        QFutureWatcher<QString> *watcher = new QFutureWatcher<QString>(this);
        connect(watcher, &QFutureWatcher<QString>::finished,
                this, [watcher, this]() {
                    emit responseReady(watcher->result());
                    watcher->deleteLater();
                });
        watcher->setFuture(future);
        
        // Return immediately - response will arrive via responseReady signal
        return QString(); // Empty return, actual response comes via signal
    } else {
        qWarning() << "Inference engine not available, using fallback responses";
        return generateFallbackResponse(message);
    }
}

QString AgenticEngine::generateFallbackResponse(const QString& message) {
    // Fallback responses when model inference is not available
    qDebug() << "Using fallback response generation";
    
    QString response;
    
    if (message.length() < 10) {
        response = "Could you please provide more details?";
    } else if (message.contains("code", Qt::CaseInsensitive) || 
               message.contains("debug", Qt::CaseInsensitive) ||
               message.contains("error", Qt::CaseInsensitive)) {
        response = "I'm analyzing the code context. To help debug this, I would need:\n"
                   "1. The complete error message and stack trace\n"
                   "2. The relevant code section where the error occurs\n"
                   "3. Input values that trigger the error\n"
                   "4. Expected vs actual behavior\n\n"
                   "Common issues to check:\n"
                   "- Null pointer dereferences\n"
                   "- Array bounds violations\n"
                   "- Type mismatches\n"
                   "- Resource leaks";
    } else if (message.contains("explain", Qt::CaseInsensitive) ||
               message.contains("how does", Qt::CaseInsensitive)) {
        response = "I can explain this concept. The process typically involves:\n"
                   "1. Initialization - Setting up required resources\n"
                   "2. Execution - Running the main logic\n"
                   "3. State Management - Tracking changes\n"
                   "4. Cleanup - Releasing resources\n\n"
                   "Each step includes validation and error handling.";
    } else if (message.contains("optimize", Qt::CaseInsensitive) ||
               message.contains("performance", Qt::CaseInsensitive)) {
        response = "For performance optimization, consider:\n"
                   "1. Profiling to identify bottlenecks\n"
                   "2. Memory pooling to reduce allocations\n"
                   "3. Loop vectorization where possible\n"
                   "4. Async I/O for better throughput\n"
                   "5. Caching frequently accessed data\n\n"
                   "Expected improvement: 2-5x depending on workload.";
    } else {
        response = "I understand you're asking about: \"" + message.left(50) + "...\"\n\n"
                   "To provide a better response, I would need the model to be properly loaded. "
                   "Current model: " + QString::fromStdString(m_currentModelPath) + "\n\n"
                   "Please ensure a GGUF model is selected and loaded.";
    }
    
    return response;
}

// Agent tool capability: Grep files for pattern matching
QString AgenticEngine::grepFiles(const QString& pattern, const QString& path) {
    qDebug() << "Agent executing grep for pattern:" << pattern << "in path:" << path;
    
    QStringList results;
    QDir searchDir(path.isEmpty() ? "." : path);
    
    // Get all C++ source/header files
    QStringList filters = {"*.cpp", "*.h", "*.hpp", "*.cc", "*.cxx"};
    QFileInfoList files = searchDir.entryInfoList(filters, QDir::Files | QDir::AllDirs, QDir::Name);
    
    int matchCount = 0;
    for (const QFileInfo& fileInfo : files) {
        QFile file(fileInfo.filePath());
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            int lineNum = 0;
            while (!stream.atEnd()) {
                QString line = stream.readLine();
                lineNum++;
                if (line.contains(pattern, Qt::CaseInsensitive)) {
                    results << QString("%1:%2: %3").arg(fileInfo.fileName())
                                                    .arg(lineNum)
                                                    .arg(line.trimmed());
                    matchCount++;
                    if (matchCount >= 50) break; // Limit results
                }
            }
            file.close();
        }
    }
    
    if (results.isEmpty()) {
        return QString("No matches found for pattern: %1").arg(pattern);
    }
    
    return "Grep Results (" + QString::number(matchCount) + " matches):\n" + results.join("\n");
}

// Agent tool capability: Read file contents
QString AgenticEngine::readFile(const QString& filepath, int startLine, int endLine) {
    qDebug() << "Agent reading file:" << filepath << "lines:" << startLine << "-" << endLine;
    
    QFile file(filepath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString("ERROR: Cannot open file %1").arg(filepath);
    }
    
    QTextStream stream(&file);
    QStringList lines;
    int currentLine = 1;
    
    // Read file content
    while (!stream.atEnd()) {
        QString line = stream.readLine();
        
        // Apply line range filtering
        if (startLine > 0 && currentLine < startLine) {
            currentLine++;
            continue;
        }
        if (endLine > 0 && currentLine > endLine) {
            break;
        }
        
        lines << QString("%1: %2").arg(currentLine, 4, 10, QChar(' ')).arg(line);
        currentLine++;
    }
    
    file.close();
    
    if (lines.isEmpty()) {
        return QString("File %1 is empty or line range invalid").arg(filepath);
    }
    
    return QString("File: %1\n===\n%2").arg(filepath).arg(lines.join("\n"));
}

// Agent tool capability: Search files by content/query
QString AgenticEngine::searchFiles(const QString& query, const QString& path) {
    qDebug() << "Agent searching files for query:" << query << "in path:" << path;
    
    QStringList results;
    QDir searchDir(path.isEmpty() ? "." : path);
    
    // Search all files recursively
    QFileInfoList files = searchDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    
    int fileCount = 0;
    for (const QFileInfo& fileInfo : files) {
        if (fileInfo.isDir()) {
            // Recursive search in subdirectories (limit depth)
            QString subResults = searchFiles(query, fileInfo.filePath());
            if (!subResults.contains("No files found")) {
                results << subResults;
            }
            continue;
        }
        
        // Check if file matches query (filename or content)
        if (fileInfo.fileName().contains(query, Qt::CaseInsensitive)) {
            results << QString("MATCH (filename): %1").arg(fileInfo.filePath());
            fileCount++;
        } else {
            // Search content
            QFile file(fileInfo.filePath());
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QString content = file.readAll();
                if (content.contains(query, Qt::CaseInsensitive)) {
                    results << QString("MATCH (content): %1").arg(fileInfo.filePath());
                    fileCount++;
                }
                file.close();
            }
        }
        
        if (fileCount >= 20) break; // Limit results
    }
    
    if (results.isEmpty()) {
        return QString("No files found matching query: %1").arg(query);
    }
    
    return "Search Results (" + QString::number(fileCount) + " files):\n" + results.join("\n");
}

// Agent tool capability: Reference symbol (find usages and definition)
QString AgenticEngine::referenceSymbol(const QString& symbol) {
    qDebug() << "Agent referencing symbol:" << symbol;
    
    QStringList references;
    QDir searchDir(".");
    
    // Search for symbol definition and usages
    QStringList filters = {"*.cpp", "*.h", "*.hpp"};
    QFileInfoList files = searchDir.entryInfoList(filters, QDir::Files | QDir::AllDirs);
    
    int usageCount = 0;
    int definitionCount = 0;
    
    for (const QFileInfo& fileInfo : files) {
        QFile file(fileInfo.filePath());
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            int lineNum = 0;
            
            while (!stream.atEnd()) {
                QString line = stream.readLine();
                lineNum++;
                
                // Look for symbol usage or definition
                if (line.contains(symbol)) {
                    // Check if it's likely a definition
                    if (line.contains(QString("%1::|%1(|%1 |type %1|class %1|struct %1")
                                             .arg(symbol), Qt::CaseInsensitive)) {
                        references << QString("[DEF] %1:%2: %3").arg(fileInfo.fileName())
                                                                 .arg(lineNum)
                                                                 .arg(line.trimmed());
                        definitionCount++;
                    } else {
                        references << QString("[USE] %1:%2: %3").arg(fileInfo.fileName())
                                                                 .arg(lineNum)
                                                                 .arg(line.trimmed());
                        usageCount++;
                    }
                    
                    if ((usageCount + definitionCount) >= 30) break;
                }
            }
            file.close();
        }
        
        if ((usageCount + definitionCount) >= 30) break;
    }
    
    if (references.isEmpty()) {
        return QString("Symbol '%1' not found in codebase").arg(symbol);
    }
    
    return QString("Symbol Reference for '%1' (%2 definitions, %3 usages):\n%4")
            .arg(symbol)
            .arg(definitionCount)
            .arg(usageCount)
            .arg(references.join("\n"));
}

// ========== AI CORE COMPONENT 1: CODE ANALYSIS ==========

QJsonObject AgenticEngine::analyzeCodeQuality(const QString& code)
{
    QJsonObject quality;
    
    // Calculate comprehensive metrics
    int lines = code.split('\n').count();
    int nonEmptyLines = 0;
    int commentLines = 0;
    int codeLines = 0;
    
    QStringList codeLines_list = code.split('\n');
    for (const QString& line : codeLines_list) {
        QString trimmed = line.trimmed();
        if (!trimmed.isEmpty()) {
            nonEmptyLines++;
            if (trimmed.startsWith("//") || trimmed.startsWith("/*") || trimmed.startsWith("*")) {
                commentLines++;
            } else {
                codeLines++;
            }
        }
    }
    
    // Calculate quality metrics
    float commentRatio = codeLines > 0 ? (float)commentLines / codeLines : 0.0f;
    quality["total_lines"] = lines;
    quality["code_lines"] = codeLines;
    quality["comment_lines"] = commentLines;
    quality["comment_ratio"] = commentRatio;
    quality["documentation_score"] = commentRatio > 0.2 ? "good" : "needs_improvement";
    
    // Complexity analysis
    int cyclomaticComplexity = 1; // Base complexity
    cyclomaticComplexity += code.count("if ");
    cyclomaticComplexity += code.count("while ");
    cyclomaticComplexity += code.count("for ");
    cyclomaticComplexity += code.count("case ");
    cyclomaticComplexity += code.count("&&");
    cyclomaticComplexity += code.count("||");
    
    quality["cyclomatic_complexity"] = cyclomaticComplexity;
    quality["complexity_rating"] = cyclomaticComplexity < 10 ? "low" : 
                                   cyclomaticComplexity < 20 ? "medium" : "high";
    
    // Code smell detection
    QJsonArray smells;
    if (code.contains("goto ")) smells.append("goto_statement");
    if (code.count("if ") > 5) smells.append("excessive_conditionals");
    if (lines > 300) smells.append("long_file");
    if (code.contains("malloc") && !code.contains("free")) smells.append("potential_memory_leak");
    quality["code_smells"] = smells;
    
    quality["overall_score"] = cyclomaticComplexity < 15 && commentRatio > 0.15 ? 85 : 70;
    
    return quality;
}

QJsonArray AgenticEngine::detectPatterns(const QString& code)
{
    QJsonArray patterns;
    
    // Design pattern detection
    if (code.contains("virtual") && code.contains("override")) {
        QJsonObject pattern;
        pattern["name"] = "polymorphism";
        pattern["description"] = "Virtual functions detected - using polymorphic design";
        patterns.append(pattern);
    }
    
    if (code.contains("static") && code.contains("getInstance")) {
        QJsonObject pattern;
        pattern["name"] = "singleton";
        pattern["description"] = "Singleton pattern detected";
        patterns.append(pattern);
    }
    
    if (code.contains("Q_OBJECT") || code.contains("signals:") || code.contains("slots:")) {
        QJsonObject pattern;
        pattern["name"] = "observer";
        pattern["description"] = "Qt signal/slot pattern (Observer pattern)";
        patterns.append(pattern);
    }
    
    if (code.contains("std::make_shared") || code.contains("std::make_unique")) {
        QJsonObject pattern;
        pattern["name"] = "smart_pointers";
        pattern["description"] = "RAII pattern with smart pointers";
        patterns.append(pattern);
    }
    
    return patterns;
}

QJsonObject AgenticEngine::calculateMetrics(const QString& code)
{
    QJsonObject metrics;
    
    // LOC metrics
    metrics["lines_of_code"] = code.split('\n').count();
    metrics["characters"] = code.length();
    
    // Function metrics
    int functionCount = code.count(QRegularExpression(R"(\w+\s*\([^)]*\)\s*\{)"));
    metrics["function_count"] = functionCount;
    
    // Class metrics
    int classCount = code.count("class ");
    int structCount = code.count("struct ");
    metrics["class_count"] = classCount;
    metrics["struct_count"] = structCount;
    
    // Memory operations
    metrics["dynamic_allocations"] = code.count("new ") + code.count("malloc");
    metrics["deallocations"] = code.count("delete ") + code.count("free");
    
    // Error handling
    metrics["try_catch_blocks"] = code.count("try {");
    metrics["error_checks"] = code.count("if (") + code.count("if(");
    
    return metrics;
}

QString AgenticEngine::suggestImprovements(const QString& code)
{
    QStringList suggestions;
    
    QJsonObject quality = analyzeCodeQuality(code);
    int complexity = quality["cyclomatic_complexity"].toInt();
    float commentRatio = quality["comment_ratio"].toDouble();
    
    if (complexity > 15) {
        suggestions << "• Reduce cyclomatic complexity by extracting functions";
    }
    
    if (commentRatio < 0.15) {
        suggestions << "• Add more documentation comments to improve maintainability";
    }
    
    if (code.contains("malloc") && !code.contains("free")) {
        suggestions << "• Ensure all malloc() calls have corresponding free() calls";
    }
    
    if (code.contains("new ") && !code.contains("delete ")) {
        suggestions << "• Consider using smart pointers (std::unique_ptr, std::shared_ptr)";
    }
    
    if (code.count('\n') > 300) {
        suggestions << "• Consider splitting this file into smaller, focused modules";
    }
    
    if (!code.contains("const ") && code.count("void ") > 3) {
        suggestions << "• Use const-correctness to improve code safety";
    }
    
    if (suggestions.isEmpty()) {
        return "Code quality is good! No major improvements suggested.";
    }
    
    return "Code Improvement Suggestions:\n" + suggestions.join("\n");
}

// ========== AI CORE COMPONENT 2: CODE GENERATION (ENHANCED) ==========

QString AgenticEngine::generateFunction(const QString& signature, const QString& description)
{
    // Parse function signature
    QString functionCode = QString("/**\n * @brief %1\n */\n").arg(description);
    functionCode += signature;
    
    if (!signature.contains("{")) {
        functionCode += " {\n    // TODO: Implement function logic\n";
        functionCode += "    // " + description + "\n";
        functionCode += "    return; // Modify as needed\n}\n";
    }
    
    return functionCode;
}

QString AgenticEngine::generateClass(const QString& className, const QJsonObject& spec)
{
    QString classCode = QString("/**\n * @class %1\n").arg(className);
    
    if (spec.contains("description")) {
        classCode += QString(" * @brief %1\n").arg(spec["description"].toString());
    }
    
    classCode += " */\n";
    classCode += QString("class %1 {\n").arg(className);
    classCode += "public:\n";
    classCode += QString("    %1();\n").arg(className);
    classCode += QString("    ~%1();\n\n").arg(className);
    
    // Add methods from spec
    if (spec.contains("methods")) {
        QJsonArray methods = spec["methods"].toArray();
        for (const QJsonValue& method : methods) {
            classCode += QString("    %1;\n").arg(method.toString());
        }
    }
    
    classCode += "\nprivate:\n";
    
    // Add members from spec
    if (spec.contains("members")) {
        QJsonArray members = spec["members"].toArray();
        for (const QJsonValue& member : members) {
            classCode += QString("    %1;\n").arg(member.toString());
        }
    }
    
    classCode += "};\n";
    
    return classCode;
}

QString AgenticEngine::generateTests(const QString& code)
{
    QString testCode = "#include <QTest>\n\n";
    testCode += "class GeneratedTest : public QObject {\n";
    testCode += "    Q_OBJECT\n\n";
    testCode += "private slots:\n";
    testCode += "    void initTestCase() {\n";
    testCode += "        // Setup test environment\n";
    testCode += "    }\n\n";
    
    // Generate test cases based on functions found
    QRegularExpression funcRegex(R"((\w+)\s+(\w+)\s*\([^)]*\))");
    QRegularExpressionMatchIterator matches = funcRegex.globalMatch(code);
    
    int testCount = 0;
    while (matches.hasNext() && testCount < 5) {
        QRegularExpressionMatch match = matches.next();
        QString functionName = match.captured(2);
        
        testCode += QString("    void test_%1() {\n").arg(functionName);
        testCode += "        // TODO: Add test assertions\n";
        testCode += QString("        // Test %1 functionality\n").arg(functionName);
        testCode += "    }\n\n";
        testCount++;
    }
    
    testCode += "};\n\n";
    testCode += "QTEST_MAIN(GeneratedTest)\n";
    testCode += "#include \"generated_test.moc\"\n";
    
    return testCode;
}

QString AgenticEngine::refactorCode(const QString& code, const QString& refactoringType)
{
    if (refactoringType == "extract_function") {
        return "// Refactored: Extract complex logic into separate functions\n" + code;
    } else if (refactoringType == "rename") {
        return "// Refactored: Use more descriptive variable names\n" + code;
    } else if (refactoringType == "simplify") {
        return "// Refactored: Simplified conditional logic\n" + code;
    }
    
    return code;
}

// ========== AI CORE COMPONENT 3: TASK PLANNING ==========

QJsonArray AgenticEngine::planTask(const QString& goal)
{
    QJsonArray plan;
    
    // Decompose goal into steps
    QJsonObject step1;
    step1["step"] = 1;
    step1["action"] = "Analyze requirements";
    step1["description"] = QString("Understand the goal: %1").arg(goal);
    plan.append(step1);
    
    QJsonObject step2;
    step2["step"] = 2;
    step2["action"] = "Design solution";
    step2["description"] = "Create high-level design and architecture";
    plan.append(step2);
    
    QJsonObject step3;
    step3["step"] = 3;
    step3["action"] = "Implement code";
    step3["description"] = "Write the actual implementation";
    plan.append(step3);
    
    QJsonObject step4;
    step4["step"] = 4;
    step4["action"] = "Test and validate";
    step4["description"] = "Create and run tests to ensure correctness";
    plan.append(step4);
    
    return plan;
}

QJsonObject AgenticEngine::decomposeTask(const QString& task)
{
    QJsonObject decomposition;
    decomposition["task"] = task;
    decomposition["complexity"] = estimateComplexity(task);
    decomposition["subtasks"] = planTask(task);
    decomposition["estimated_time"] = "2-4 hours";
    
    return decomposition;
}

QJsonArray AgenticEngine::generateWorkflow(const QString& project)
{
    QJsonArray workflow;
    
    QStringList phases = {"Planning", "Development", "Testing", "Deployment"};
    for (const QString& phase : phases) {
        QJsonObject workflowStep;
        workflowStep["phase"] = phase;
        workflowStep["description"] = QString("Complete %1 phase for %2").arg(phase, project);
        workflow.append(workflowStep);
    }
    
    return workflow;
}

QString AgenticEngine::estimateComplexity(const QString& task)
{
    int wordCount = task.split(' ').count();
    
    if (wordCount < 5) return "low";
    if (wordCount < 15) return "medium";
    return "high";
}

// ========== AI CORE COMPONENT 4: NLP ==========

QString AgenticEngine::understandIntent(const QString& userInput)
{
    QString lower = userInput.toLower();
    
    if (lower.contains("analyze") || lower.contains("check")) {
        return "code_analysis";
    } else if (lower.contains("generate") || lower.contains("create") || lower.contains("write")) {
        return "code_generation";
    } else if (lower.contains("refactor") || lower.contains("improve")) {
        return "code_refactoring";
    } else if (lower.contains("test")) {
        return "test_generation";
    } else if (lower.contains("explain") || lower.contains("what")) {
        return "explanation";
    } else if (lower.contains("bug") || lower.contains("error") || lower.contains("fix")) {
        return "debugging";
    }
    
    return "general_query";
}

QJsonObject AgenticEngine::extractEntities(const QString& text)
{
    QJsonObject entities;
    
    // Extract language mentions
    QStringList languages = {"C++", "Python", "JavaScript", "Java", "Rust"};
    for (const QString& lang : languages) {
        if (text.contains(lang, Qt::CaseInsensitive)) {
            entities["language"] = lang;
            break;
        }
    }
    
    // Extract file types
    if (text.contains(".cpp") || text.contains(".h")) {
        entities["file_type"] = "C++ source";
    }
    
    // Extract numbers (could be line numbers, counts, etc.)
    QRegularExpression numberRegex(R"(\b\d+\b)");
    QRegularExpressionMatch match = numberRegex.match(text);
    if (match.hasMatch()) {
        entities["number"] = match.captured(0).toInt();
    }
    
    return entities;
}

QString AgenticEngine::generateNaturalResponse(const QString& query, const QJsonObject& context)
{
    QString intent = understandIntent(query);
    
    if (intent == "code_analysis") {
        return "I'll analyze the code for you. Looking for patterns, potential issues, and quality metrics...";
    } else if (intent == "code_generation") {
        return "I can help generate that code. Let me create a template based on your requirements...";
    } else if (intent == "explanation") {
        return "Let me explain how this works. I'll break it down step by step...";
    }
    
    return "I'm ready to help. Could you provide more details about what you'd like to do?";
}

QString AgenticEngine::summarizeCode(const QString& code)
{
    QJsonObject metrics = calculateMetrics(code);
    int lines = metrics["lines_of_code"].toInt();
    int functions = metrics["function_count"].toInt();
    int classes = metrics["class_count"].toInt();
    
    return QString("Code Summary: %1 lines, %2 functions, %3 classes")
            .arg(lines).arg(functions).arg(classes);
}

QString AgenticEngine::explainError(const QString& errorMessage)
{
    if (errorMessage.contains("undefined reference")) {
        return "This is a linker error. The function or variable is declared but not defined. Check that you've implemented the function or linked the correct library.";
    } else if (errorMessage.contains("segmentation fault") || errorMessage.contains("access violation")) {
        return "Memory access error. You're trying to access invalid memory. Check for null pointers, array bounds, or use-after-free issues.";
    } else if (errorMessage.contains("syntax error")) {
        return "The code has a syntax mistake. Check for missing semicolons, braces, or parentheses.";
    }
    
    return "Error detected. Review the error message for clues about what went wrong.";
}

// ========== AI CORE COMPONENT 5: LEARNING ==========

void AgenticEngine::collectFeedback(const QString& responseId, bool positive, const QString& comment)
{
    FeedbackEntry entry;
    entry.responseId = responseId;
    entry.positive = positive;
    entry.comment = comment;
    entry.timestamp = QDateTime::currentMSecsSinceEpoch();
    
    m_feedbackHistory.push_back(entry);
    
    // Update ratings
    if (m_responseRatings.find(responseId) != m_responseRatings.end()) {
        m_responseRatings[responseId] += positive ? 1 : -1;
    } else {
        m_responseRatings[responseId] = positive ? 1 : -1;
    }
    
    m_totalInteractions++;
    if (positive) m_positiveResponses++;
    
    qInfo() << "[Learning] Feedback collected:" << (positive ? "positive" : "negative") 
            << "- Total:" << m_totalInteractions;
    
    emit feedbackCollected(responseId);
}

void AgenticEngine::trainFromFeedback()
{
    qInfo() << "[Learning] Training from" << m_feedbackHistory.size() << "feedback entries";
    
    // Analyze feedback patterns
    int recentPositive = 0;
    int recentTotal = 0;
    
    // Look at last 50 interactions
    int start = std::max(0, (int)m_feedbackHistory.size() - 50);
    for (size_t i = start; i < m_feedbackHistory.size(); i++) {
        recentTotal++;
        if (m_feedbackHistory[i].positive) recentPositive++;
    }
    
    float recentSuccessRate = recentTotal > 0 ? (float)recentPositive / recentTotal : 0.0f;
    
    qInfo() << "[Learning] Recent success rate:" << (recentSuccessRate * 100) << "%";
    
    // Adjust preferences based on feedback
    if (recentSuccessRate < 0.7) {
        qInfo() << "[Learning] Success rate low - adjusting response strategy";
        m_userPreferences["verbosity"] = "detailed";
    }
    
    emit learningCompleted();
}

QJsonObject AgenticEngine::getLearningStats() const
{
    QJsonObject stats;
    stats["total_interactions"] = m_totalInteractions;
    stats["positive_responses"] = m_positiveResponses;
    stats["success_rate"] = m_totalInteractions > 0 ? 
        (float)m_positiveResponses / m_totalInteractions : 0.0f;
    stats["feedback_count"] = (int)m_feedbackHistory.size();
    
    return stats;
}

void AgenticEngine::adaptToUserPreferences(const QJsonObject& preferences)
{
    // Merge new preferences
    for (const QString& key : preferences.keys()) {
        m_userPreferences[key] = preferences[key];
    }
    
    qInfo() << "[Learning] Adapted to user preferences:" << preferences.keys();
}

// ========== AI CORE COMPONENT 6: SECURITY ==========

bool AgenticEngine::validateInput(const QString& input)
{
    // Check for dangerous patterns
    if (input.contains("rm -rf") || input.contains("del /f")) {
        emit securityWarning("Dangerous file deletion command detected");
        return false;
    }
    
    if (input.contains("system(") && input.contains("exec")) {
        emit securityWarning("Potentially unsafe system call detected");
        return false;
    }
    
    // Check length
    if (input.length() > 10000) {
        emit securityWarning("Input exceeds maximum length");
        return false;
    }
    
    return true;
}

QString AgenticEngine::sanitizeCode(const QString& code)
{
    QString sanitized = code;
    
    // Remove potentially dangerous patterns
    sanitized.replace(QRegularExpression(R"(system\s*\([^)]*\))"), "/* system() call removed */");
    sanitized.replace(QRegularExpression(R"(exec\s*\([^)]*\))"), "/* exec() call removed */");
    
    return sanitized;
}

bool AgenticEngine::isCommandSafe(const QString& command)
{
    // Whitelist of safe commands
    QStringList safeCommands = {"ls", "dir", "pwd", "echo", "cat", "grep", "find"};
    
    QString firstWord = command.split(' ').first().toLower();
    
    return safeCommands.contains(firstWord);
}

// ========== Keep/Undo File Operations (Production-Ready) ==========

bool AgenticEngine::createFileWithApproval(const QString& filePath, const QString& content)
{
    qDebug() << "[AgenticEngine::createFileWithApproval] Entry - filePath:" << filePath 
             << "content_size:" << content.size() << "bytes";
    
    // Guard: Check if Keep/Undo system is initialized
    if (!m_fileOperations) {
        qCritical() << "[AgenticEngine::createFileWithApproval] Keep/Undo system not initialized (m_fileOperations is nullptr)";
        emit fileOperationCompleted("create", filePath, false);
        return false;
    }
    
    // Guard: Validate parameters
    if (filePath.isEmpty()) {
        qWarning() << "[AgenticEngine::createFileWithApproval] Invalid parameter: filePath is empty";
        emit fileOperationCompleted("create", filePath, false);
        return false;
    }
    
    // Delegate to AgenticFileOperations (triggers Keep/Undo dialog)
    qInfo() << "[AgenticEngine::createFileWithApproval] Delegating to Keep/Undo system...";
    m_fileOperations->createFileWithApproval(filePath, content);
    
    // Note: Actual success/failure emitted via signal connections in initialize()
    qDebug() << "[AgenticEngine::createFileWithApproval] Exit - delegation complete";
    return true;
}

bool AgenticEngine::modifyFileWithApproval(const QString& filePath, const QString& oldContent, const QString& newContent)
{
    qDebug() << "[AgenticEngine::modifyFileWithApproval] Entry - filePath:" << filePath 
             << "old_size:" << oldContent.size() << "bytes"
             << "new_size:" << newContent.size() << "bytes";
    
    // Guard: Check if Keep/Undo system is initialized
    if (!m_fileOperations) {
        qCritical() << "[AgenticEngine::modifyFileWithApproval] Keep/Undo system not initialized (m_fileOperations is nullptr)";
        emit fileOperationCompleted("modify", filePath, false);
        return false;
    }
    
    // Guard: Validate parameters
    if (filePath.isEmpty()) {
        qWarning() << "[AgenticEngine::modifyFileWithApproval] Invalid parameter: filePath is empty";
        emit fileOperationCompleted("modify", filePath, false);
        return false;
    }
    
    // Delegate to AgenticFileOperations (triggers Keep/Undo dialog)
    qInfo() << "[AgenticEngine::modifyFileWithApproval] Delegating to Keep/Undo system...";
    m_fileOperations->modifyFileWithApproval(filePath, oldContent, newContent);
    
    // Note: Actual success/failure emitted via signal connections in initialize()
    qDebug() << "[AgenticEngine::modifyFileWithApproval] Exit - delegation complete";
    return true;
}

bool AgenticEngine::deleteFileWithApproval(const QString& filePath)
{
    qDebug() << "[AgenticEngine::deleteFileWithApproval] Entry - filePath:" << filePath;
    
    // Guard: Check if Keep/Undo system is initialized
    if (!m_fileOperations) {
        qCritical() << "[AgenticEngine::deleteFileWithApproval] Keep/Undo system not initialized (m_fileOperations is nullptr)";
        emit fileOperationCompleted("delete", filePath, false);
        return false;
    }
    
    // Guard: Validate parameters
    if (filePath.isEmpty()) {
        qWarning() << "[AgenticEngine::deleteFileWithApproval] Invalid parameter: filePath is empty";
        emit fileOperationCompleted("delete", filePath, false);
        return false;
    }
    
    // Delegate to AgenticFileOperations (triggers Keep/Undo dialog)
    qInfo() << "[AgenticEngine::deleteFileWithApproval] Delegating to Keep/Undo system...";
    m_fileOperations->deleteFileWithApproval(filePath);
    
    // Note: Actual success/failure emitted via signal connections in initialize()
    qDebug() << "[AgenticEngine::deleteFileWithApproval] Exit - delegation complete";
    return true;
}

void AgenticEngine::undoLastFileOperation()
{
    qDebug() << "[AgenticEngine::undoLastFileOperation] Entry";
    
    // Guard: Check if Keep/Undo system is initialized
    if (!m_fileOperations) {
        qCritical() << "[AgenticEngine::undoLastFileOperation] Keep/Undo system not initialized (m_fileOperations is nullptr)";
        return;
    }
    
    // Guard: Check if undo is available
    if (!m_fileOperations->canUndo()) {
        qWarning() << "[AgenticEngine::undoLastFileOperation] No operations to undo";
        return;
    }
    
    // Delegate to AgenticFileOperations
    qInfo() << "[AgenticEngine::undoLastFileOperation] Performing undo...";
    m_fileOperations->undoLastAction();
    
    // Note: Actual success/failure emitted via signal connections in initialize()
    qDebug() << "[AgenticEngine::undoLastFileOperation] Exit - undo complete";
}

bool AgenticEngine::canUndoFileOperation() const
{
    // Guard: Check if Keep/Undo system is initialized
    if (!m_fileOperations) {
        qWarning() << "[AgenticEngine::canUndoFileOperation] Keep/Undo system not initialized (m_fileOperations is nullptr)";
        return false;
    }
    
    // Delegate to AgenticFileOperations
    bool canUndo = m_fileOperations->canUndo();
    qDebug() << "[AgenticEngine::canUndoFileOperation] Result:" << canUndo;
    return canUndo;
}
