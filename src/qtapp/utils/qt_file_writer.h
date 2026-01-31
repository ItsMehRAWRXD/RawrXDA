/**
 * \file qt_file_writer.h
 * \brief Qt-based implementation of IFileWriter interface
 * \author RawrXD Team
 * \date 2025-12-05
 */

#ifndef RAWRXD_QT_FILE_WRITER_H
#define RAWRXD_QT_FILE_WRITER_H

#include "../interfaces/ifile_writer.h"

namespace RawrXD {

/**
 * \brief Qt-based concrete implementation of file writing
 * 
 * Uses QSaveFile for atomic writes (write to temp, then rename).
 * This ensures data integrity even if the application crashes
 * during a save operation.
 */
class QtFileWriter : public void, public IFileWriter {

public:
    explicit QtFileWriter(void* parent = nullptr);
    ~QtFileWriter() override = default;
    
    // IFileWriter interface implementation
    FileOperationResult writeFile(const std::string& path,
                                 const std::string& content,
                                 bool createBackup = false) override;
    
    FileOperationResult writeFileRaw(const std::string& path,
                                    const std::vector<uint8_t>& data,
                                    bool createBackup = false) override;
    
    FileOperationResult createFile(const std::string& path) override;
    
    FileOperationResult deleteFile(const std::string& path, 
                                  bool moveToTrash = true) override;
    
    FileOperationResult renameFile(const std::string& oldPath,
                                  const std::string& newPath) override;
    
    FileOperationResult copyFile(const std::string& sourcePath,
                                const std::string& destPath,
                                bool overwrite = false) override;
    
    std::string createBackup(const std::string& path) override;
    
    void setAutoBackup(bool enable) override;
    
    bool isAutoBackupEnabled() const override;

private:
    bool m_autoBackup;
    
    std::string toAbsolutePath(const std::string& path) const;
    bool exists(const std::string& path) const;
};

} // namespace RawrXD

#endif // RAWRXD_QT_FILE_WRITER_H

