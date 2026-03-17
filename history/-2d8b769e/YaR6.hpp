#pragma once
#include <QObject>
#include <QByteArray>
#include <QString>
#include "gguf_loader.hpp"

class InferenceEngine : public QObject {
    Q_OBJECT
public:
    explicit InferenceEngine(const QString& ggufPath, QObject* parent = nullptr);
    void request(const QString& prompt, qint64 reqId);

signals:
    void resultReady(qint64 reqId, const QString& answer);
    void error(qint64 reqId, const QString& txt);

private:
    GGUFLoader loader;
};
