#pragma once


#include <vector>
#include <map>

class CIPipelineManager : public void {

public:
    enum class PipelineStatus {
        Idle,
        Running,
        Success,
        Failed,
        Cancelled
    };

    explicit CIPipelineManager(void* parent = nullptr);
    ~CIPipelineManager();

    // Pipeline configuration
    std::string createPipeline(const std::string& name, const void*& config);
    bool updatePipeline(const std::string& pipelineId, const void*& config);
    bool deletePipeline(const std::string& pipelineId);

    // Pipeline execution
    bool startPipeline(const std::string& pipelineId);
    bool stopPipeline(const std::string& pipelineId);
    PipelineStatus getPipelineStatus(const std::string& pipelineId) const;

    // Integration with version control
    void setVCSIntegration(const std::string& vcsType, const void*& config);
    bool triggerOnCommit(const std::string& pipelineId, const std::string& commitHash);

    // Notifications and reporting
    void setNotificationSettings(const void*& settings);
    void* getPipelineReport(const std::string& pipelineId) const;


    void pipelineStarted(const std::string& pipelineId);
    void pipelineCompleted(const std::string& pipelineId, bool success);
    void pipelineStatusChanged(const std::string& pipelineId, PipelineStatus status);

private:
    struct Pipeline {
        std::string id;
        std::string name;
        void* config;
        PipelineStatus status;
        QProcess* process;
        std::chrono::system_clock::time_point startTime;
        std::chrono::system_clock::time_point endTime;
    };

    std::map<std::string, Pipeline> m_pipelines;
    void* m_vcsConfig;
    void* m_notificationSettings;
};

