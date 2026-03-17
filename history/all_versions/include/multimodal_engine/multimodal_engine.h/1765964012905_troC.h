#ifndef MULTIMODAL_ENGINE_H
#define MULTIMODAL_ENGINE_H

#include <QObject>
#include <QImage>
#include <QString>

class MultiModalEngine : public QObject
{
    Q_OBJECT

public:
    explicit MultiModalEngine(QObject *parent = nullptr);

    // Accepts QImage or file path → base-64 PNG/JPG
    // Auto-detects MIME, resizes > 1024 px longest edge (bicubic)
    // Emits visionPromptReady(QString base64, QString mime)
    void processImage(const QImage &image);
    void processImage(const QString &filePath);

signals:
    void visionPromptReady(const QString &base64, const QString &mime);
};

#endif // MULTIMODAL_ENGINE_H