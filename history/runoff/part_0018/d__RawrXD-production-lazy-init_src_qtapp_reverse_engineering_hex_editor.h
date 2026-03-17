/**
 * \file hex_editor.h
 * \brief Hex editor for binary viewing and editing
 * \author RawrXD Team
 * \date 2026-01-22
 */

#pragma once

#include <QString>
#include <QByteArray>
#include <QVector>
#include <QMap>

namespace RawrXD {
namespace ReverseEngineering {

/**
 * \struct HexBookmark
 * \brief A bookmark in the binary
 */
struct HexBookmark {
    uint64_t address;           ///< Bookmark address
    QString label;              ///< Bookmark label
    QString comment;            ///< Bookmark comment
    uint32_t color;             ///< Highlight color (RGB)
};

/**
 * \class HexEditor
 * \brief Hex editor for binary files
 */
class HexEditor {
public:
    /**
     * \brief Construct hex editor
     */
    explicit HexEditor();
    
    /**
     * \brief Open binary file
     * \param filePath Path to file
     * \return True if successful
     */
    bool open(const QString& filePath);
    
    /**
     * \brief Get raw binary data
     * \return Binary data
     */
    const QByteArray& data() const { return m_data; }
    
    /**
     * \brief Get bytes at offset
     * \param offset Byte offset
     * \param size Number of bytes
     * \return Bytes at location
     */
    QByteArray bytesAt(uint64_t offset, size_t size) const;
    
    /**
     * \brief Read value at offset
     * \param offset Byte offset
     * \param size Value size (1, 2, 4, 8)
     * \param littleEndian Byte order
     * \return Value as uint64_t
     */
    uint64_t readValue(uint64_t offset, int size, bool littleEndian) const;
    
    /**
     * \brief Write value at offset
     * \param offset Byte offset
     * \param value Value to write
     * \param size Value size (1, 2, 4, 8)
     * \param littleEndian Byte order
     * \return True if successful
     */
    bool writeValue(uint64_t offset, uint64_t value, int size, bool littleEndian);
    
    /**
     * \brief Replace bytes at offset
     * \param offset Byte offset
     * \param data New data
     * \return True if successful
     */
    bool replaceBytes(uint64_t offset, const QByteArray& data);
    
    /**
     * \brief Insert bytes at offset
     * \param offset Byte offset
     * \param data Data to insert
     * \return True if successful
     */
    bool insertBytes(uint64_t offset, const QByteArray& data);
    
    /**
     * \brief Delete bytes at offset
     * \param offset Byte offset
     * \param size Number of bytes to delete
     * \return True if successful
     */
    bool deleteBytes(uint64_t offset, size_t size);
    
    /**
     * \brief Find sequence of bytes
     * \param pattern Bytes to search for
     * \param startOffset Search start offset
     * \return Offset of first match, or -1 if not found
     */
    int64_t findBytes(const QByteArray& pattern, uint64_t startOffset = 0) const;
    
    /**
     * \brief Find all matches
     * \param pattern Bytes to search for
     * \return Vector of offsets
     */
    QVector<uint64_t> findAllBytes(const QByteArray& pattern) const;
    
    /**
     * \brief Add bookmark
     * \param address Bookmark address
     * \param label Bookmark label
     * \param comment Optional comment
     * \param color Highlight color
     */
    void addBookmark(uint64_t address, const QString& label, 
                     const QString& comment = "", uint32_t color = 0xFF0000);
    
    /**
     * \brief Remove bookmark
     * \param address Bookmark address
     */
    void removeBookmark(uint64_t address);
    
    /**
     * \brief Get all bookmarks
     * \return Vector of bookmarks
     */
    const QVector<HexBookmark>& bookmarks() const { return m_bookmarks; }
    
    /**
     * \brief Get bookmark at address
     * \param address Address to check
     * \return Bookmark, or empty if not found
     */
    HexBookmark bookmarkAt(uint64_t address) const;
    
    /**
     * \brief Add annotation/comment
     * \param offset Byte offset
     * \param comment Comment text
     */
    void annotate(uint64_t offset, const QString& comment);
    
    /**
     * \brief Get annotation
     * \param offset Byte offset
     * \return Comment text, or empty if not found
     */
    QString getAnnotation(uint64_t offset) const;
    
    /**
     * \brief Save changes to file
     * \param filePath Save destination (default: original file)
     * \return True if successful
     */
    bool save(const QString& filePath = "");
    
    /**
     * \brief Get file size
     * \return File size in bytes
     */
    uint64_t fileSize() const { return m_data.size(); }
    
    /**
     * \brief Check if file was modified
     * \return True if modified
     */
    bool isModified() const { return m_modified; }

private:
    QByteArray m_data;
    QByteArray m_originalData;
    QString m_filePath;
    bool m_modified;
    QVector<HexBookmark> m_bookmarks;
    QMap<uint64_t, QString> m_annotations;
};

} // namespace ReverseEngineering
} // namespace RawrXD
