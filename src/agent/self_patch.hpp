#pragma once
#include <QString>
#include <QObject>

class SelfPatch : public QObject {
    Q_OBJECT
public:
    explicit SelfPatch(QObject* parent = nullptr);
    
    // Add Vulkan kernel from template
    bool addKernel(const QString& name, const QString& templateName);
    
    // Add C++ wrapper for kernel
    bool addCpp(const QString& name, const QString& deps);
    
    // Hot-reload the binary (rebuild + restart)
    bool hotReload();
    
    // Patch existing file
    bool patchFile(const QString& filename, const QString& patch);
    
signals:
    void kernelAdded(const QString& name);
    void cppAdded(const QString& name);
    void reloadStarted();
    void reloadCompleted(bool success);
};
