# Fix remaining Qt in test_model_trainer_validation.cpp
path = "test_model_trainer_validation.cpp"
with open(path, "r", encoding="utf-8") as f:
    c = f.read()

# testRealTrainingWorkflow: replace Qt block
old1 = """void testRealTrainingWorkflow()
{
    qDebug() << "Testing Real Training Workflow...";
    
    ModelTrainer trainer;
    
    // Connect to signals for monitoring
    QSignalSpy startedSpy(&trainer, &ModelTrainer::trainingStarted);
    QSignalSpy errorSpy(&trainer, &ModelTrainer::trainingError);
    QSignalSpy completedSpy(&trainer, &ModelTrainer::trainingCompleted);
    
    // Create minimal test configuration
    ModelTrainer::TrainingConfig config;
    config.datasetPath = "minimal_test.jsonl";
    config.outputPath = "test_output.gguf";
    config.epochs = 1;
    config.batchSize = 2;
    config.sequenceLength = 64;
    
    // Create minimal test dataset
    QFile testFile(config.datasetPath);
    if (testFile.open(QIODevice::WriteOnly)) {
        for (int i = 0; i < 10; ++i) {
            QJsonObject obj;
            obj["text"] = QString("Test sample %1").arg(i);
            testFile.write(QJsonDocument(obj).toJson(QJsonDocument::Compact) + "\\n");
        }
        testFile.close();
    }
    
    // Note: This would require a real InferenceEngine to be fully tested
    // For now, test the interface and signal connections
    
    // Cleanup test file
    QFile::remove(config.datasetPath);
    QFile::remove(config.outputPath);
    
    qDebug() << "✓ Real training workflow test passed";
}"""

new1 = """void testRealTrainingWorkflow()
{
    std::printf("Testing Real Training Workflow...\\n");
    ModelTrainer trainer;
    ModelTrainer::TrainingConfig config;
    config.datasetPath = "minimal_test.jsonl";
    config.outputPath = "test_output.gguf";
    config.epochs = 1;
    config.batchSize = 2;
    config.sequenceLength = 64;
    std::ofstream testFile(config.datasetPath);
    if (testFile) {
        for (int i = 0; i < 10; ++i) {
            nlohmann::json obj;
            obj["text"] = "Test sample " + std::to_string(i);
            testFile << obj.dump() << "\\n";
        }
    }
    std::remove(config.datasetPath.c_str());
    std::remove(config.outputPath.c_str());
    std::printf("✓ Real training workflow test passed\\n");
}"""

old2 = """QTEST_MAIN(TestModelTrainerValidation)
#include "test_model_trainer_validation.moc\""""

new2 = """} // namespace

int main()
{
    initTestCase();
    testTransformerRealAttention();
    testTransformerRealFFN();
    testTransformerLayerNorm();
    testTransformerForwardPass();
    testTransformerNumericalStability();
    testModelTrainerInitialization();
    testDatasetLoading();
    testTokenization();
    testTrainingConfiguration();
    testOptimizerOperations();
    testModelValidation();
    testRealTrainingWorkflow();
    cleanupTestCase();
    std::printf("All tests passed.\\n");
    return 0;
}"""

if old1 in c:
    c = c.replace(old1, new1)
else:
    print("old1 not found")
if old2 in c:
    c = c.replace(old2, new2)
else:
    print("old2 not found")
# Replace any remaining qDebug
import re
c = re.sub(r'qDebug\(\) << "([^"]+)";', r'std::printf("\1\\n");', c)

with open(path, "w", encoding="utf-8") as f:
    f.write(c)
print("Done")
