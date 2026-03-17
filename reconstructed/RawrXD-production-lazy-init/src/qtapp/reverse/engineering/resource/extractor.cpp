/**
 * \file resource_extractor.cpp
 * \brief Implementation of resource extractor
 * \author RawrXD Team
 * \date 2026-01-22
 */

#include "resource_extractor.h"
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <cctype>

using namespace RawrXD::ReverseEngineering;

ResourceExtractor::ResourceExtractor(const BinaryLoader& loader)
    : m_loader(loader) {
}

QVector<StringInfo> ResourceExtractor::extractStrings(int minLength) {
    m_strings.clear();
    
    // Extract ASCII strings
    auto asciiStrings = extractAsciiStrings(minLength);
    
    // Extract Unicode strings
    auto unicodeStrings = extractUnicodeStrings(minLength);
    
    // Combine and sort by address
    m_strings = asciiStrings + unicodeStrings;
    std::sort(m_strings.begin(), m_strings.end(),
              [](const StringInfo& a, const StringInfo& b) {
                  return a.address < b.address;
              });
    
    qDebug() << "Extracted" << m_strings.size() << "strings total";
    return m_strings;
}

QVector<StringInfo> ResourceExtractor::extractAsciiStrings(int minLength) {
    QVector<StringInfo> strings;
    const QByteArray& data = m_loader.rawData();
    
    QString currentString;
    uint64_t stringStart = 0;
    
    for (uint64_t i = 0; i < static_cast<uint64_t>(data.size()); ++i) {
        uint8_t ch = static_cast<uint8_t>(data[i]);
        
        if (isValidAsciiChar(ch)) {
            if (currentString.isEmpty()) {
                stringStart = i;
            }
            currentString += QChar(ch);
        } else {
            // String ended
            if (currentString.length() >= minLength) {
                StringInfo info;
                info.address = stringStart;
                info.value = currentString;
                info.length = currentString.length();
                info.isAscii = true;
                info.isUnicode = false;
                strings.append(info);
            }
            currentString.clear();
        }
    }
    
    // Handle last string
    if (currentString.length() >= minLength) {
        StringInfo info;
        info.address = stringStart;
        info.value = currentString;
        info.length = currentString.length();
        info.isAscii = true;
        info.isUnicode = false;
        strings.append(info);
    }
    
    qDebug() << "Extracted" << strings.size() << "ASCII strings";
    return strings;
}

QVector<StringInfo> ResourceExtractor::extractUnicodeStrings(int minLength) {
    QVector<StringInfo> strings;
    const QByteArray& data = m_loader.rawData();
    
    QString currentString;
    uint64_t stringStart = 0;
    
    for (uint64_t i = 0; i + 1 < static_cast<uint64_t>(data.size()); i += 2) {
        uint16_t ch = *reinterpret_cast<const uint16_t*>(data.constData() + i);
        
        if (isValidUnicodeChar(ch)) {
            if (currentString.isEmpty()) {
                stringStart = i;
            }
            currentString += QChar(ch);
        } else if (ch == 0) {
            // String ended
            if (currentString.length() >= minLength) {
                StringInfo info;
                info.address = stringStart;
                info.value = currentString;
                info.length = currentString.length();
                info.isUnicode = true;
                info.isAscii = false;
                strings.append(info);
            }
            currentString.clear();
        }
    }
    
    qDebug() << "Extracted" << strings.size() << "Unicode strings";
    return strings;
}

QVector<ResourceInfo> ResourceExtractor::extractResources() {
    m_resources.clear();
    
    // PE resource extraction (simplified)
    if (m_loader.metadata().format == BinaryFormat::PE) {
        // Resources are in .rsrc section
        for (const auto& section : m_loader.sections()) {
            if (section.name == ".rsrc") {
                // Would parse resource directory here
                qDebug() << "Found PE resource section at" << section.virtualAddress;
            }
        }
    }
    
    return m_resources;
}

QVector<StringInfo> ResourceExtractor::searchStrings(const QString& pattern, bool caseSensitive) const {
    QVector<StringInfo> results;
    
    for (const auto& str : m_strings) {
        bool matches = false;
        
        if (caseSensitive) {
            matches = str.value.contains(pattern);
        } else {
            matches = str.value.toLower().contains(pattern.toLower());
        }
        
        if (matches) {
            results.append(str);
        }
    }
    
    return results;
}

bool ResourceExtractor::exportStrings(const QString& filename) const {
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Cannot open file for writing:" << filename;
        return false;
    }
    
    QTextStream stream(&file);
    
    // Write header
    stream << "Address\t\tString\t\t\tType\n";
    stream << "========\t\t======\t\t\t====\n\n";
    
    // Write strings
    for (const auto& str : m_strings) {
        stream << QString("0x%1\t%2\t%3\n")
                  .arg(str.address, 8, 16, QChar('0'))
                  .arg(str.value.left(40))
                  .arg(str.isUnicode ? "Unicode" : "ASCII");
    }
    
    file.close();
    
    qDebug() << "Exported" << m_strings.size() << "strings to" << filename;
    return true;
}

QMap<QString, int> ResourceExtractor::getStatistics() const {
    QMap<QString, int> stats;
    
    stats["Total Strings"] = m_strings.size();
    
    int asciiCount = 0, unicodeCount = 0;
    for (const auto& str : m_strings) {
        if (str.isAscii) asciiCount++;
        if (str.isUnicode) unicodeCount++;
    }
    
    stats["ASCII Strings"] = asciiCount;
    stats["Unicode Strings"] = unicodeCount;
    stats["Total Resources"] = m_resources.size();
    
    return stats;
}

bool ResourceExtractor::isValidAsciiChar(uint8_t ch) {
    return (ch >= 32 && ch <= 126) || ch == '\n' || ch == '\r' || ch == '\t';
}

bool ResourceExtractor::isValidUnicodeChar(uint16_t ch) {
    return (ch >= 32 && ch <= 0xFEFF) && ch != 0;
}

QString ResourceExtractor::extractStringAt(uint64_t offset, bool unicode) {
    const QByteArray& data = m_loader.rawData();
    
    if (unicode) {
        QString result;
        for (uint64_t i = offset; i + 1 < static_cast<uint64_t>(data.size()); i += 2) {
            uint16_t ch = *reinterpret_cast<const uint16_t*>(data.constData() + i);
            if (ch == 0) break;
            result += QChar(ch);
        }
        return result;
    } else {
        QString result;
        for (uint64_t i = offset; i < static_cast<uint64_t>(data.size()); ++i) {
            uint8_t ch = static_cast<uint8_t>(data[i]);
            if (ch == 0) break;
            result += QChar(ch);
        }
        return result;
    }
}
