#include "file_system_manager.h"

#include <QFile>
#include <QDir>
#include <QTextCodec>
#include <QTextStream>
#include <QTextDecoder>
#include <QFileInfo>
#include <QStandardPaths>
#include <QSettings>
#include <QDebug>
#include <QFileSystemWatcher>
#include <QMutex>

FileSystemManager& FileSystemManager::instance() {
    static FileSystemManager s_instance;
    return s_instance;
}

FileSystemManager::FileSystemManager()
    : QObject(nullptr)
    , m_watcher(std::make_unique<QFileSystemWatcher>())
{
    connect(m_watcher.get(), &QFileSystemWatcher::fileChanged,
            this, &FileSystemManager::onFileChanged);
    
    // Load recent files from settings
    QSettings settings("RawrXD", "AgenticIDE");
    m_recentFiles = settings.value("recentFiles", QStringList()).toStringList();
}

FileSystemManager::~FileSystemManager() = default;

FileSystemManager::FileStatus FileSystemManager::readFile(
    const QString& path, QString& outContent, FileEncoding& outEncoding)
{
    try {
        QFile file(path);
        if (!file.exists()) {
            qWarning() << "File not found:" << path;
            return FileStatus::NotFound;
        }

        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "Cannot open file:" << path << file.errorString();
            if (file.error() == QFile::PermissionsError) {
                return FileStatus::PermissionDenied;
            }
            return FileStatus::IOError;
        }

        QByteArray data = file.readAll();
        file.close();

        // Detect encoding
        outEncoding = detectEncoding(data);
        
        QTextCodec* codec = nullptr;
        switch (outEncoding) {
            case FileEncoding::UTF8:
                codec = QTextCodec::codecForName("UTF-8");
                break;
            case FileEncoding::UTF16:
                codec = QTextCodec::codecForName("UTF-16");
                break;
            case FileEncoding::Latin1:
                codec = QTextCodec::codecForName("ISO-8859-1");
                break;
            case FileEncoding::System:
                codec = QTextCodec::codecForLocale();
                break;
            default:
                codec = QTextCodec::codecForName("UTF-8");
                break;
        }

        if (!codec) {
            qWarning() << "Codec not found, using UTF-8";
            codec = QTextCodec::codecForName("UTF-8");
        }

        outContent = codec->toUnicode(data);

        // Update stats
        emit fileRead(path);
        return FileStatus::OK;

    } catch (const std::exception& e) {
        qCritical() << "Exception reading file:" << path << e.what();
        return FileStatus::IOError;
    }
}

FileSystemManager::FileStatus FileSystemManager::writeFile(
    const QString& path, const QString& content, FileEncoding encoding)
{
    try {
        // Ensure directory exists
        QFileInfo fileInfo(path);
        if (!fileInfo.dir().exists()) {
            if (!createDirectory(fileInfo.dir().absolutePath())) {
                qWarning() << "Cannot create directory:" << fileInfo.dir().absolutePath();
                return FileStatus::IOError;
            }
        }

        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            qWarning() << "Cannot write file:" << path << file.errorString();
            if (file.error() == QFile::PermissionsError) {
                return FileStatus::PermissionDenied;
            }
            return FileStatus::IOError;
        }

        // Select codec
        QTextCodec* codec = nullptr;
        switch (encoding) {
            case FileEncoding::UTF8:
                codec = QTextCodec::codecForName("UTF-8");
                break;
            case FileEncoding::UTF16:
                codec = QTextCodec::codecForName("UTF-16");
                break;
            case FileEncoding::Latin1:
                codec = QTextCodec::codecForName("ISO-8859-1");
                break;
            case FileEncoding::System:
                codec = QTextCodec::codecForLocale();
                break;
            default:
                codec = QTextCodec::codecForName("UTF-8");
                break;
        }

        QByteArray data = codec->fromUnicode(content);
        file.write(data);
        file.close();

        // Add to recent files
        addRecentFile(path);

        emit fileWritten(path);
        return FileStatus::OK;

    } catch (const std::exception& e) {
        qCritical() << "Exception writing file:" << path << e.what();
        return FileStatus::IOError;
    }
}

bool FileSystemManager::fileExists(const QString& path) const {
    return QFile::exists(path);
}

qint64 FileSystemManager::getLastModifiedTime(const QString& path) const {
    QFileInfo info(path);
    return info.lastModified().toMSecsSinceEpoch();
}

void FileSystemManager::watchFile(const QString& path) {
    if (!m_watcher->files().contains(path)) {
        m_watcher->addPath(path);
        qDebug() << "Watching file:" << path;
    }
}

