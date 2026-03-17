#include <QtTest/QtTest>
#include "../digestion_config_manager.h"

class DigestionConfigTests : public QObject {
    Q_OBJECT
private slots:
    void loadJsonConfig();
    void loadYamlConfig();
};

void DigestionConfigTests::loadJsonConfig() {
    QJsonObject json;
    json["digestion"] = QJsonObject{
        {"chunk_size", 1024},
        {"threads", 2},
        {"apply_fixes", true},
        {"flags", QJsonArray{"DEFLATE", "CRC"}}
    };
    json["database"] = QJsonObject{
        {"path", "test.db"},
        {"enabled", true}
    };

    QString error;
    DigestionModuleConfig cfg = DigestionConfigManager::loadFromJson(json, &error);
    QVERIFY(error.isEmpty());
    QCOMPARE(cfg.engineConfig.chunkSize, 1024);
    QCOMPARE(cfg.engineConfig.threadCount, 2);
    QCOMPARE(cfg.engineConfig.applyExtensions, true);
    QCOMPARE(cfg.flags.size(), 2);
    QCOMPARE(cfg.databasePath, QString("test.db"));
}

void DigestionConfigTests::loadYamlConfig() {
    const QString yaml = QStringLiteral(
        "digestion:\n"
        "  chunk_size: 2048\n"
        "  threads: 4\n"
        "  flags: [DEFLATE, CRC]\n"
        "database:\n"
        "  path: digestion_results.db\n"
        "  enabled: true\n");

    QString error;
    DigestionModuleConfig cfg = DigestionConfigManager::loadFromYaml(yaml, &error);
    QVERIFY(error.isEmpty());
    QCOMPARE(cfg.engineConfig.chunkSize, 2048);
    QCOMPARE(cfg.engineConfig.threadCount, 4);
    QCOMPARE(cfg.flags.size(), 2);
    QCOMPARE(cfg.databasePath, QString("digestion_results.db"));
}

QTEST_MAIN(DigestionConfigTests)
#include "digestion_config_tests.moc"
