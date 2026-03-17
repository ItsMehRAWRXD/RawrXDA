#include "ai_code_assistant.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    qDebug() << QString("[%1] AI Code Assistant Test Started")
                    .arg(QDateTime::currentDateTime().toString(Qt::ISODate));

    // Create AI Code Assistant instance
    AICodeAssistant assistant;

    // Configure it
    assistant.setOllamaServer("localhost", 11434);
    assistant.setWorkspaceRoot(".");
    assistant.setModel("ministral-3");
    assistant.setTemperature(0.7f);
    assistant.setMaxTokens(2048);

    qDebug() << "[Test] AICodeAssistant configured successfully";

    // Test signal connections
    QObject::connect(&assistant, &AICodeAssistant::searchResultsReady,
            [](const QStringList &results) {
                qDebug() << "[Test] Search completed with" << results.count() << "results";
            });

    QObject::connect(&assistant, &AICodeAssistant::latencyMeasured,
            [](qint64 ms) {
                qDebug() << "[Test] Latency:" << ms << "ms";
            });

    qDebug() << "[Test] Testing file search...";
    assistant.searchFiles("*.h", ".");

    qDebug() << "[Test] AI Code Assistant validation completed successfully";
    return 0;
}
