#include "multimodal_engine.h"
#include <QImage>
#include <QFile>
#include <QFileInfo>
#include <QBuffer>
#include <QImageReader>
#include <QImageWriter>
#include <QDebug>

MultiModalEngine::MultiModalEngine(QObject *parent)
    : QObject(parent)
{
}

void MultiModalEngine::processImage(const QImage &image)
{
    QImage scaledImage = image;
    if (image.width() > 1024 || image.height() > 1024) {
        scaledImage = image.scaled(1024, 1024, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    QByteArray ba;
    QBuffer buffer(&ba);
    buffer.open(QIODevice::WriteOnly);
    scaledImage.save(&buffer, "PNG");
    QString base64 = QString::fromLatin1(ba.toBase64().data());
    emit visionPromptReady(base64, "image/png");
}

void MultiModalEngine::processImage(const QString &filePath)
{
    QImage image(filePath);
    if (image.isNull()) {
        qWarning() << "Failed to load image from file path:" << filePath;
        return;
    }
    processImage(image);
}