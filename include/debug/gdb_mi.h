#ifndef GDB_MI_H
#define GDB_MI_H

#include <QString>
#include <QJsonObject>
#include <QJsonArray>

// Minimal GDB/MI parser
class GdbMI
{
public:
    // Parse a GDB/MI output record
    static QJsonObject parseOutputRecord(const QString &record);

    // Parse a GDB/MI result record
    static QJsonObject parseResultRecord(const QString &record);

    // Parse a GDB/MI async record
    static QJsonObject parseAsyncRecord(const QString &record);

private:
    // Helper functions for parsing
    static QString parseCString(const QString &str, int &index);
    static QJsonArray parseTuple(const QString &str, int &index);
    static QJsonArray parseList(const QString &str, int &index);
    static QJsonObject parseResult(const QString &str, int &index);
};

#endif // GDB_MI_H