void FileSystemManager::unwatchFile(const QString& path) {
    if (m_watcher->files().contains(path)) {
        m_watcher->removePath(path);
        qDebug() << "Stopped watching:" << path;
    }
}

void FileSystemManager::addRecentFile(const QString& path) {
    m_recentFiles.removeAll(path);  // Remove duplicates
    m_recentFiles.prepend(path);
    
    // Keep only last N files
    if (m_recentFiles.size() > MAX_RECENT_FILES) {
        m_recentFiles = m_recentFiles.mid(0, MAX_RECENT_FILES);
    }

    // Persist to settings
    QSettings settings("RawrXD", "AgenticIDE");
    settings.setValue("recentFiles", m_recentFiles);
}

QStringList FileSystemManager::getRecentFiles(int maxCount) const {
    return m_recentFiles.mid(0, maxCount);
}

void FileSystemManager::clearRecentFiles() {
    m_recentFiles.clear();
    QSettings settings("RawrXD", "AgenticIDE");
    settings.remove("recentFiles");
}

QString FileSystemManager::encodingName(FileEncoding encoding) {
    switch (encoding) {
        case FileEncoding::UTF8:
            return "UTF-8";
        case FileEncoding::UTF16:
            return "UTF-16";
        case FileEncoding::Latin1:
            return "ISO-8859-1";
        case FileEncoding::System:
            return "System";
        default:
            return "Unknown";
    }
}

FileSystemManager::FileEncoding FileSystemManager::detectEncoding(const QByteArray& data) {
    if (data.isEmpty()) {
        return FileEncoding::UTF8;
    }

    // Check for BOM
    if (data.startsWith("\xEF\xBB\xBF")) {
        return FileEncoding::UTF8;
    }
    if (data.startsWith("\xFF\xFE") || data.startsWith("\xFE\xFF")) {
        return FileEncoding::UTF16;
    }

    // Try to detect UTF-8
    bool isValidUtf8 = true;
    for (int i = 0; i < data.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(data[i]);
        
        if ((c & 0x80) == 0) {
            continue;  // ASCII
        } else if ((c & 0xE0) == 0xC0) {
            if (i + 1 >= data.size() || (static_cast<unsigned char>(data[i + 1]) & 0xC0) != 0x80) {
                isValidUtf8 = false;
                break;
            }
            i++;
        } else if ((c & 0xF0) == 0xE0) {
            if (i + 2 >= data.size()) {
                isValidUtf8 = false;
                break;
            }
            unsigned char c1 = static_cast<unsigned char>(data[i + 1]);
            unsigned char c2 = static_cast<unsigned char>(data[i + 2]);
            if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80) {
                isValidUtf8 = false;
                break;
            }
            i += 2;
        } else if ((c & 0xF8) == 0xF0) {
            if (i + 3 >= data.size()) {
                isValidUtf8 = false;
                break;
            }
            unsigned char c1 = static_cast<unsigned char>(data[i + 1]);
            unsigned char c2 = static_cast<unsigned char>(data[i + 2]);
            unsigned char c3 = static_cast<unsigned char>(data[i + 3]);
            if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80) {
                isValidUtf8 = false;
                break;
            }
            i += 3;
        } else {
            isValidUtf8 = false;
            break;
        }
    }

    if (isValidUtf8) {
        return FileEncoding::UTF8;
    }

    // Default to system encoding
    return FileEncoding::System;
}

qint64 FileSystemManager::getFileSize(const QString& path) const {
    QFileInfo info(path);
    return info.size();
}

int FileSystemManager::countLines(const QString& content) const {
    if (content.isEmpty()) {
        return 0;
    }
    return content.count('\n') + 1;
}

QString FileSystemManager::getExtension(const QString& path) {
    QFileInfo info(path);
    return info.suffix();
}

QString FileSystemManager::getFileName(const QString& path) {
    QFileInfo info(path);
    return info.fileName();
}

QString FileSystemManager::getDirectory(const QString& path) {
    QFileInfo info(path);
    return info.dir().absolutePath();
}

bool FileSystemManager::createDirectory(const QString& path) {
    QDir dir;
    return dir.mkpath(path);
}

bool FileSystemManager::deleteFile(const QString& path) {
    unwatchFile(path);
    return QFile::remove(path);
}

bool FileSystemManager::deleteDirectory(const QString& path) {
    QDir dir(path);
    return dir.removeRecursively();
}

QString FileSystemManager::getAbsolutePath(const QString& path) {
    return QFileInfo(path).absoluteFilePath();
}

QString FileSystemManager::normalizePath(const QString& path) {
    return QDir::cleanPath(path).replace('\\', '/');
}

void FileSystemManager::onFileChanged(const QString& path) {
    qDebug() << "External file change detected:" << path;
    emit fileChangedExternally(path);
}
