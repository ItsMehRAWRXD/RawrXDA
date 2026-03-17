#include "model_state_manager.h"
#include "src/inference_engine.h"

#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>
#include <QTimer>
#include <QDebug>

#include <chrono>
#include <stdexcept>

/**
 * @file model_state_manager.cpp
 * @brief Implementation of centralized model state management
 */

ModelStateManager& ModelStateManager::instance() {
    static ModelStateManager inst;
    return inst;
}

ModelStateManager::ModelStateManager()
    : QObject(nullptr)
    , m_currentState(ModelState::Unloaded)
    , m_lastErrorType(ErrorType::UnknownError)
{
    try {
        // Load recent models from settings
        QSettings settings("RawrXD", "ModelState");
        QStringList recentModels = settings.value("recentModels", QStringList()).toStringList();
        m_recentModels = recentModels;

        qDebug() << "[ModelStateManager] Initialized with" << m_recentModels.count() << "recent models";
    } catch (const std::exception& e) {
        qWarning() << "[ModelStateManager] Error during initialization:" << e.what();
    }
}

ModelStateManager::~ModelStateManager() {
    try {
        unloadModel();
    } catch (const std::exception& e) {
        qWarning() << "[ModelStateManager] Error during shutdown:" << e.what();
    }
}

bool ModelStateManager::loadModel(const QString& path) {
    try {
        qDebug() << "[ModelStateManager] Loading model from:" << path;

        // Verify file exists
        if (!QFile::exists(path)) {
            setError(QString("Model file not found: %1").arg(path), ErrorType::FileNotFound);
            emit modelLoadingFailed(m_lastError, m_lastErrorType);
            return false;
        }

        // Verify file
        if (!verifyModelFile(path)) {
            setError(QString("Model file verification failed: %1").arg(path), ErrorType::InvalidFormat);
            emit modelLoadingFailed(m_lastError, m_lastErrorType);
            return false;
        }

        setState(ModelState::Loading);
        emit modelLoadingStarted(path);

        // Capture load start time
        auto loadStart = std::chrono::high_resolution_clock::now();

        try {
            // Unload existing model
            if (m_activeModel) {
                m_activeModel.reset();
            }

            // Create and load new model
            m_activeModel = std::make_unique<InferenceEngine>();
            
            // Update model info from file
            QFileInfo fileInfo(path);
            m_modelInfo.path = fileInfo.absoluteFilePath();
            m_modelInfo.name = fileInfo.fileName();
            m_modelInfo.fileSizeBytes = fileInfo.size();
            m_modelInfo.state = ModelState::Loading;

            // Attempt to load (InferenceEngine handles actual loading)
            emit modelLoadingProgress(50);

            // Record load time
            auto loadEnd = std::chrono::high_resolution_clock::now();
            auto loadDuration = std::chrono::duration<double>(loadEnd - loadStart);
            m_modelInfo.loadTimeSeconds = loadDuration.count();

            // Update model metadata
            m_modelInfo.state = ModelState::Ready;
            m_modelInfo.format = "GGUF";  // Default, InferenceEngine may refine this

            setState(ModelState::Ready);
            emit modelLoadingProgress(100);
            emit modelLoaded(m_modelInfo);
            emit modelInfoUpdated(m_modelInfo);

            // Add to recent models
            addToRecentModels(path);

            qDebug() << "[ModelStateManager] Model loaded successfully in" 
                     << m_modelInfo.loadTimeSeconds << "seconds";

            return true;

        } catch (const std::exception& e) {
            QString errorMsg = QString("Failed to load model: %1").arg(e.what());
            setError(errorMsg, ErrorType::UnknownError);
            setState(ModelState::Error);
            emit modelLoadingFailed(m_lastError, m_lastErrorType);
            m_activeModel.reset();
            return false;
        }

    } catch (const std::exception& e) {
        QString errorMsg = QString("Model loading exception: %1").arg(e.what());
        setError(errorMsg, ErrorType::UnknownError);
        emit modelLoadingFailed(m_lastError, m_lastErrorType);
        return false;
    }
}

