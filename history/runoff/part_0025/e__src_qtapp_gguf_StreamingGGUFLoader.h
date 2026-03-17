#pragma once
#include <QObject>
#include <QString>
#include <QByteArray>
#include <QFile>
#include <QHash>
#include <vector>

struct TensorMetadata {
    QString name;
    uint32_t ndims = 0;
    uint32_t ggml_type = 0;
    quint64 absolute_offset = 0;
    quint64 size_bytes = 0;
    QString zone_id;
};

struct ZoneMemory {
    quint64 start_offset_in_file = 0;
    quint64 size_bytes = 0;
    uchar* mapped_data = nullptr;
};

class StreamingGGUFLoader : public QObject {
    Q_OBJECT
public:
    explicit StreamingGGUFLoader(QObject* parent = nullptr);

    bool Open(const QString& filePath);
    bool BuildTensorIndex();
    void Close();

    bool LoadZone(const QString& zoneName);
    void UnloadZone(const QString& zoneName);
    void UnloadAll();

    bool GetTensorData(const QString& tensorName, std::vector<uint8_t>& outData);

    QString getModelName() const;
    qint64 getTotalSize() const;

signals:
    void ZoneLoaded(const QString& zoneName);
    void ZoneEvicted(const QString& zoneName);

private:
    QByteArray readDataFromFile(qint64 offset, qint64 size);

    QFile file_;
    QString modelName_;
    qint64 totalSize_ = 0;
    QHash<QString, TensorMetadata> tensorIndex_;
    QHash<QString, ZoneMemory> loadedZones_;
};
