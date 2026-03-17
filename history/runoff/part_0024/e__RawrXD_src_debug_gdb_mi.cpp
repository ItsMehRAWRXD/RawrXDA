#include "gdb_mi.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QDebug>

QJsonObject GdbMI::parseOutputRecord(const QString &record)
{
    // This is a simplified parser. A full implementation would be more complex.
    QJsonObject result;
    if (record.startsWith("^")) {
        result = parseResultRecord(record);
    } else if (record.startsWith("*") || record.startsWith("+") || record.startsWith("=")) {
        result = parseAsyncRecord(record);
    }
    return result;
}

QJsonObject GdbMI::parseResultRecord(const QString &record)
{
    QJsonObject result;
    int index = 1; // Skip the '^'
    // Find the first comma or end of string to get the result class
    int commaIndex = record.indexOf(',', index);
    if (commaIndex == -1) {
        commaIndex = record.length();
    }
    QString resultClass = record.mid(index, commaIndex - index);
    result["result-class"] = resultClass;
    index = commaIndex + 1;

    // Parse results
    QJsonArray results;
    while (index < record.length()) {
        QJsonObject res = parseResult(record, index);
        if (!res.isEmpty()) {
            results.append(res);
        }
        // Skip comma
        if (index < record.length() && record[index] == ',') {
            index++;
        }
    }
    result["results"] = results;
    return result;
}

QJsonObject GdbMI::parseAsyncRecord(const QString &record)
{
    QJsonObject result;
    // This is a simplified parser. A full implementation would be more complex.
    // For now, we'll just return an empty object.
    Q_UNUSED(record)
    return result;
}

QString GdbMI::parseCString(const QString &str, int &index)
{
    // This is a simplified parser. A full implementation would be more complex.
    // For now, we'll just return a dummy string.
    Q_UNUSED(str)
    Q_UNUSED(index)
    return QString();
}

QJsonArray GdbMI::parseTuple(const QString &str, int &index)
{
    // This is a simplified parser. A full implementation would be more complex.
    // For now, we'll just return an empty array.
    Q_UNUSED(str)
    Q_UNUSED(index)
    return QJsonArray();
}

QJsonArray GdbMI::parseList(const QString &str, int &index)
{
    // This is a simplified parser. A full implementation would be more complex.
    // For now, we'll just return an empty array.
    Q_UNUSED(str)
    Q_UNUSED(index)
    return QJsonArray();
}

QJsonObject GdbMI::parseResult(const QString &str, int &index)
{
    QJsonObject result;
    // Find the '=' to separate the variable from the value
    int equalIndex = str.indexOf('=', index);
    if (equalIndex == -1) {
        // No '=' found, invalid result
        index = str.length(); // Skip to end
        return result;
    }

    QString variable = str.mid(index, equalIndex - index).trimmed();
    index = equalIndex + 1;

    // Determine the type of value (c-string, tuple, list)
    if (index < str.length()) {
        QJsonValue value;
        if (str[index] == '"') {
            // c-string
            QString cstr = parseCString(str, index);
            value = cstr;
        } else if (str[index] == '{') {
            // tuple
            QJsonArray tuple = parseTuple(str, index);
            value = tuple;
        } else if (str[index] == '[') {
            // list
            QJsonArray list = parseList(str, index);
            value = list;
        } else {
            // Simple value (e.g., a number or identifier)
            int nextComma = str.indexOf(',', index);
            int nextBrace = str.indexOf('}', index);
            int nextBracket = str.indexOf(']', index);
            int endIndex = str.length();
            if (nextComma != -1) endIndex = qMin(endIndex, nextComma);
            if (nextBrace != -1) endIndex = qMin(endIndex, nextBrace);
            if (nextBracket != -1) endIndex = qMin(endIndex, nextBracket);
            QString simpleValue = str.mid(index, endIndex - index).trimmed();
            value = simpleValue;
            index = endIndex;
        }
        result[variable] = value;
    }

    return result;
}