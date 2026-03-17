// Agentic Engine - AI agent coordination with real GGUF inference
#include "agentic_engine.h"
#include "inference_engine.h"
#include <QTimer>
#include <QStringList>
#include <QRandomGenerator>
#include <QDebug>

AgenticEngine::AgenticEngine(QObject* parent) 
    : QObject(parent)
    , m_inferenceEngine(nullptr)
    , m_maxMode(false)
    , m_maxTokens(256)
{}

void AgenticEngine::initialize() {
    qDebug() << "Agentic Engine initialized";
}

void AgenticEngine::setInferenceEngine(InferenceEngine* engine) {
    m_inferenceEngine = engine;
    qDebug() << "Inference engine connected to agentic engine";
}

void AgenticEngine::loadModel(const QString& modelPath) {
    if (!m_inferenceEngine) {
        qWarning() << "No inference engine available";
        emit responseReady("Error: No inference engine configured");
        return;
    }
    
    qDebug() << "Loading model:" << modelPath;
    if (m_inferenceEngine->Initialize(modelPath.toStdString())) {
        emit responseReady("Model loaded successfully: " + modelPath.section('/', -1));
    } else {
        emit responseReady("Failed to load model: " + modelPath);
    }
}

void AgenticEngine::setMaxMode(bool enabled) {
    m_maxMode = enabled;
    m_maxTokens = enabled ? 1024 : 256;
    qDebug() << "Max mode:" << enabled << "| Max tokens:" << m_maxTokens;
}

void AgenticEngine::processMessage(const QString& message) {
    qDebug() << "Processing message:" << message;
    
    // Check if we have a loaded model for real inference
    if (m_inferenceEngine && m_inferenceEngine->isModelLoaded()) {
        // Use real GGUF model inference
        QTimer::singleShot(100, this, [this, message]() {
            QString response = generateRealResponse(message);
            emit responseReady(response);
        });
    } else {
        // No model loaded - provide helpful message
        QTimer::singleShot(100, this, [this, message]() {
            QString response = "[No model loaded] Please select a GGUF model from the dropdown above to enable AI responses.\n\n";
            response += "Your message: \"" + message + "\"";
            emit responseReady(response);
        });
    }
}

QString AgenticEngine::generateRealResponse(const QString& message) {
    if (!m_inferenceEngine || !m_inferenceEngine->isModelLoaded()) {
        return "Error: Model not loaded";
    }
    
    // Tokenize the input
    std::vector<int32_t> inputTokens = m_inferenceEngine->tokenize(message);
    qDebug() << "Input tokenized:" << inputTokens.size() << "tokens";
    
    // Generate response tokens
    std::vector<int32_t> outputTokens = m_inferenceEngine->generate(inputTokens, m_maxTokens);
    qDebug() << "Generated:" << outputTokens.size() << "tokens";
    
    // Detokenize back to text
    QString response = m_inferenceEngine->detokenize(outputTokens);
    
    // If response is empty or just the input, provide feedback
    if (response.trimmed().isEmpty() || response == message) {
        response = "[Model Output] " + response;
        response += "\n\n(Model: " + QString::fromStdString(m_inferenceEngine->modelPath()).section('/', -1) + ")";
    }
    
    return response;
}

QString AgenticEngine::analyzeCode(const QString& code) {
    if (m_inferenceEngine && m_inferenceEngine->isModelLoaded()) {
        QString prompt = "Analyze the following code:\n" + code;
        return generateRealResponse(prompt);
    }
    return "[No model] Please load a model to analyze code.";
}

QString AgenticEngine::generateCode(const QString& prompt) {
    if (m_inferenceEngine && m_inferenceEngine->isModelLoaded()) {
        QString fullPrompt = "Generate code for: " + prompt;
        return generateRealResponse(fullPrompt);
    }
    return "[No model] Please load a model to generate code.";
}

QString AgenticEngine::generateResponse(const QString& message) {
    // Fallback for when no model is loaded
    return "Please load a GGUF model to get AI responses. Select one from the model dropdown above.";
}
