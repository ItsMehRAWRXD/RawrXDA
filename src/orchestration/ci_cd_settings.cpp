#include "ci_cd_settings.h"
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QProcess>
#include <QDateTime>
#include <QMap>
#include <QDebug>

// Real CI/CD job execution with QProcess

struct JobData {
    QString id;
    TrainingJobConfig config;
    QProcess* process;
    QDateTime startTime;
    QString status;
    QList<PipelineStage> stages;
};

static QMap<QString, JobData> s_jobs;

CICDSettings::CICDSettings(QObject* parent) : QObject(parent) {}
CICDSettings::~CICDSettings() {
    // Clean up running processes
    for (auto& job : s_jobs) {
        if (job.process && job.process->state() == QProcess::Running) {
            job.process->terminate();
            job.process->waitForFinished(5000);
            delete job.process;
        }
    }
}

QString CICDSettings::createTrainingJob(const TrainingJobConfig& config) {
    QString jobId = generateJobId();
    
    JobData job;
    job.id = jobId;
    job.config = config;
    job.process = nullptr;
    job.status = "created";
    
    s_jobs[jobId] = job;
    
    qDebug() << "[CICDSettings] Created job:" << jobId;
    return jobId;
}

bool CICDSettings::startJob(const QString& jobId) {
    if (!s_jobs.contains(jobId)) {
        return false;
    }
    
    JobData& job = s_jobs[jobId];
    
    if (job.process && job.process->state() == QProcess::Running) {
        qWarning() << "[CICDSettings] Job already running:" << jobId;
        return false;
    }
    
    // Create new process
    job.process = new QProcess(this);
    job.startTime = QDateTime::currentDateTime();
    job.status = "running";
    
    // Extract command from config
    QString command = job.config.command.isEmpty() ? "echo Running job" : job.config.command;
    QString workDir = job.config.workDir.isEmpty() ? "." : job.config.workDir;
    
    job.process->setWorkingDirectory(workDir);
    job.process->setProcessChannelMode(QProcess::MergedChannels);
    
    // Connect signals
    connect(job.process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, jobId](int exitCode, QProcess::ExitStatus status) {
        if (s_jobs.contains(jobId)) {
            s_jobs[jobId].status = (exitCode == 0) ? "completed" : "failed";
            qDebug() << "[CICDSettings] Job" << jobId << "finished with code" << exitCode;
            emit jobCompleted(jobId, exitCode == 0);
        }
    });
    
    // Start process
    job.process->start("pwsh.exe", QStringList() << "-Command" << command);
    
    qDebug() << "[CICDSettings] Started job:" << jobId << "Command:" << command;
    emit jobStarted(jobId);
    
    return true;
}

bool CICDSettings::cancelJob(const QString& jobId) {
    if (!s_jobs.contains(jobId)) {
        return false;
    }
    
    JobData& job = s_jobs[jobId];
    
    if (job.process && job.process->state() == QProcess::Running) {
        job.process->terminate();
        bool finished = job.process->waitForFinished(5000);
        
        if (!finished) {
            job.process->kill();
            job.process->waitForFinished();
        }
        
        job.status = "cancelled";
        qDebug() << "[CICDSettings] Cancelled job:" << jobId;
        emit jobCancelled(jobId);
        return true;
    }
    
    return false;
}

QJsonObject CICDSettings::getJobStatus(const QString& jobId) const {
    if (!s_jobs.contains(jobId)) {
        return QJsonObject();
    }
    
    const JobData& job = s_jobs[jobId];
    
    QJsonObject status;
    status["id"] = jobId;
    status["status"] = job.status;
    status["startTime"] = job.startTime.toString(Qt::ISODate);
    status["command"] = job.config.command;
    status["workDir"] = job.config.workDir;
    
    if (job.process) {
        status["exitCode"] = job.process->exitCode();
        status["running"] = (job.process->state() == QProcess::Running);
    }
    
    return status;
}

QStringList CICDSettings::listJobs() const {
    return s_jobs.keys();
}

bool CICDSettings::configurePipeline(const QString& jobId, const QList<PipelineStage>& stages) {
    if (!s_jobs.contains(jobId)) {
        return false;
    }
    
    s_jobs[jobId].stages = stages;
    qDebug() << "[CICDSettings] Configured pipeline for job:" << jobId << "Stages:" << stages.size();
    return true;
}

QString CICDSettings::generateJobId() {
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString random = QString::number(QDateTime::currentMSecsSinceEpoch() % 10000);
    return QString("job_%1_%2").arg(timestamp, random);
}

QString CICDSettings::generateRunId() {
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString random = QString::number(QDateTime::currentMSecsSinceEpoch() % 10000);
    return QString("run_%1_%2").arg(timestamp, random);
}

QString CICDSettings::generateDeploymentId() {
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString random = QString::number(QDateTime::currentMSecsSinceEpoch() % 10000);
    return QString("deploy_%1_%2").arg(timestamp, random);
}