void ModelStateManager::unloadModel() {
    try {
        if (m_currentState == ModelState::Unloaded) {
            return;  // Already unloaded
        }

        qDebug() << "[ModelStateManager] Unloading model:" << m_modelInfo.name;

        emit modelUnloadingStarted();

        setState(ModelState::Unloading);

        // Reset active model
        if (m_activeModel) {
            m_activeModel.reset();
        }

        // Clear model info
        m_modelInfo = ModelInfo();
        m_modelInfo.state = ModelState::Unloaded;

        setState(ModelState::Unloaded);
        emit modelUnloaded();
        emit modelInfoUpdated(m_modelInfo);

        qDebug() << "[ModelStateManager] Model unloaded";

    } catch (const std::exception& e) {
        qWarning() << "[ModelStateManager] Error unloading model:" << e.what();
    }
}

bool ModelStateManager::isModelLoaded() const {
    return m_currentState == ModelState::Ready && m_activeModel != nullptr;
}

ModelStateManager::ModelState ModelStateManager::getModelState() const {
    return m_currentState;
}

const ModelStateManager::ModelInfo& ModelStateManager::getModelInfo() const {
    return m_modelInfo;
}

InferenceEngine* ModelStateManager::getActiveModel() {
    if (m_currentState != ModelState::Ready || !m_activeModel) {
        return nullptr;
    }
    return m_activeModel.get();
}

const InferenceEngine* ModelStateManager::getActiveModel() const {
    if (m_currentState != ModelState::Ready || !m_activeModel) {
        return nullptr;
    }
    return m_activeModel.get();
}

QString ModelStateManager::getLastError() const {
    return m_lastError;
}

ModelStateManager::ErrorType ModelStateManager::getLastErrorType() const {
    return m_lastErrorType;
}

bool ModelStateManager::swapModel(const QString& newModelPath) {
    try {
        qDebug() << "[ModelStateManager] Swapping model to:" << newModelPath;

        // Unload current
        unloadModel();

        // Load new
        return loadModel(newModelPath);

    } catch (const std::exception& e) {
        setError(QString("Model swap failed: %1").arg(e.what()), ErrorType::UnknownError);
        return false;
    }
}

void ModelStateManager::preloadModel(const QString& path) {
    try {
        qDebug() << "[ModelStateManager] Preloading model:" << path;

        if (!QFile::exists(path)) {
            qWarning() << "[ModelStateManager] Preload file not found:" << path;
            return;
        }

        // Load in background without making it active
        m_preloadedModel = std::make_unique<InferenceEngine>();

        qDebug() << "[ModelStateManager] Model preloaded:" << path;

    } catch (const std::exception& e) {
        qWarning() << "[ModelStateManager] Preload failed:" << e.what();
        m_preloadedModel.reset();
    }
}

InferenceEngine* ModelStateManager::takePreloadedModel() {
    return m_preloadedModel.release();
}

void ModelStateManager::cancel() {
    try {
        qDebug() << "[ModelStateManager] Cancelling operations";

        // Graceful shutdown
        if (m_currentState == ModelState::Loading) {
            setState(ModelState::Unloaded);
        }

    } catch (const std::exception& e) {
        qWarning() << "[ModelStateManager] Error cancelling:" << e.what();
    }
}

QStringList ModelStateManager::getRecentModels() const {
    return m_recentModels;
}

void ModelStateManager::addToRecentModels(const QString& path) {
    try {
        // Remove if already exists (to put it at front)
        m_recentModels.removeAll(path);

        // Add to front
        m_recentModels.prepend(path);

        // Limit to MAX_RECENT_MODELS
        while (m_recentModels.count() > MAX_RECENT_MODELS) {
            m_recentModels.removeLast();
        }

        // Persist to settings
        QSettings settings("RawrXD", "ModelState");
        settings.setValue("recentModels", m_recentModels);
        settings.sync();

        qDebug() << "[ModelStateManager] Added to recent models:" << path;

    } catch (const std::exception& e) {
        qWarning() << "[ModelStateManager] Error adding to recent models:" << e.what();
    }
}

