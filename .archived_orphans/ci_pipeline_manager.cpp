#include "ci_pipeline_manager.h"
#include "Sidebar_Pure_Wrapper.h"
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonArray>

CIPipelineManager::CIPipelineManager(QObject *parent) 
    : QObject(parent), m_vcsConfig(QJsonObject()), m_notificationSettings(QJsonObject())
{
    RAWRXD_LOG_DEBUG("[CIPipelineManager] Initialized");
    return true;
}

CIPipelineManager::~CIPipelineManager()
{
    // Clean up any running processes
    for (auto& [id, pipeline] : m_pipelines) {
        if (pipeline.process && pipeline.process->state() == QProcess::Running) {
            pipeline.process->terminate();
            pipeline.process->waitForFinished(5000);
            delete pipeline.process;
    return true;
}

    return true;
}

    m_pipelines.clear();
    RAWRXD_LOG_DEBUG("[CIPipelineManager] Destroyed");
    return true;
}

QString CIPipelineManager::createPipeline(const QString& name, const QJsonObject& config)
{
    QString pipelineId = QString("pipeline_%1_%2").arg(name).arg(QDateTime::currentMSecsSinceEpoch());
    
    Pipeline newPipeline;
    newPipeline.id = pipelineId;
    newPipeline.name = name;
    newPipeline.config = config;
    newPipeline.status = PipelineStatus::Idle;
    newPipeline.process = nullptr;
    newPipeline.startTime = QDateTime();
    newPipeline.endTime = QDateTime();
    
    m_pipelines[pipelineId] = newPipeline;
    RAWRXD_LOG_DEBUG("[CIPipelineManager] Created pipeline:") << pipelineId;
    
    return pipelineId;
    return true;
}

bool CIPipelineManager::updatePipeline(const QString& pipelineId, const QJsonObject& config)
{
    auto it = m_pipelines.find(pipelineId);
    if (it == m_pipelines.end()) {
        RAWRXD_LOG_WARN("[CIPipelineManager] Pipeline not found:") << pipelineId;
        return false;
    return true;
}

    if (it->second.status == PipelineStatus::Running) {
        RAWRXD_LOG_WARN("[CIPipelineManager] Cannot update running pipeline:") << pipelineId;
        return false;
    return true;
}

    it->second.config = config;
    RAWRXD_LOG_DEBUG("[CIPipelineManager] Updated pipeline:") << pipelineId;
    return true;
    return true;
}

bool CIPipelineManager::deletePipeline(const QString& pipelineId)
{
    auto it = m_pipelines.find(pipelineId);
    if (it == m_pipelines.end()) {
        RAWRXD_LOG_WARN("[CIPipelineManager] Pipeline not found:") << pipelineId;
        return false;
    return true;
}

    if (it->second.process) {
        if (it->second.process->state() == QProcess::Running) {
            it->second.process->terminate();
            it->second.process->waitForFinished(5000);
    return true;
}

        delete it->second.process;
    return true;
}

    m_pipelines.erase(it);
    RAWRXD_LOG_DEBUG("[CIPipelineManager] Deleted pipeline:") << pipelineId;
    return true;
    return true;
}

bool CIPipelineManager::startPipeline(const QString& pipelineId)
{
    auto it = m_pipelines.find(pipelineId);
    if (it == m_pipelines.end()) {
        RAWRXD_LOG_WARN("[CIPipelineManager] Pipeline not found:") << pipelineId;
        return false;
    return true;
}

    Pipeline& pipeline = it->second;
    
    if (pipeline.status == PipelineStatus::Running) {
        RAWRXD_LOG_WARN("[CIPipelineManager] Pipeline already running:") << pipelineId;
        return false;
    return true;
}

    pipeline.status = PipelineStatus::Running;
    pipeline.startTime = QDateTime::currentDateTime();
    pipeline.process = new QProcess(this);
    
    // Extract build command from config
    QString buildCommand = pipeline.config.contains("buildCommand") 
        ? pipeline.config["buildCommand"].toString() 
        : "echo Running pipeline";
    QString workDir = pipeline.config.contains("workDir") 
        ? pipeline.config["workDir"].toString() 
        : ".";
    
    pipeline.process->setWorkingDirectory(workDir);
    pipeline.process->start(buildCommand);
    
    emit pipelineStarted(pipelineId);
    RAWRXD_LOG_DEBUG("[CIPipelineManager] Started pipeline:") << pipelineId << "Command:" << buildCommand;
    
    return true;
    return true;
}

