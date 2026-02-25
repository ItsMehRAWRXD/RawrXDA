#pragma once
#include <QString>
#include <QVariant>
#include <QByteArray>
#include <QHash>
#include <QStringList>
#include <memory>

// Qt wrapper for basic GGUF file parsing and tensor discovery
class GGUFLoader {
public:
    explicit GGUFLoader(const QString& path);
    ~GGUFLoader();

    bool isOpen() const;
    QVariant getParam(const QString& key, const QVariant& defaultValue) const;
    QByteArray inflateWeight(const QString& tensorName);
    QHash<QString, QByteArray> getTokenizerMetadata() const;
    QStringList tensorNames() const;

private:
    QString m_path;
    mutable QHash<QString, QVariant> m_metadataCache;
    mutable QStringList m_cachedTensorNames;
    bool m_initialized{false};
    
    void initializeNativeLoader();
};