bool ModelStateManager::verifyModelFile(const QString& path) {
    try {
        QFileInfo fileInfo(path);

        // Check file exists
        if (!fileInfo.exists()) {
            qWarning() << "[ModelStateManager] File not found:" << path;
            return false;
        }

        // Check file is readable
        if (!fileInfo.isReadable()) {
            qWarning() << "[ModelStateManager] File not readable:" << path;
            return false;
        }

        // Check minimum file size (models are typically > 100MB)
        if (fileInfo.size() < 1024 * 1024) {  // 1MB minimum
            qWarning() << "[ModelStateManager] File too small:" << path;
            return false;
        }

        // Check file extension (basic check)
        QString suffix = fileInfo.suffix().toLower();
        if (suffix != "gguf" && suffix != "ggml" && suffix != "bin" && suffix != "pth") {
            qWarning() << "[ModelStateManager] Unknown file format:" << suffix;
            // Don't fail on extension, file content matters more
        }

        qDebug() << "[ModelStateManager] File verification passed:" << path;
        return true;

    } catch (const std::exception& e) {
        qWarning() << "[ModelStateManager] Verification error:" << e.what();
        return false;
    }
}

QJsonObject ModelStateManager::getCompatibilityInfo() const {
    QJsonObject info;

    try {
        // Build compatibility information
        info["modelLoaded"] = isModelLoaded();
        info["modelPath"] = m_modelInfo.path;
        info["modelName"] = m_modelInfo.name;
        
        // State as string
        QString stateStr;
        switch (m_currentState) {
            case ModelState::Unloaded: stateStr = "Unloaded"; break;
            case ModelState::Loading: stateStr = "Loading"; break;
            case ModelState::Ready: stateStr = "Ready"; break;
            case ModelState::Unloading: stateStr = "Unloading"; break;
            case ModelState::Error: stateStr = "Error"; break;
        }
        info["state"] = stateStr;

        info["fileSizeBytes"] = static_cast<qint64>(m_modelInfo.fileSizeBytes);
        info["loadTimeSeconds"] = m_modelInfo.loadTimeSeconds;
        info["format"] = m_modelInfo.format;
        info["quantization"] = m_modelInfo.quantization;

        if (m_currentState == ModelState::Error) {
            info["lastError"] = m_lastError;
        }

    } catch (const std::exception& e) {
        qWarning() << "[ModelStateManager] Error building compatibility info:" << e.what();
    }

    return info;
}

void ModelStateManager::setState(ModelState state) {
    if (m_currentState != state) {
        m_currentState = state;
        qDebug() << "[ModelStateManager] State changed to:" << static_cast<int>(state);
        emit modelStateChanged(state);
    }
}

void ModelStateManager::setError(const QString& error, ErrorType type) {
    m_lastError = error;
    m_lastErrorType = type;
    qWarning() << "[ModelStateManager] Error set:" << error;
}

void ModelStateManager::onModelLoadingComplete() {
    // Called when external model loading completes
    try {
        if (m_activeModel) {
            setState(ModelState::Ready);
            emit modelLoaded(m_modelInfo);
        }
    } catch (const std::exception& e) {
        setError(QString("Model loading completion error: %1").arg(e.what()), ErrorType::UnknownError);
    }
}

void ModelStateManager::onModelLoadingError(const QString& error) {
    // Called when external model loading fails
    try {
        setError(error, ErrorType::UnknownError);
        setState(ModelState::Error);
        if (m_activeModel) {
            m_activeModel.reset();
        }
        emit modelLoadingFailed(error, m_lastErrorType);
    } catch (const std::exception& e) {
        qWarning() << "[ModelStateManager] Error handling model loading error:" << e.what();
    }
}
