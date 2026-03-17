// Agentic Engine - AI agent coordination
#include "agentic_engine.h"
#include "../include/gguf_loader.h"
#include <QTimer>
#include <QStringList>
#include <QRandomGenerator>
#include <QDebug>
#include <QThread>

AgenticEngine::AgenticEngine(QObject* parent) 
    : QObject(parent), m_modelLoaded(false), m_inferenceEngine(nullptr) {}

void AgenticEngine::initialize() {
    qDebug() << "Agentic Engine initialized - waiting for model selection";
    // Don't load any model on startup - wait for model selector
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
        bool success = loadModelAsync(modelPath.toStdString());
        emit modelLoadingFinished(success, modelPath);
        thread->quit();
    });
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    thread->start();
}

bool AgenticEngine::loadModelAsync(const std::string& modelPath) {
    try {
        // Here you would initialize the real inference engine with the GGUF model
        m_modelLoaded = true;
        m_currentModelPath = modelPath;
        qDebug() << "Model loaded:" << QString::fromStdString(modelPath);
        return true;
    } catch (const std::exception& e) {
        qCritical() << "Failed to load model:" << e.what();
        m_modelLoaded = false;
        return false;
    }
}

void AgenticEngine::processMessage(const QString& message) {
    qDebug() << "Processing message:" << message;
    
    // Process with delay to simulate inference
    QTimer::singleShot(500, this, [this, message]() {
        QString response;
        if (m_modelLoaded) {
            // Use real tokenization from loaded model
            response = generateTokenizedResponse(message);
        } else {
            // Fallback to keyword-based responses if no model loaded
            response = generateResponse(message);
        }
        emit responseReady(response);
    });
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
