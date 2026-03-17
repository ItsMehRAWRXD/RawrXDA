#pragma once

#include <QObject>
#include <QString>

class InferenceEngine;

class AgenticEngine : public QObject {
    Q_OBJECT
public:
    explicit AgenticEngine(QObject* parent = nullptr);
    virtual ~AgenticEngine() = default;
    
    void initialize();
    void setInferenceEngine(InferenceEngine* engine);
    void processMessage(const QString& message);
    QString analyzeCode(const QString& code);
    QString generateCode(const QString& prompt);
    
public slots:
    void loadModel(const QString& modelPath);
    void setMaxMode(bool enabled);
    
signals:
    void responseReady(const QString& response);
    
private:
    QString generateResponse(const QString& message);
    QString generateRealResponse(const QString& message);
    
    InferenceEngine* m_inferenceEngine;
    bool m_maxMode;
    int m_maxTokens;
};
