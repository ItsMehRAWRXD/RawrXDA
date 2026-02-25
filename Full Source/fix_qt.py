import re
path = "test_model_trainer_validation.cpp"
with open(path, "r", encoding="utf-8") as f:
    c = f.read()
c = re.sub(r'qDebug\(\) << "([^"]+)";', r'std::printf("\1\n");', c)
c = c.replace("QTEST_MAIN(TestModelTrainerValidation)\n#include \"test_model_trainer_validation.moc\"",
    "} // namespace\n\nint main() {\n    initTestCase();\n    testTransformerRealAttention();\n    testTransformerRealFFN();\n    testTransformerLayerNorm();\n    testTransformerForwardPass();\n    testTransformerNumericalStability();\n    testModelTrainerInitialization();\n    testDatasetLoading();\n    testTokenization();\n    testTrainingConfiguration();\n    testOptimizerOperations();\n    testModelValidation();\n    testRealTrainingWorkflow();\n    cleanupTestCase();\n    std::printf(\"All tests passed.\\n\");\n    return 0;\n}")
with open(path, "w", encoding="utf-8") as f:
    f.write(c)
print("ok")
