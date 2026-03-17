/**
 * \file resource_extractor.h
 * \brief Extract strings and resources from binaries
 * \author RawrXD Team
 * \date 2026-01-22
 */

#pragma once

#include "binary_loader.h"
#include <QString>
#include <QVector>
#include <QMap>

namespace RawrXD {
namespace ReverseEngineering {

/**
 * \struct StringInfo
 * \brief Information about a string in the binary
 */
struct StringInfo {
    uint64_t address;           ///< String address/RVA
    QString value;              ///< String value
    uint32_t length;            ///< String length
    bool isUnicode;             ///< True if Unicode string
    bool isAscii;               ///< True if ASCII string
    int frequency;              ///< Number of references to this string
};

/**
 * \struct ResourceInfo
 * \brief Information about a resource
 */
struct ResourceInfo {
    uint64_t address;           ///< Resource address/RVA
    QString type;               ///< Resource type (BITMAP, DIALOG, ICON, etc.)
    uint32_t size;              ///< Resource size
    QString name;               ///< Resource name (if named)
    QByteArray data;            ///< Resource data
};

/**
 * \class ResourceExtractor
 * \brief Extracts strings and resources from binaries
 */
class ResourceExtractor {
public:
    /**
     * \brief Construct extractor from binary loader
     * \param loader Loaded binary
     */
    explicit ResourceExtractor(const BinaryLoader& loader);
    
    /**
     * \brief Extract all strings
     * \param minLength Minimum string length to extract
     * \return Vector of strings found
     */
    QVector<StringInfo> extractStrings(int minLength = 4);
    
    /**
     * \brief Extract ASCII strings
     * \param minLength Minimum string length
     * \return Vector of ASCII strings
     */
    QVector<StringInfo> extractAsciiStrings(int minLength = 4);
    
    /**
     * \brief Extract Unicode strings
     * \param minLength Minimum string length
     * \return Vector of Unicode strings
     */
    QVector<StringInfo> extractUnicodeStrings(int minLength = 4);
    
    /**
     * \brief Extract resources (Windows PE)
     * \return Vector of resources found
     */
    QVector<ResourceInfo> extractResources();
    
    /**
     * \brief Get all extracted strings
     * \return Vector of all strings
     */
    const QVector<StringInfo>& strings() const { return m_strings; }
    
    /**
     * \brief Get all extracted resources
     * \return Vector of all resources
     */
    const QVector<ResourceInfo>& resources() const { return m_resources; }
    
    /**
     * \brief Search strings by text
     * \param pattern Text to search for
     * \param caseSensitive Case sensitive search
     * \return Vector of matching strings
     */
    QVector<StringInfo> searchStrings(const QString& pattern, bool caseSensitive = false) const;
    
    /**
     * \brief Export strings to text file
     * \param filename Output filename
     * \return True if successful
     */
    bool exportStrings(const QString& filename) const;
    
    /**
     * \brief Get statistics about extracted data
     * \return Map of statistic names to values
     */
    QMap<QString, int> getStatistics() const;

private:
    const BinaryLoader& m_loader;
    QVector<StringInfo> m_strings;
    QVector<ResourceInfo> m_resources;
    
    // Helper methods
    bool isValidAsciiChar(uint8_t ch);
    bool isValidUnicodeChar(uint16_t ch);
    QString extractStringAt(uint64_t offset, bool unicode);
};

} // namespace ReverseEngineering
} // namespace RawrXD
