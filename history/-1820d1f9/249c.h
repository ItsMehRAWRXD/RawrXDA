#pragma once

#include <QObject>
#include <QString>
#include <vector>
#include <string>

class AgenticEngine : public QObject {
    Q_OBJECT
public:
    explicit AgenticEngine(QObject* parent = nullptr);
    virtual ~AgenticEngine() = default;
    
    void initialize();
    void processMessage(const QString& message);
    QString analyzeCode(const QString& code);
    QString generateCode(const QString& prompt);
    
    // Model management methods
    bool isModelLoaded() const { return m_modelLoaded; }
    QString currentModelPath() const { return QString::fromStdString(m_currentModelPath); }
    
public slots:
    void setModel(const QString& modelPath);
    
signals:
    void responseReady(const QString& response);
    void modelLoadingFinished(bool success, const QString& modelPath);
    
private:
    QString generateResponse(const QString& message);
    QString generateTokenizedResponse(const QString& message);
    bool loadModelAsync(const std::string& modelPath);
    
    // Model state
    bool m_modelLoaded = false;
    std::string m_currentModelPath;
    void* m_inferenceEngine = nullptr;
};
