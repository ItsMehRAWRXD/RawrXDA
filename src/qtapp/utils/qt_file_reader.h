/**
 * \file qt_file_reader.h
 * \brief Qt-based implementation of IFileReader interface
 * \author RawrXD Team
 * \date 2025-12-05
 */

#ifndef RAWRXD_QT_FILE_READER_H
#define RAWRXD_QT_FILE_READER_H

#include "../interfaces/ifile_reader.h"

namespace RawrXD {

/**
 * \brief Qt-based concrete implementation of file reading
 * 
 * Uses Qt's QFile, QTextStream, and QTextCodec for file operations.
 * This class is the low-level module that high-level code should NOT
 * depend on directly - use IFileReader interface instead.
 */
class QtFileReader : public IFileReader {
public:
    QtFileReader() = default;
    ~QtFileReader() override = default;
    
    // IFileReader interface implementation
    bool readFile(const QString& path, 
                 QString& content, 
                 Encoding* detectedEncoding = nullptr) const override;
    
    bool readFileRaw(const QString& path, QByteArray& data) const override;
    
    Encoding detectEncoding(const QByteArray& data) const override;
    
    bool exists(const QString& path) const override;
    
    bool isFile(const QString& path) const override;
    
    bool isReadable(const QString& path) const override;
    
    qint64 fileSize(const QString& path) const override;
};

} // namespace RawrXD

#endif // RAWRXD_QT_FILE_READER_H
