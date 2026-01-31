#include "ci_pipeline_manager.h"


CIPipelineManager::CIPipelineManager(void *parent) 
    : void(parent), m_vcsConfig(void*()), m_notificationSettings(void*())
{
}

CIPipelineManager::~CIPipelineManager()
{
    // Clean up any running processes
    for (auto& [id, pipeline] : m_pipelines) {
        if (pipeline.process && pipeline.process->state() == void*::Running) {
            pipeline.process->terminate();
            pipeline.process->waitForFinished(5000);
            delete pipeline.process;
        }
    }
    m_pipelines.clear();
}

std::string CIPipelineManager::createPipeline(const std::string& name, const void*& config)
{
    std::string pipelineId = std::string("pipeline_%1_%2"));
    
    Pipeline newPipeline;
    newPipeline.id = pipelineId;
    newPipeline.name = name;
    newPipeline.config = config;
    newPipeline.status = PipelineStatus::Idle;
    newPipeline.process = nullptr;
    newPipeline.startTime = std::chrono::system_clock::time_point();
    newPipeline.endTime = std::chrono::system_clock::time_point();
    
    m_pipelines[pipelineId] = newPipeline;
    
    return pipelineId;
}

bool CIPipelineManager::updatePipeline(const std::string& pipelineId, const void*& config)
{
    auto it = m_pipelines.find(pipelineId);
    if (it == m_pipelines.end()) {
        return false;
    }
    
    if (it->second.status == PipelineStatus::Running) {
        return false;
    }
    
    it->second.config = config;
    return true;
}

bool CIPipelineManager::deletePipeline(const std::string& pipelineId)
{
    auto it = m_pipelines.find(pipelineId);
    if (it == m_pipelines.end()) {
        return false;
    }
    
    if (it->second.process) {
        if (it->second.process->state() == void*::Running) {
            it->second.process->terminate();
            it->second.process->waitForFinished(5000);
        }
        delete it->second.process;
    }
    
    m_pipelines.erase(it);
    return true;
}

bool CIPipelineManager::startPipeline(const std::string& pipelineId)
{
    auto it = m_pipelines.find(pipelineId);
    if (it == m_pipelines.end()) {
        return false;
    }
    
    Pipeline& pipeline = it->second;
    
    if (pipeline.status == PipelineStatus::Running) {
        return false;
    }
    
    pipeline.status = PipelineStatus::Running;
    pipeline.startTime = std::chrono::system_clock::time_point::currentDateTime();
    pipeline.process = new void*(this);
    
    // Extract build command from config
    std::string buildCommand = pipeline.config.contains("buildCommand") 
        ? pipeline.config["buildCommand"].toString() 
        : "echo Running pipeline";
    std::string workDir = pipeline.config.contains("workDir") 
        ? pipeline.config["workDir"].toString() 
        : ".";
    
    pipeline.process->setWorkingDirectory(workDir);
    pipeline.process->start(buildCommand);
    
    pipelineStarted(pipelineId);
    
    return true;
}

bool CIPipelineManager::stopPipeline(const std::string& pipelineId)
{
    auto it = m_pipelines.find(pipelineId);
    if (it == m_pipelines.end()) {
        return false;
    }
    
    Pipeline& pipeline = it->second;
    
    if (pipeline.status != PipelineStatus::Running || !pipeline.process) {
        return false;
    }
    
    pipeline.process->terminate();
    bool finished = pipeline.process->waitForFinished(5000);
    
    if (!finished) {
        pipeline.process->kill();
        pipeline.process->waitForFinished();
    }
    
    pipeline.status = PipelineStatus::Cancelled;
    pipeline.endTime = std::chrono::system_clock::time_point::currentDateTime();
    
    pipelineCompleted(pipelineId, false);
    pipelineStatusChanged(pipelineId, pipeline.status);
    
    return true;
}

CIPipelineManager::PipelineStatus CIPipelineManager::getPipelineStatus(const std::string& pipelineId) const
{
    auto it = m_pipelines.find(pipelineId);
    if (it == m_pipelines.end()) {
        return PipelineStatus::Idle;
    }
    
    return it->second.status;
}

void CIPipelineManager::setVCSIntegration(const std::string& vcsType, const void*& config)
{
    m_vcsConfig = config;
    m_vcsConfig["type"] = vcsType;
}

bool CIPipelineManager::triggerOnCommit(const std::string& pipelineId, const std::string& commitHash)
{
    auto it = m_pipelines.find(pipelineId);
    if (it == m_pipelines.end()) {
        return false;
    }
    
    return startPipeline(pipelineId);
}

void CIPipelineManager::setNotificationSettings(const void*& settings)
{
    m_notificationSettings = settings;
}

void* CIPipelineManager::getPipelineReport(const std::string& pipelineId) const
{
    auto it = m_pipelines.find(pipelineId);
    if (it == m_pipelines.end()) {
        return void*();
    }
    
    const Pipeline& pipeline = it->second;
    void* report;
    report["id"] = pipelineId;
    report["name"] = pipeline.name;
    report["status"] = static_cast<int>(pipeline.status);
    report["startTime"] = pipeline.startTime.toString(//ISODate);
    report["endTime"] = pipeline.endTime.toString(//ISODate);
    
    if (pipeline.process) {
        report["exitCode"] = pipeline.process->exitCode();
    }
    
    return report;
}

