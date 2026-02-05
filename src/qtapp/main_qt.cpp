// RawrXD IDE - C++ Migration from PowerShell
#include <QApplication>
#include <QMessageBox>
#include <QDebug>
#include <QFile>
#include <QDir>
// #include "auto_update.hpp"
#include "MainWindow.h"
#include "../centralized_exception_handler.h"
#include "../production_config_manager.h"

#ifdef ENABLE_TIER2_INTEGRATION
#include "../tier2_production_integration.h"
#endif

int main(int argc, char* argv[])
{
    try {
        // Install global exception handler FIRST
        RawrXD::CentralizedExceptionHandler::instance().installHandler();
        RawrXD::CentralizedExceptionHandler::instance().enableAutomaticRecovery(true);
        
        // Create logs directory
        QDir().mkpath(QCoreApplication::applicationDirPath() + "/logs");
        
        QApplication app(argc, argv);
        
        qDebug() << "Starting RawrXD-QtShell...";
        
        // Load production configuration
        auto& configManager = RawrXD::ProductionConfigManager::instance();
        if (!configManager.loadConfig()) {
            qWarning() << "Failed to load production config, using defaults";
        }
        
        qDebug() << "Configuration environment:" << configManager.getEnvironment();
        
#ifdef ENABLE_TIER2_INTEGRATION
        // Initialize Tier2 production systems if enabled
        RawrXD::ProductionTier2Manager* tier2Manager = nullptr;
        if (configManager.isFeatureEnabled("tier2_integration")) {
            qDebug() << "Initializing Tier2 production systems...";
            tier2Manager = new RawrXD::ProductionTier2Manager(&app);
            
            if (!tier2Manager->initialize()) {
                qWarning() << "Tier2 initialization failed, continuing without enhanced features";
                delete tier2Manager;
                tier2Manager = nullptr;
            } else {
                qDebug() << "Tier2 systems initialized successfully";
            }
        }
#endif
        
        // Disable auto-update during initial testing
        // AutoUpdate updater;
        // updater.checkAndInstall();
        
        qDebug() << "Creating MainWindow...";
        MainWindow window;
        qDebug() << "Showing window...";
        window.show();
        
        qDebug() << "Entering event loop...";
        int result = app.exec();
        
#ifdef ENABLE_TIER2_INTEGRATION
        // Cleanup Tier2 systems
        if (tier2Manager) {
            qDebug() << "Shutting down Tier2 systems...";
            delete tier2Manager;
        }
#endif
        
        // Uninstall exception handler
        RawrXD::CentralizedExceptionHandler::instance().uninstallHandler();
        
        return result;
    }
    catch (const std::exception& e) {
        // Report through centralized handler
        RawrXD::CentralizedExceptionHandler::instance().reportException(e);
        
        QFile errorLog("startup_crash.txt");
        if (errorLog.open(QIODevice::WriteOnly | QIODevice::Text)) {
            errorLog.write(e.what());
            errorLog.close();
        }
        return -1;
    }
    catch (...) {
        RawrXD::CentralizedExceptionHandler::instance().reportError(
            "Unknown exception in main()", 
            "unknown", 
            QJsonObject()
        );
        return -1;
    }
}
