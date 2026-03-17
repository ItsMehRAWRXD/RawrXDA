#ifndef COMPRESSION_INTERFACE_H
#define COMPRESSION_INTERFACE_H

#include <vector>
#include <cstdint>
#include <QByteArray>
#include <QDebug>
#include <QObject>
#include <QString>
#include <memory>
#include <chrono>

// Stub compression statistics structure
struct CompressionStats {
    qint64 originalSize = 0;
    qint64 compressedSize = 0;
    double compressionRatio = 0.0;
    qint64 timeMs = 0;
    QString algorithm;
};

// Interface for compression providers (stub)
class ICompressionProvider {
public:
    virtual ~ICompressionProvider() = default;
    virtual QByteArray compress(const QByteArray& data) = 0;
    virtual QByteArray decompress(const QByteArray& data) = 0;
    virtual CompressionStats getStats() const = 0;
};

// Stub: all functions return uncompressed/success for now

struct CompressionConfig {
    uint32_t dict_size = 0;
    bool enable_dictionary = false;
    size_t block_size = 65536;
    int thread_count = 1;
    bool enable_simd = false;
    int level = 6;
};

class EnhancedBrutalGzipWrapper : public QObject {
    Q_OBJECT
public:
    explicit EnhancedBrutalGzipWrapper(QObject* parent = nullptr) : QObject(parent) {}

signals:
    void compressionProgress(int progress);

public:
    bool CompressWithDict(const std::vector<uint8_t>&,
                          std::vector<uint8_t>& out,
                          const std::vector<uint8_t>&) {
        qDebug() << "STUB: CompressWithDict - returning false";
        Q_EMIT compressionProgress(0);
        return false;
    }

    std::vector<uint8_t> CompressParallel(const std::vector<uint8_t>& raw,
                                          const CompressionConfig&) {
        qDebug() << "STUB: CompressParallel - passing through uncompressed";
        Q_EMIT compressionProgress(0);
        return raw;
    }

    QByteArray CompressWithAVX512(const QByteArray& input, int) {
        qDebug() << "STUB: CompressWithAVX512 - passing through";
        Q_EMIT compressionProgress(0);
        return input;
    }

    QByteArray CompressWithAVX2(const QByteArray& input, int) {
        qDebug() << "STUB: CompressWithAVX2 - passing through";
        Q_EMIT compressionProgress(0);
        return input;
    }

private:
    CompressionConfig config_;
    bool has_avx2_ = false;
};

// Factory for creating compression providers (stub implementation)
class CompressionFactory {
public:
    static std::shared_ptr<ICompressionProvider> Create(unsigned int type = 0);
};

#endif // COMPRESSION_INTERFACE_H
