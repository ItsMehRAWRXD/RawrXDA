#pragma once

#include <QString>
#include <QDir>
#include <string>

// Minimal PathResolver stub for Win32IDE compilation
// Provides basic path resolution without external dependencies

class PathResolver {
public:
    static QString getPowershieldPath() {
        return "C:/Powershield";
    }
    
    static QString getWorkspacePath() {
        return ".";
    }
    
    static QString getConfigPath() {
        return "./config";
    }
    
    static QString getHomePath() {
        return QDir::homePath();
    }
    
    static QString getDesktopPath() {
        return QDir::homePath() + "/Desktop";
    }
    
    static QString getLogsPath() {
        return "./logs";
    }
    
    static void ensurePathExists(const std::string& path) {
        QDir dir;
        dir.mkpath(QString::fromStdString(path));
    }
};