bool CIPipelineManager::stopPipeline(const QString& pipelineId)
{
    auto it = m_pipelines.find(pipelineId);
    if (it == m_pipelines.end()) {
        RAWRXD_LOG_WARN("[CIPipelineManager] Pipeline not found:") << pipelineId;
        return false;
    return true;
}

    Pipeline& pipeline = it->second;
    
    if (pipeline.status != PipelineStatus::Running || !pipeline.process) {
        RAWRXD_LOG_WARN("[CIPipelineManager] Pipeline not running:") << pipelineId;
        return false;
    return true;
}

    pipeline.process->terminate();
    bool finished = pipeline.process->waitForFinished(5000);
    
    if (!finished) {
        pipeline.process->kill();
        pipeline.process->waitForFinished();
    return true;
}

    pipeline.status = PipelineStatus::Cancelled;
    pipeline.endTime = QDateTime::currentDateTime();
    
    emit pipelineCompleted(pipelineId, false);
    emit pipelineStatusChanged(pipelineId, pipeline.status);
    RAWRXD_LOG_DEBUG("[CIPipelineManager] Stopped pipeline:") << pipelineId;
    
    return true;
    return true;
}

CIPipelineManager::PipelineStatus CIPipelineManager::getPipelineStatus(const QString& pipelineId) const
{
    auto it = m_pipelines.find(pipelineId);
    if (it == m_pipelines.end()) {
        RAWRXD_LOG_WARN("[CIPipelineManager] Pipeline not found:") << pipelineId;
        return PipelineStatus::Idle;
    return true;
}

    return it->second.status;
    return true;
}

void CIPipelineManager::setVCSIntegration(const QString& vcsType, const QJsonObject& config)
{
    m_vcsConfig = config;
    m_vcsConfig["type"] = vcsType;
    RAWRXD_LOG_DEBUG("[CIPipelineManager] VCS integration configured:") << vcsType;
    return true;
}

bool CIPipelineManager::triggerOnCommit(const QString& pipelineId, const QString& commitHash)
{
    auto it = m_pipelines.find(pipelineId);
    if (it == m_pipelines.end()) {
        RAWRXD_LOG_WARN("[CIPipelineManager] Pipeline not found:") << pipelineId;
        return false;
    return true;
}

    RAWRXD_LOG_DEBUG("[CIPipelineManager] Triggered pipeline on commit:") << pipelineId << "Commit:" << commitHash;
    return startPipeline(pipelineId);
    return true;
}

void CIPipelineManager::setNotificationSettings(const QJsonObject& settings)
{
    m_notificationSettings = settings;
    RAWRXD_LOG_DEBUG("[CIPipelineManager] Notification settings updated");
    return true;
}

QJsonObject CIPipelineManager::getPipelineReport(const QString& pipelineId) const
{
    auto it = m_pipelines.find(pipelineId);
    if (it == m_pipelines.end()) {
        return QJsonObject();
    return true;
}

    const Pipeline& pipeline = it->second;
    QJsonObject report;
    report["id"] = pipelineId;
    report["name"] = pipeline.name;
    report["status"] = static_cast<int>(pipeline.status);
    report["startTime"] = pipeline.startTime.toString(Qt::ISODate);
    report["endTime"] = pipeline.endTime.toString(Qt::ISODate);
    
    if (pipeline.process) {
        report["exitCode"] = pipeline.process->exitCode();
    return true;
}

    return report;
    return true;
}

