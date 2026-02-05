/**
 * \file qt_file_reader.cpp
 * \brief Implementation of Qt-based file reader
 * \author RawrXD Team
 * \date 2025-12-05
 */

#include "qt_file_reader.h"
#include <QFile>
#include <QFileInfo>
#include <QDebug>

namespace RawrXD {

bool QtFileReader::readFile(const QString& path, 
                            QString& content, 
                            Encoding* detectedEncoding) const 
{
    QByteArray rawData;
    if (!readFileRaw(path, rawData)) {
        return false;
    }
    
    Encoding encoding = detectEncoding(rawData);
    if (detectedEncoding) {
        *detectedEncoding = encoding;
    }
    
    // Convert based on detected encoding
    switch (encoding) {
        case Encoding::UTF8:
            content = QString::fromUtf8(rawData);
            break;
            
        case Encoding::UTF16_LE:
            content = QString::fromUtf16(
                reinterpret_cast<const char16_t*>(rawData.constData()), 
                rawData.size() / 2
            );
            break;
            
        case Encoding::UTF16_BE: {
            // Swap bytes for big endian
            QByteArray swapped;
            swapped.reserve(rawData.size());
            for (int i = 0; i < rawData.size() - 1; i += 2) {
                swapped.append(rawData[i + 1]);
                swapped.append(rawData[i]);
            }
            content = QString::fromUtf16(
                reinterpret_cast<const char16_t*>(swapped.constData()), 
                swapped.size() / 2
            );
            break;
        }
        
        case Encoding::ASCII:
        case Encoding::Unknown:
        default:
            // Try UTF-8 first, fall back to Latin-1
            content = QString::fromUtf8(rawData);
            if (content.contains(QChar::ReplacementCharacter)) {
                content = QString::fromLatin1(rawData);
            }
            break;
    }
    
    return true;
}

bool QtFileReader::readFileRaw(const QString& path, QByteArray& data) const {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "QtFileReader: Failed to open file for reading:" 
                   << path << "-" << file.errorString();
        return false;
    }
    
    data = file.readAll();
    return !data.isNull();
}

Encoding QtFileReader::detectEncoding(const QByteArray& data) const {
    if (data.isEmpty()) {
        return Encoding::UTF8; // Default to UTF-8
    }
    
    // Check for BOM (Byte Order Mark)
    if (data.size() >= 3 && data[0] == '\xEF' && data[1] == '\xBB' && data[2] == '\xBF') {
        return Encoding::UTF8; // UTF-8 BOM
    }
    if (data.size() >= 2 && data[0] == '\xFF' && data[1] == '\xFE') {
        return Encoding::UTF16_LE; // UTF-16 LE BOM
    }
    if (data.size() >= 2 && data[0] == '\xFE' && data[1] == '\xFF') {
        return Encoding::UTF16_BE; // UTF-16 BE BOM
    }
    
    // Heuristic detection for UTF-8
    int utf8Sequences = 0;
    int asciiChars = 0;
    
    for (int i = 0; i < data.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(data[i]);
        
        if (c < 0x80) {
            asciiChars++;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < data.size() && 
                   (static_cast<unsigned char>(data[i + 1]) & 0xC0) == 0x80) {
            utf8Sequences++;
            i += 1;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < data.size()) {
            if ((static_cast<unsigned char>(data[i + 1]) & 0xC0) == 0x80 &&
                (static_cast<unsigned char>(data[i + 2]) & 0xC0) == 0x80) {
                utf8Sequences++;
                i += 2;
            }
        } else if ((c & 0xF8) == 0xF0 && i + 3 < data.size()) {
            if ((static_cast<unsigned char>(data[i + 1]) & 0xC0) == 0x80 &&
                (static_cast<unsigned char>(data[i + 2]) & 0xC0) == 0x80 &&
                (static_cast<unsigned char>(data[i + 3]) & 0xC0) == 0x80) {
                utf8Sequences++;
                i += 3;
            }
        }
    }
    
    // If we found valid UTF-8 sequences, it's likely UTF-8
    if (utf8Sequences > 0) {
        return Encoding::UTF8;
    }
    
    // If all ASCII characters, report as ASCII
    if (asciiChars == data.size()) {
        return Encoding::ASCII;
    }
    
    return Encoding::Unknown;
}

bool QtFileReader::exists(const QString& path) const {
    return QFileInfo::exists(path);
}

bool QtFileReader::isFile(const QString& path) const {
    QFileInfo info(path);
    return info.exists() && info.isFile();
}

bool QtFileReader::isReadable(const QString& path) const {
    QFileInfo info(path);
    return info.isReadable();
}

qint64 QtFileReader::fileSize(const QString& path) const {
    QFileInfo info(path);
    return info.exists() ? info.size() : -1;
}

} // namespace RawrXD
