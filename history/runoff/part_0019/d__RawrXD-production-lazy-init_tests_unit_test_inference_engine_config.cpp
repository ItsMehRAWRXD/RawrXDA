#include <QtTest/QtTest>
#include "../src/qtapp/inference_engine.hpp"
#include "../src/qtapp/settings_manager.h"

class TestInferenceEngineConfig : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testDefaultModelDirectoryUsesSettings();
    void testDefaultModelDirectoryUsesEnvironment();
    void testDefaultModelDirectoryFallsBack();

private:
    QVariant m_savedDefaultPath;
    bool m_hadDefaultPath{false};
};

void TestInferenceEngineConfig::initTestCase()
{
    SettingsManager& settings = SettingsManager::instance();
    m_hadDefaultPath = settings.contains("models/defaultPath");
    if (m_hadDefaultPath) {
        m_savedDefaultPath = settings.getValue("models/defaultPath");
    }
}

void TestInferenceEngineConfig::cleanupTestCase()
{
    SettingsManager& settings = SettingsManager::instance();
    if (m_hadDefaultPath) {
        settings.setValue("models/defaultPath", m_savedDefaultPath);
    } else {
        settings.remove("models/defaultPath");
    }
    qunsetenv("OLLAMA_MODELS");
}

void TestInferenceEngineConfig::testDefaultModelDirectoryUsesSettings()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString configuredPath = QStringLiteral("C:/Config/Models");
    settings.setValue("models/defaultPath", configuredPath);
    qunsetenv("OLLAMA_MODELS");

    QCOMPARE(InferenceEngine::defaultModelDirectory(), configuredPath);

    settings.remove("models/defaultPath");
}

void TestInferenceEngineConfig::testDefaultModelDirectoryUsesEnvironment()
{
    SettingsManager& settings = SettingsManager::instance();
    settings.remove("models/defaultPath");
    qputenv("OLLAMA_MODELS", "  C:/Env/Models  ");

    QCOMPARE(InferenceEngine::defaultModelDirectory(), QStringLiteral("C:/Env/Models"));

    qunsetenv("OLLAMA_MODELS");
}

void TestInferenceEngineConfig::testDefaultModelDirectoryFallsBack()
{
    SettingsManager& settings = SettingsManager::instance();
    settings.remove("models/defaultPath");
    qunsetenv("OLLAMA_MODELS");

    QCOMPARE(InferenceEngine::defaultModelDirectory(), QStringLiteral("D:/OllamaModels"));
}

QTEST_MAIN(TestInferenceEngineConfig)
#include "test_inference_engine_config.moc"
