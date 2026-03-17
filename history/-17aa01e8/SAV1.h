#ifndef MODEL_INTERFACE_H
#define MODEL_INTERFACE_H

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QList>
#include <memory>

struct ModelConfig {
    QString name;
    QString type;
};

struct GenerationOptions {
    int maxTokens;
    double temperature;
};

struct GenerationResult {
    QString text;
    bool success;
};

class ModelInterface : public QObject {
    Q_OBJECT
public:
    ModelInterface(QObject* parent = nullptr);
    ~ModelInterface();

    bool loadModel(const QString& modelPath);
    bool unloadModel();
    bool isModelLoaded() const;
    QJsonObject executeModel(const QString& input);
    QString generateAutonomous(const QString& prompt);
    QJsonArray planTasks(const QString& goal);
    QJsonObject analyzeCode(const QString& code);
    QJsonArray generateTests(const QString& code);
    void setConfiguration(const QJsonObject& config);
    QJsonObject getConfiguration() const;
    QString getModelName() const;
    QString getModelVersion() const;

    // missing for adapter
    bool loadConfig(const QString& path);
    void registerModel(const QString& name, const ModelConfig& config);
    QString selectBestModel(const QString& task, const QString& lang, bool local);
    QString selectCostOptimalModel(const QString& prompt, double budget);
    QString selectFastestModel(const QString& task = "");
    GenerationResult generate(const QString& p, const QString& m, const GenerationOptions& o);
    QList<QString> getAvailableModels() const;
    QJsonObject getUsageStatistics() const;
    double getAverageLatency(const QString& m) const;
    int getSuccessRate(const QString& m) const;
    double getTotalCost() const;
private:
    class Private;
    std::unique_ptr<Private> d;
};

#endif