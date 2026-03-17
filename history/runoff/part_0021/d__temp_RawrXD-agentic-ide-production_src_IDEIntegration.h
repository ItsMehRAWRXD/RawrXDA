#ifndef IDEINTEGRATION_H
#define IDEINTEGRATION_H

#include <QObject>
#include <QString>
#include <QMutex>
#include "ModelLoaderBridge.h"
#include "InferenceSession.h"
#include "TokenStreamRouter.h"
#include "PerformanceMonitor.h"

class IDEIntegration : public QObject {
    Q_OBJECT

public:
    static IDEIntegration* getInstance();
    
    // System initialization
    bool initialize();
    void shutdown();
    
    // Model management
    void loadModel(const QString& path);
    void unloadModel(const QString& modelName);
    QVector<ModelInfo> getLoadedModels() const;
    
    // Inference management
    InferenceSession* createInferenceSession(const QString& modelName);
    void deleteInferenceSession(InferenceSession* session);
    
    // Token streaming
    TokenStreamRouter* getTokenRouter() const;
    
    // Performance monitoring
    PerformanceMonitor* getPerformanceMonitor() const;
    
    // Query system status
    bool isInitialized() const;
    QString getSystemStatus() const;

signals:
    void systemInitialized();
    void systemShutdown();
    void modelLoaded(const QString& name);
    void modelUnloaded(const QString& name);
    void inferenceStarted(const QString& modelName);
    void inferenceCompleted(const QString& modelName);

private:
    IDEIntegration();
    ~IDEIntegration();
    
    static IDEIntegration* s_instance;
    static QMutex s_mutex;
    
    ModelLoaderBridge* m_loader;
    TokenStreamRouter* m_tokenRouter;
    PerformanceMonitor* m_performanceMonitor;
    QVector<InferenceSession*> m_activeSessions;
    bool m_initialized = false;
    
    Q_DISABLE_COPY(IDEIntegration)
};

#endif // IDEINTEGRATION_H
