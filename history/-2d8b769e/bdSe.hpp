#pragma once
#include <QObject>
#include <QByteArray>
#include <QString>
#include <QMutex>
#include <QElapsedTimer>
#include <vector>
#include <cstdint>
#include "gguf_loader.hpp"
#include "transformer_inference.hpp"

class InferenceEngine : public QObject {
    Q_OBJECT
public:
    explicit InferenceEngine(const QString& ggufPath = QString(), QObject* parent = nullptr);
    
    Q_INVOKABLE bool loadModel(const QString& path);
    bool isModelLoaded() const;
    QString modelPath() const;
    QStringList tensorNames() const;
    
    // Performance metrics
    qint64 memoryUsageMB() const;
    double tokensPerSecond() const;
    double temperature() const;

public slots:
    void request(const QString& prompt, qint64 reqId);
    void unloadModel();
    void setQuantMode(const QString& mode);
    void setLayerQuant(const QString& tensorName, const QString& quant);

signals:
    void resultReady(qint64 reqId, const QString& answer);
    void error(qint64 reqId, const QString& errorMsg);
    void modelLoadedChanged(bool loaded, const QString& modelName);
    void streamToken(qint64 reqId, const QString& token);
    void streamFinished(qint64 reqId);
    void quantChanged(const QString& mode);

private:
    QString m_modelPath;
    GGUFLoader* m_loader;
    mutable QMutex m_mutex;
    QString m_quantMode{"Q4_0"};
    QHash<QString, QString> m_perLayerQuant;
    QHash<QString, QByteArray> m_tensorCache;
    TransformerInference m_transformer;
    
    // Performance tracking
    qint64 m_memoryUsageMB{0};
    double m_tokensPerSecond{0.0};
    double m_temperature{0.8};
    QElapsedTimer m_inferenceTimer;
    
    QString extractModelName(const QString& path) const;
    void rebuildTensorCache();
    std::vector<int32_t> tokenize(const QString& text) const;
    QString detokenize(const std::vector<int32_t>& tokens) const;
};
