#pragma once
#include <QString>
#include <QByteArray>
#include <QVector>
#include <QMap>

struct BinarySection {
    QString name;
    quint64 address;
    quint64 size;
    QByteArray data;
};

struct BinarySymbol {
    QString name;
    quint64 address;
    QString type;
};

class BinaryLoader {
public:
    bool load(const QString &path);
    QVector<BinarySection> sections() const;
    QVector<BinarySymbol> symbols() const;
    QString format() const; // "PE", "ELF", "Mach-O"
    QString error() const;
private:
    QVector<BinarySection> m_sections;
    QVector<BinarySymbol> m_symbols;
    QString m_format;
    QString m_error;
};
