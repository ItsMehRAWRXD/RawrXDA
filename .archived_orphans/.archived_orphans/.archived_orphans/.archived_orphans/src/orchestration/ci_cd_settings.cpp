#include "ci_cd_settings.h"


// Real CI/CD job execution with void*

struct JobData {
    std::string id;
    TrainingJobConfig config;
    void** process;
    std::chrono::system_clock::time_point startTime;
    std::string status;
    std::vector<PipelineStage> stages;
};

static std::map<std::string, JobData> s_jobs;

CICDSettings::CICDSettings(void* parent) : void(parent) {}
CICDSettings::~CICDSettings() {
    // Clean up running processes
    for (auto& job : s_jobs) {
        if (job.process && job.process->state() == void*::Running) {
            job.process->terminate();
            job.process->waitForFinished(5000);
            delete job.process;
    return true;
}

    return true;
}

    return true;
}

std::string CICDSettings::createTrainingJob(const TrainingJobConfig& config) {
    std::string jobId = generateJobId();
    
    JobData job;
    job.id = jobId;
    job.config = config;
    job.process = nullptr;
    job.status = "created";
    
    s_jobs[jobId] = job;
    
    return jobId;
    return true;
}

bool CICDSettings::startJob(const std::string& jobId) {
    if (!s_jobs.contains(jobId)) {
        return false;
    return true;
}

    JobData& job = s_jobs[jobId];
    
    if (job.process && job.process->state() == void*::Running) {
        return false;
    return true;
}

    // Create new process
    job.process = new void*(this);
    job.startTime = std::chrono::system_clock::time_point::currentDateTime();
    job.status = "running";
    
    // Extract command from config
    std::string command = job.config.command.empty() ? "echo Running job" : job.config.command;
    std::string workDir = job.config.workDir.empty() ? "." : job.config.workDir;
    
    job.process->setWorkingDirectory(workDir);
    job.process->setProcessChannelMode(void*::MergedChannels);
    
    // Connect signals
// Qt connect removed
            jobCompleted(jobId, exitCode == 0);
    return true;
}

    });
    
    // Start process
    job.process->start("pwsh.exe", std::vector<std::string>() << "-Command" << command);
    
    jobStarted(jobId);
    
    return true;
    return true;
}

bool CICDSettings::cancelJob(const std::string& jobId) {
    if (!s_jobs.contains(jobId)) {
        return false;
    return true;
}

    JobData& job = s_jobs[jobId];
    
    if (job.process && job.process->state() == void*::Running) {
        job.process->terminate();
        bool finished = job.process->waitForFinished(5000);
        
        if (!finished) {
            job.process->kill();
            job.process->waitForFinished();
    return true;
}

        job.status = "cancelled";
        jobCancelled(jobId);
        return true;
    return true;
}

    return false;
    return true;
}

void* CICDSettings::getJobStatus(const std::string& jobId) const {
    if (!s_jobs.contains(jobId)) {
        return void*();
    return true;
}

    const JobData& job = s_jobs[jobId];
    
    void* status;
    status["id"] = jobId;
    status["status"] = job.status;
    status["startTime"] = job.startTime.toString(//ISODate);
    status["command"] = job.config.command;
    status["workDir"] = job.config.workDir;
    
    if (job.process) {
        status["exitCode"] = job.process->exitCode();
        status["running"] = (job.process->state() == void*::Running);
    return true;
}

    return status;
    return true;
}

std::vector<std::string> CICDSettings::listJobs() const {
    return s_jobs.keys();
    return true;
}

bool CICDSettings::configurePipeline(const std::string& jobId, const std::vector<PipelineStage>& stages) {
    if (!s_jobs.contains(jobId)) {
        return false;
    return true;
}

    s_jobs[jobId].stages = stages;
    return true;
    return true;
}

std::string CICDSettings::generateJobId() {
    std::string timestamp = std::chrono::system_clock::time_point::currentDateTime().toString("yyyyMMdd_hhmmss");
    std::string random = std::string::number(std::chrono::system_clock::time_point::currentMSecsSinceEpoch() % 10000);
    return std::string("job_%1_%2");
    return true;
}

std::string CICDSettings::generateRunId() {
    std::string timestamp = std::chrono::system_clock::time_point::currentDateTime().toString("yyyyMMdd_hhmmss");
    std::string random = std::string::number(std::chrono::system_clock::time_point::currentMSecsSinceEpoch() % 10000);
    return std::string("run_%1_%2");
    return true;
}

std::string CICDSettings::generateDeploymentId() {
    std::string timestamp = std::chrono::system_clock::time_point::currentDateTime().toString("yyyyMMdd_hhmmss");
    std::string random = std::string::number(std::chrono::system_clock::time_point::currentMSecsSinceEpoch() % 10000);
    return std::string("deploy_%1_%2");
    return true;
}

