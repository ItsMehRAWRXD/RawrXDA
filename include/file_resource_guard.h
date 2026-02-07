#pragma once

#include <QString>
#include <QFile>
#include <QDir>
#include <memory>
#include <QDebug>

/**
 * @class FileResourceGuard
 * @brief RAII-style resource guard for file operations
 * 
 * Ensures proper cleanup of file resources even in error conditions.
 * Automatically closes files and can optionally clean up temporary files.
 */
class FileResourceGuard {
private:
    QString m_filePath;
    QFile* m_file;
    bool m_shouldCleanup;
    bool m_isTemporary;

public:
    /**
     * @brief Construct a file resource guard
     * @param filePath Path to the file being managed
     * @param file Pointer to QFile object (can be nullptr)
     * @param isTemporary Whether this is a temporary file that should be cleaned up
     */
    explicit FileResourceGuard(const QString& filePath, QFile* file = nullptr, bool isTemporary = false)
        : m_filePath(filePath)
        , m_file(file)
        , m_shouldCleanup(false)
        , m_isTemporary(isTemporary)
    {
        qDebug() << "[FileResourceGuard] Created guard for:" << filePath;
    }

    /**
     * @brief Destructor - automatically cleans up resources
     */
    ~FileResourceGuard() {
        release();
    }

    /**
     * @brief Mark this resource for cleanup
     */
    void markForCleanup() {
        m_shouldCleanup = true;
    }

    /**
     * @brief Release resources manually
     */
    void release() {
        // Close file if open
        if (m_file && m_file->isOpen()) {
            m_file->close();
            qDebug() << "[FileResourceGuard] Closed file:" << m_filePath;
        }

        // Clean up temporary file if marked
        if (m_shouldCleanup && m_isTemporary) {
            if (QFile::remove(m_filePath)) {
                qDebug() << "[FileResourceGuard] Cleaned up temporary file:" << m_filePath;
            } else {
                qWarning() << "[FileResourceGuard] Failed to clean up temporary file:" << m_filePath;
            }
        } else if (m_shouldCleanup) {
            qDebug() << "[FileResourceGuard] Marked for cleanup but not temporary:" << m_filePath;
        }
    }

    /**
     * @brief Get the file path being managed
     */
    const QString& filePath() const {
        return m_filePath;
    }

    /**
     * @brief Check if this is a temporary file
     */
    bool isTemporary() const {
        return m_isTemporary;
    }
};

/**
 * @class DirectoryResourceGuard
 * @brief RAII-style resource guard for directory operations
 * 
 * Ensures proper cleanup of directory resources even in error conditions.
 */
class DirectoryResourceGuard {
private:
    QString m_dirPath;
    bool m_shouldCleanup;

public:
    /**
     * @brief Construct a directory resource guard
     * @param dirPath Path to the directory being managed
     */
    explicit DirectoryResourceGuard(const QString& dirPath)
        : m_dirPath(dirPath)
        , m_shouldCleanup(false)
    {
        qDebug() << "[DirectoryResourceGuard] Created guard for:" << dirPath;
    }

    /**
     * @brief Destructor - automatically cleans up resources
     */
    ~DirectoryResourceGuard() {
        release();
    }

    /**
     * @brief Mark this resource for cleanup
     */
    void markForCleanup() {
        m_shouldCleanup = true;
    }

    /**
     * @brief Release resources manually
     */
    void release() {
        // Clean up directory if marked
        if (m_shouldCleanup) {
            QDir dir(m_dirPath);
            if (dir.exists()) {
                if (dir.removeRecursively()) {
                    qDebug() << "[DirectoryResourceGuard] Cleaned up directory:" << m_dirPath;
                } else {
                    qWarning() << "[DirectoryResourceGuard] Failed to clean up directory:" << m_dirPath;
                }
            }
        }
    }

    /**
     * @brief Get the directory path being managed
     */
    const QString& dirPath() const {
        return m_dirPath;
    }
};