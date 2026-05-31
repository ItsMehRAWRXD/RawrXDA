#include "ci_pipeline_manager.h"
#include <QDebug>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonArray>

CIPipelineManager::CIPipelineManager(QObject *parent) 
    : QObject(parent), m_vcsConfig(QJsonObject()), m_notificationSettings(QJsonObject())
{
    qDebug() << "[CIPipelineManager] Initialized";
}

CIPipelineManager::~CIPipelineManager()
{
    // Clean up any running processes
    for (auto& [id, pipeline] : m_pipelines) {
        if (pipeline.process && pipeline.process->state() == QProcess::Running) {
            pipeline.process->terminate();
            pipeline.process->waitForFinished(5000);
            delete pipeline.process;
        }
    }
    m_pipelines.clear();
    qDebug() << "[CIPipelineManager] Destroyed";
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
    qDebug() << "[CIPipelineManager] Created pipeline:" << pipelineId;
    
    return pipelineId;
}

bool CIPipelineManager::updatePipeline(const QString& pipelineId, const QJsonObject& config)
{
    auto it = m_pipelines.find(pipelineId);
    if (it == m_pipelines.end()) {
        qWarning() << "[CIPipelineManager] Pipeline not found:" << pipelineId;
        return false;
    }
    
    if (it->second.status == PipelineStatus::Running) {
        qWarning() << "[CIPipelineManager] Cannot update running pipeline:" << pipelineId;
        return false;
    }
    
    it->second.config = config;
    qDebug() << "[CIPipelineManager] Updated pipeline:" << pipelineId;
    return true;
}

bool CIPipelineManager::deletePipeline(const QString& pipelineId)
{
    auto it = m_pipelines.find(pipelineId);
    if (it == m_pipelines.end()) {
        qWarning() << "[CIPipelineManager] Pipeline not found:" << pipelineId;
        return false;
    }
    
    if (it->second.process) {
        if (it->second.process->state() == QProcess::Running) {
            it->second.process->terminate();
            it->second.process->waitForFinished(5000);
        }
        delete it->second.process;
    }
    
    m_pipelines.erase(it);
    qDebug() << "[CIPipelineManager] Deleted pipeline:" << pipelineId;
    return true;
}

bool CIPipelineManager::startPipeline(const QString& pipelineId)
{
    auto it = m_pipelines.find(pipelineId);
    if (it == m_pipelines.end()) {
        qWarning() << "[CIPipelineManager] Pipeline not found:" << pipelineId;
        return false;
    }
    
    Pipeline& pipeline = it->second;
    
    if (pipeline.status == PipelineStatus::Running) {
        qWarning() << "[CIPipelineManager] Pipeline already running:" << pipelineId;
        return false;
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
    qDebug() << "[CIPipelineManager] Started pipeline:" << pipelineId << "Command:" << buildCommand;
    
    return true;
}

bool CIPipelineManager::stopPipeline(const QString& pipelineId)
{
    auto it = m_pipelines.find(pipelineId);
    if (it == m_pipelines.end()) {
        qWarning() << "[CIPipelineManager] Pipeline not found:" << pipelineId;
        return false;
    }
    
    Pipeline& pipeline = it->second;
    
    if (pipeline.status != PipelineStatus::Running || !pipeline.process) {
        qWarning() << "[CIPipelineManager] Pipeline not running:" << pipelineId;
        return false;
    }
    
    pipeline.process->terminate();
    bool finished = pipeline.process->waitForFinished(5000);
    
    if (!finished) {
        pipeline.process->kill();
        pipeline.process->waitForFinished();
    }
    
    pipeline.status = PipelineStatus::Cancelled;
    pipeline.endTime = QDateTime::currentDateTime();
    
    emit pipelineCompleted(pipelineId, false);
    emit pipelineStatusChanged(pipelineId, pipeline.status);
    qDebug() << "[CIPipelineManager] Stopped pipeline:" << pipelineId;
    
    return true;
}

CIPipelineManager::PipelineStatus CIPipelineManager::getPipelineStatus(const QString& pipelineId) const
{
    auto it = m_pipelines.find(pipelineId);
    if (it == m_pipelines.end()) {
        qWarning() << "[CIPipelineManager] Pipeline not found:" << pipelineId;
        return PipelineStatus::Idle;
    }
    
    return it->second.status;
}

void CIPipelineManager::setVCSIntegration(const QString& vcsType, const QJsonObject& config)
{
    m_vcsConfig = config;
    m_vcsConfig["type"] = vcsType;
    qDebug() << "[CIPipelineManager] VCS integration configured:" << vcsType;
}

bool CIPipelineManager::triggerOnCommit(const QString& pipelineId, const QString& commitHash)
{
    auto it = m_pipelines.find(pipelineId);
    if (it == m_pipelines.end()) {
        qWarning() << "[CIPipelineManager] Pipeline not found:" << pipelineId;
        return false;
    }
    
    qDebug() << "[CIPipelineManager] Triggered pipeline on commit:" << pipelineId << "Commit:" << commitHash;
    return startPipeline(pipelineId);
}

void CIPipelineManager::setNotificationSettings(const QJsonObject& settings)
{
    m_notificationSettings = settings;
    qDebug() << "[CIPipelineManager] Notification settings updated";
}

QJsonObject CIPipelineManager::getPipelineReport(const QString& pipelineId) const
{
    auto it = m_pipelines.find(pipelineId);
    if (it == m_pipelines.end()) {
        return QJsonObject();
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
    }
    
    return report;
}
