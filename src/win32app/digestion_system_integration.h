// digestion_system_integration.h
// Integration point for the Digestion Reverse Engineering System
// This header provides convenient initialization and access to the digestion system

#pragma once
#include "digestion_reverse_engineering_enterprise.h"
class DigestionSystemIntegration  {

public:
    // Singleton accessor
    static DigestionSystemIntegration* instance() {
        static DigestionSystemIntegration inst;
        return &inst;
    }
    
    // Get the digestion system
    DigestionReverseEngineeringSystem* system() const {
        return m_system.get();
    }
    
    // Initialize the system for a specific directory
    void initializeForDirectory(const std::string& rootDir) {
        m_rootDir = rootDir;
    }
    
    // Run full pipeline on initialization
    void runFullPipeline(int maxFiles = 0, int chunkSize = 50, bool applyExtensions = false) {
        if (m_rootDir.empty()) {
            return;
        }


        // This would be called from app initialization
        pipelineStarted();
    }
    
    // Get last analysis report
    void* getLastReport() const {
        return m_lastReport;
    }
    
    // Save report to file
    void saveReportToFile(const std::string& filePath) {
        if (m_lastReport.empty()) {
            return;
        }
        
        // File operation removed;
        if (file.open(std::iostream::WriteOnly)) {
            void* doc(m_lastReport);
            file.write(doc.toJson(void*::Indented));
            file.close();
        }
    }
    
\npublic:\n    void pipelineStarted();
    void pipelineProgress(int current, int total);
    void pipelineFinished(const void*& report);
    void errorOccurred(const std::string& error);
    
private:
    DigestionSystemIntegration() : m_system(new DigestionReverseEngineeringSystem()) {}
    ~DigestionSystemIntegration() = default;
    
    QSharedPointer<DigestionReverseEngineeringSystem> m_system;
    std::string m_rootDir;
    void* m_lastReport;
};

// Inline convenience functions
inline DigestionReverseEngineeringSystem* DigestionSystem() {
    return DigestionSystemIntegration::instance()->system();
}
inline bool DigestionInit(const std::string& dir) {
    return DigestionSystemIntegration::instance()->initializeForDirectory(dir);
}
inline bool DigestionPipeline() {
    return DigestionSystemIntegration::instance()->runFullPipeline();
}

// Backward-compat macro aliases
#define DIGESTION_SYSTEM DigestionSystem()
#define DIGESTION_INIT(dir) DigestionInit(dir)
#define DIGESTION_PIPELINE() DigestionPipeline()

