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
class QtFileWriter : public IFileWriter {
public:
    QtFileWriter();
    ~QtFileWriter() override = default;
    
    // IFileWriter interface implementation
    FileOperationResult writeFile(const QString& path,
                                 const QString& content,
                                 bool createBackup = false) override;
    
    FileOperationResult writeFileRaw(const QString& path,
                                    const QByteArray& data,
                                    bool createBackup = false) override;
    
    FileOperationResult createFile(const QString& path) override;
    
    FileOperationResult deleteFile(const QString& path, 
                                  bool moveToTrash = true) override;
    
    FileOperationResult renameFile(const QString& oldPath,
                                  const QString& newPath) override;
    
    FileOperationResult copyFile(const QString& sourcePath,
                                const QString& destPath,
                                bool overwrite = false) override;
    
    QString createBackup(const QString& path) override;
    
    void setAutoBackup(bool enable) override;
    
    bool isAutoBackupEnabled() const override;

private:
    bool m_autoBackup;
    
    QString toAbsolutePath(const QString& path) const;
    bool exists(const QString& path) const;
};

} // namespace RawrXD

#endif // RAWRXD_QT_FILE_WRITER_H
