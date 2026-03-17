#pragma once
#include <QString>
#include <QVector>
#include <QMap>

struct MemoryRegion {
    quint64 base;
    quint64 size;
    QString permissions;
    QByteArray data;
};

struct RuntimeSymbol {
    QString name;
    quint64 address;
    QString type;
    QByteArray value;
};

class DynamicAnalyzer {
public:
    bool attach(quint64 pid);
    bool launch(const QString &path, const QStringList &args);
    QVector<MemoryRegion> memoryMap() const;
    QVector<RuntimeSymbol> runtimeSymbols() const;
    QString error() const;
private:
    QVector<MemoryRegion> m_memory;
    QVector<RuntimeSymbol> m_symbols;
    QString m_error;
};
