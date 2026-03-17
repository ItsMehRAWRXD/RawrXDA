#include "QtStreamingLoader.h"
#include "StreamingGGUFLoader.h"
#include <QString>
#include <QByteArray>

static StreamingGGUFLoader* toCore(void* p) { return reinterpret_cast<StreamingGGUFLoader*>(p); }

QtStreamingLoader::QtStreamingLoader(QObject* parent)
    : QObject(parent) {}

bool QtStreamingLoader::open(const QString& filepath) {
    try {
        auto* core = new StreamingGGUFLoader();
        if (!core->Open(filepath)) {
            delete core;
            emit error(QString::fromLatin1("Failed to open GGUF: %1").arg(filepath));
            return false;
        }
        m_loader = core;
        emit opened(filepath, static_cast<quint64>(core->getTotalSize()));
        return true;
    } catch (...) {
        emit error("Exception opening GGUF file");
        return false;
    }
}

void QtStreamingLoader::close() {
    auto* core = toCore(m_loader);
    if (core) {
        core->Close();
        delete core;
        m_loader = nullptr;
        emit closed();
    }
}

bool QtStreamingLoader::loadZone(const QString& zoneName, quint64 maxMemoryMB) {
    auto* core = toCore(m_loader);
    if (!core) { emit error("Loader not open"); return false; }
    m_maxZoneMB = maxMemoryMB;
    // Core API takes only the zone name (QString)
    if (!core->LoadZone(zoneName)) {
        emit error(QString::fromLatin1("Failed to load zone: %1").arg(zoneName));
        return false;
    }
    // We cannot easily get exact bytes here without exposing zone info; emit nominal stats.
    emit zoneLoaded(zoneName, 0);
    // Core header does not expose memory usage API; skip memoryStats here
    return true;
}

bool QtStreamingLoader::unloadZone(const QString& zoneName) {
    auto* core = toCore(m_loader);
    if (!core) { emit error("Loader not open"); return false; }
    // Core API returns void; attempt unload and consider success
    core->UnloadZone(zoneName);
    emit zoneUnloaded(zoneName);
    // No memory usage API present
    return true;
}

bool QtStreamingLoader::getTensorData(const QString& tensorName, QByteArray& outData) {
    auto* core = toCore(m_loader);
    if (!core) { emit error("Loader not open"); return false; }
    std::vector<uint8_t> buf;
    if (!core->GetTensorData(tensorName, buf)) {
        emit error(QString::fromLatin1("Failed to get tensor: %1").arg(tensorName));
        return false;
    }
    outData = QByteArray(reinterpret_cast<const char*>(buf.data()), static_cast<int>(buf.size()));
    emit tensorChunk(tensorName, outData);
    // No memory usage API present
    return true;
}
