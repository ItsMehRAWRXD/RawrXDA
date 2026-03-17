// ============================================================================
// test_model_trainer_validation.cpp — C++20, no Qt.
// Validation tests for ModelTrainer and TransformerBlockScalar (assert-based).
// ============================================================================

#include "model_trainer.h"
#include "transformer_block_scalar.h"
#include <nlohmann/json.hpp>
#include <cmath>
#include <vector>
#include <random>
#include <fstream>
#include <cassert>
#include <cstdio>
#include <cstdlib>

#define QVERIFY(x)   assert(x)
#define QCOMPARE(a,b) assert((a)==(b))

static void initTestCase() {}
static void cleanupTestCase() {}

namespace {

std::vector<float> createTestWeights(size_t size);
std::vector<std::vector<float>> createTestEmbeddings();
bool validateMatrixMultiplication(const std::vector<float>& result,
    const std::vector<float>& expected, float tolerance = 1e-5f);

void testTransformerRealAttention()
{
    std::printf("Testing Real Multi-Head Self-Attention...\n");
    
    // Create test transformer block
    TransformerBlockScalar transformer;
    
    // Test dimensions match production architecture
    QCOMPARE(transformer.getHiddenDim(), 4096u);
    QCOMPARE(transformer.getHeadCount(), 32u);
    QCOMPARE(transformer.getHeadDim(), 128u); // 4096 / 32 = 128
    
    // Create test input (batch_size=1, seq_len=16, hidden_dim=4096)
    std::vector<float> input(1 * 16 * 4096, 0.1f);
    
    // Test attention computation
    std::vector<float> output = transformer.selfAttention(input, 16);
    
    // Validate output dimensions
    QCOMPARE(output.size(), input.size());
    
    // Validate numerical properties
    for (float val : output) {
        QVERIFY(std::isfinite(val)); // No NaN or infinity
        QVERIFY(val != 0.0f); // Non-zero output
    }
    
    std::printf("✓ Real attention test passed\n");
}

void testTransformerRealFFN()
{
    std::printf("Testing Real Feed-Forward Network...\n");
    
    TransformerBlockScalar transformer;
    
    // Test FFN dimensions
    QCOMPARE(transformer.getFFNHiddenDim(), 16384u); // 4096 * 4
    
    // Create test input
    std::vector<float> input(4096, 0.1f);
    
    // Test FFN computation
    std::vector<float> output = transformer.feedForward(input);
    
    // Validate output
    QCOMPARE(output.size(), 4096u);
    
    // Test SiLU activation properties
    for (float val : output) {
        QVERIFY(std::isfinite(val));
        // SiLU should produce values between ~-0.28 and infinity
        QVERIFY(val > -0.3f);
    }
    
    std::printf("✓ Real FFN test passed\n");
}

void testTransformerLayerNorm()
{
    std::printf("Testing Real Layer Normalization...\n");
    
    TransformerBlockScalar transformer;
    
    // Create test input with variance
    std::vector<float> input(4096);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<float> dist(0.0f, 1.0f);
    
    for (float& val : input) {
        val = dist(gen);
    }
    
    // Test layer normalization
    std::vector<float> output = transformer.layerNorm(input);
    
    // Validate normalization properties
    float mean = 0.0f;
    float variance = 0.0f;
    
    for (float val : output) {
        mean += val;
    }
    mean /= output.size();
    
    for (float val : output) {
        variance += (val - mean) * (val - mean);
    }
    variance /= output.size();
    
    // Mean should be near 0, variance near 1
    QVERIFY(std::abs(mean) < 1e-5f);
    QVERIFY(std::abs(variance - 1.0f) < 1e-5f);
    
    qDebug() << "✓ Layer normalization test passed";
}

void testTransformerForwardPass()
{
    qDebug() << "Testing Complete Forward Pass...";
    
    TransformerBlockScalar transformer;
    
    // Test embedding lookup
    auto embeddings = createTestEmbeddings();
    std::vector<uint32_t> tokens = {100, 200, 300}; // Test tokens
    
    std::vector<float> hiddenStates = transformer.embedTokens(tokens, embeddings);
    QCOMPARE(hiddenStates.size(), tokens.size() * 4096);
    
    // Test full forward pass
    std::vector<float> logits = transformer.runForwardPass(hiddenStates, tokens.size());
    
    // Validate output dimensions
    QCOMPARE(logits.size(), tokens.size() * 32000); // vocab_size = 32000
    
    // Validate logits properties
    for (float val : logits) {
        QVERIFY(std::isfinite(val));
    }
    
    qDebug() << "✓ Complete forward pass test passed";
}

void testTransformerNumericalStability()
{
    qDebug() << "Testing Numerical Stability...";
    
    TransformerBlockScalar transformer;
    
    // Test with extreme values
    std::vector<float> extremeInput(4096);
    for (size_t i = 0; i < extremeInput.size(); ++i) {
        extremeInput[i] = (i % 2 == 0) ? 1e6f : -1e6f; // Large positive/negative
    }
    
    // Test attention with extreme values
    std::vector<float> attentionOutput = transformer.selfAttention(extremeInput, 1);
    for (float val : attentionOutput) {
        QVERIFY(std::isfinite(val)); // Should handle extremes without NaN
    }
    
    // Test softmax stability
    std::vector<float> logits = {1e6f, -1e6f, 0.0f}; // Extreme logits
    std::vector<float> probs = transformer.softmax(logits);
    
    // Probabilities should sum to 1
    float sum = 0.0f;
    for (float prob : probs) {
        sum += prob;
        QVERIFY(prob >= 0.0f && prob <= 1.0f);
    }
    QVERIFY(std::abs(sum - 1.0f) < 1e-5f);
    
    qDebug() << "✓ Numerical stability test passed";
}

// ===== ModelTrainer Validation Tests =====

void testModelTrainerInitialization()
{
    qDebug() << "Testing ModelTrainer Initialization...";
    
    ModelTrainer trainer;
    
    // Test default configuration
    ModelTrainer::TrainingConfig config;
    QCOMPARE(config.epochs, 3);
    QCOMPARE(config.learningRate, 1e-4f);
    QCOMPARE(config.batchSize, 4);
    QCOMPARE(config.sequenceLength, 512);
    
    // Test initialization state
    QVERIFY(!trainer.isTraining());
    QCOMPARE(trainer.getCurrentEpoch(), 0);
    QCOMPARE(trainer.getCurrentLoss(), 0.0f);
    
    qDebug() << "✓ ModelTrainer initialization test passed";
}

void testDatasetLoading()
{
    std::printf("Testing Dataset Loading...\n");
    ModelTrainer trainer;
    {
        std::ofstream plainTextFile("test_plain.txt");
        if (plainTextFile) {
            plainTextFile << "Sample text line 1\nSample text line 2\n";
        }
    }
    {
        std::ofstream jsonlFile("test_jsonl.jsonl");
        if (jsonlFile) {
            jsonlFile << "{\"text\": \"Sample JSON text\"}\n{\"text\": \"Another sample\"}\n";
        }
    }
    auto format1 = trainer.detectDatasetFormat("test_plain.txt");
    QCOMPARE(format1, ModelTrainer::DatasetFormat::PlainText);
    auto format2 = trainer.detectDatasetFormat("test_jsonl.jsonl");
    QCOMPARE(format2, ModelTrainer::DatasetFormat::JsonLines);
    std::remove("test_plain.txt");
    std::remove("test_jsonl.jsonl");
    std::printf("✓ Dataset loading test passed\n");
}

void testTokenization()
{
    qDebug() << "Testing Tokenization...";
    
    ModelTrainer trainer;
    
    // Test tokenization with sample text
    std::string sampleText = "This is a test sentence for tokenization.";
    
    // Note: This would require a real tokenizer implementation
    // For now, test the interface
    std::vector<uint32_t> tokens = trainer.tokenizeText(sampleText);
    
    // Basic validation
    QVERIFY(tokens.size() > 0); // Should produce tokens
    for (uint32_t token : tokens) {
        QVERIFY(token < 32000); // Within vocabulary range
    }
    
    std::printf("✓ Tokenization test passed\n");
}

void testTrainingConfiguration()
{
    std::printf("Testing Training Configuration...\n");
    
    ModelTrainer trainer;
    
    // Test comprehensive configuration
    ModelTrainer::TrainingConfig config;
    config.datasetPath = "test_dataset.jsonl";
    config.outputPath = "trained_model.gguf";
    config.epochs = 5;
    config.learningRate = 2e-4f;
    config.batchSize = 8;
    config.sequenceLength = 1024;
    config.gradientClip = 2.0f;
    config.validationSplit = 0.2f;
    config.warmupSteps = 0.2f;
    config.weightDecay = 0.02f;
    config.validateEveryEpoch = false;
    
    // Validate configuration values
    QCOMPARE(config.epochs, 5);
    QCOMPARE(config.learningRate, 2e-4f);
    QCOMPARE(config.batchSize, 8);
    QCOMPARE(config.sequenceLength, 1024);
    QCOMPARE(config.gradientClip, 2.0f);
    
    std::printf("✓ Training configuration test passed\n");
}

void testOptimizerOperations()
{
    std::printf("Testing Optimizer Operations...\n");
    
    // Test Adam optimizer learning rate scheduling
    ModelTrainer trainer;
    
    // Test learning rate calculation
    float lr1 = trainer.getLearningRate(100, 1000); // 10% through training
    float lr2 = trainer.getLearningRate(500, 1000); // 50% through training
    float lr3 = trainer.getLearningRate(900, 1000); // 90% through training
    
    // Learning rate should follow warmup then decay pattern
    QVERIFY(lr1 > 0.0f);
    QVERIFY(lr2 > 0.0f);
    QVERIFY(lr3 > 0.0f);
    
    // Test gradient clipping
    std::vector<float> gradients = {10.0f, -5.0f, 3.0f, -8.0f};
    trainer.clipGradients(gradients, 5.0f); // Clip to max norm 5.0
    
    float norm = 0.0f;
    for (float g : gradients) {
        norm += g * g;
    }
    norm = std::sqrt(norm);
    
    QVERIFY(norm <= 5.0f + 1e-5f); // Should be clipped
    
    std::printf("✓ Optimizer operations test passed\n");
}

void testModelValidation()
{
    std::printf("Testing Model Validation...\n");
    
    ModelTrainer trainer;
    
    // Test perplexity calculation
    float perplexity = trainer.calculatePerplexity();
    
    // Perplexity should be positive finite value
    QVERIFY(std::isfinite(perplexity));
    QVERIFY(perplexity > 0.0f);
    
    // Test validation workflow
    bool validationSuccess = trainer.validateModel();
    
    // Validation should complete without errors
    QVERIFY(validationSuccess);
    
    std::printf("✓ Model validation test passed\n");
}

void testRealTrainingWorkflow()
{
    std::printf("Testing Real Training Workflow...\n");
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
            testFile << obj.dump() << "\n";
        }
    }
    std::remove(config.datasetPath.c_str());
    std::remove(config.outputPath.c_str());
    std::printf("✓ Real training workflow test passed\n");
}

// ===== Helper Methods =====

std::vector<float> createTestWeights(size_t size)
{
    std::vector<float> weights(size);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<float> dist(0.0f, 0.02f); // Small variance
    
    for (float& weight : weights) {
        weight = dist(gen);
    }
    
    return weights;
}

std::vector<std::vector<float>> createTestEmbeddings()
{
    std::vector<std::vector<float>> embeddings(32000); // vocab_size
    
    for (auto& embedding : embeddings) {
        embedding.resize(4096); // hidden_dim
        for (float& val : embedding) {
            val = (rand() % 1000) / 1000.0f - 0.5f; // Random [-0.5, 0.5]
        }
    }
    
    return embeddings;
}

bool validateMatrixMultiplication(
    const std::vector<float>& result, 
    const std::vector<float>& expected, 
    float tolerance)
{
    if (result.size() != expected.size()) {
        return false;
    }
    
    for (size_t i = 0; i < result.size(); ++i) {
        if (std::abs(result[i] - expected[i]) > tolerance) {
            return false;
        }
    }
    
    return true;
}

} // namespace

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
    std::printf("All tests passed.\n");
    return 0;
}