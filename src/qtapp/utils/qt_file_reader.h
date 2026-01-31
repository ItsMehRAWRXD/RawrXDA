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
 * Uses Qt's std::fstream, QTextStream, and QTextCodec for file operations.
 * This class is the low-level module that high-level code should NOT
 * depend on directly - use IFileReader interface instead.
 */
class QtFileReader : public IFileReader {
public:
    QtFileReader() = default;
    ~QtFileReader() override = default;
    
    // IFileReader interface implementation
    bool readFile(const std::string& path, 
                 std::string& content, 
                 Encoding* detectedEncoding = nullptr) const override;
    
    bool readFileRaw(const std::string& path, std::vector<uint8_t>& data) const override;
    
    Encoding detectEncoding(const std::vector<uint8_t>& data) const override;
    
    bool exists(const std::string& path) const override;
    
    bool isFile(const std::string& path) const override;
    
    bool isReadable(const std::string& path) const override;
    
    int64_t fileSize(const std::string& path) const override;
};

} // namespace RawrXD

#endif // RAWRXD_QT_FILE_READER_H

