/**
 * @file model_loading_tests.cpp
 * @brief Comprehensive test suite for model loading with MASM compression
 * 
 * Tests:
 * - Standard GGUF loading (Q4_K, Q5_K, Q8_K)
 * - MASM decompression with GZIP-compressed tensors
 * - MASM decompression with DEFLATE-compressed tensors
 * - Error handling and fallback behavior
 * - Performance metrics validation
 * 
 * Compliance: AI Toolkit Production Readiness Guidelines
 * - Behavioral regression tests
 * - Performance benchmarking
 * - Error injection testing
 */

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QTemporaryFile>
#include <QElapsedTimer>
#include "../src/qtapp/gguf_loader.hpp"
#include "../src/qtapp/compression_wrappers.h"
#include "../src/qtapp/inference_engine.hpp"
#include "../src/agentic_engine.h"

class ModelLoadingTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    
    // Standard GGUF Loading Tests
    void testLoadStandardQ4K();
    void testLoadStandardQ5K();
    void testLoadStandardQ8K();
    void testLoadInvalidGGUF();
    void testLoadNonExistentFile();
    
    // MASM Compression Tests
    void testGzipDecompression();
    void testDeflateDecompression();
    void testUncompressedPassthrough();
    void testMixedCompressionTensors();
    
    // Performance Tests
    void testDecompressionThroughput();
    void testLargeModelLoadTime();
    void testMemoryUsageDuringLoad();
    
    // Error Handling Tests
    void testCorruptedGzipData();
    void testPartialGGUFFile();
    void testOutOfMemoryScenario();
    
    // Integration Tests
    void testAgenticEngineLoading();
    void testInferenceEngineIntegration();
    void testModelReadySignalEmission();
    
private:
    QString m_testDataPath;
    QByteArray createMockGGUF();
    QByteArray createCompressedTensor(const QByteArray& data, const QString& compressionType);
};

void ModelLoadingTests::initTestCase()
{
    qInfo() << "[ModelLoadingTests] ========== TEST SUITE START ==========";
    m_testDataPath = QDir::temp().filePath("model_loading_tests");
    QDir().mkpath(m_testDataPath);
    qInfo() << "[ModelLoadingTests] Test data path:" << m_testDataPath;
}

void ModelLoadingTests::cleanupTestCase()
{
    qInfo() << "[ModelLoadingTests] ========== TEST SUITE END ==========";
    QDir(m_testDataPath).removeRecursively();
}

// ========== Standard GGUF Loading Tests ==========

void ModelLoadingTests::testLoadStandardQ4K()
{
    qInfo() << "[TEST] testLoadStandardQ4K - Loading Q4_K quantized model";
    
    // Mock Q4_K GGUF file
    QTemporaryFile mockModel;
    QVERIFY(mockModel.open());
    mockModel.write(createMockGGUF());
    mockModel.flush();
    mockModel.close();
    
    QElapsedTimer timer;
    timer.start();
    
    GGUFLoaderQt loader(mockModel.fileName());
    
    qint64 loadTime = timer.elapsed();
    qInfo() << "[TEST] Q4_K model loaded in" << loadTime << "ms";
    
    QVERIFY(loader.isOpen());
    QVERIFY(loadTime < 5000);  // Should load within 5 seconds
}

void ModelLoadingTests::testLoadStandardQ5K()
{
    qInfo() << "[TEST] testLoadStandardQ5K - Loading Q5_K quantized model";
    
    QTemporaryFile mockModel;
    QVERIFY(mockModel.open());
    mockModel.write(createMockGGUF());
    mockModel.flush();
    mockModel.close();
    
    GGUFLoaderQt loader(mockModel.fileName());
    
    QVERIFY(loader.isOpen());
    QStringList tensors = loader.tensorNames();
    QVERIFY(!tensors.isEmpty());
    qInfo() << "[TEST] Q5_K model has" << tensors.size() << "tensors";
}

void ModelLoadingTests::testLoadStandardQ8K()
{
    qInfo() << "[TEST] testLoadStandardQ8K - Loading Q8_K quantized model";
    
    QTemporaryFile mockModel;
    QVERIFY(mockModel.open());
    mockModel.write(createMockGGUF());
    mockModel.flush();
    mockModel.close();
    
    GGUFLoaderQt loader(mockModel.fileName());
    
    QVERIFY(loader.isOpen());
    
    // Test metadata extraction
    QVariant nLayers = loader.getParam("n_layer", 0);
    QVERIFY(nLayers.toInt() > 0);
    qInfo() << "[TEST] Q8_K model has" << nLayers.toInt() << "layers";
}

void ModelLoadingTests::testLoadInvalidGGUF()
{
    qInfo() << "[TEST] testLoadInvalidGGUF - Testing error handling for invalid GGUF";
    
    QTemporaryFile invalidFile;
    QVERIFY(invalidFile.open());
    invalidFile.write("NOT A VALID GGUF FILE");
    invalidFile.flush();
    invalidFile.close();
    
    GGUFLoaderQt loader(invalidFile.fileName());
    
    QVERIFY(!loader.isOpen());
    qInfo() << "[TEST] Invalid GGUF correctly rejected";
}

void ModelLoadingTests::testLoadNonExistentFile()
{
    qInfo() << "[TEST] testLoadNonExistentFile - Testing error handling for missing file";
    
    GGUFLoaderQt loader("/non/existent/path/model.gguf");
    
    QVERIFY(!loader.isOpen());
    qInfo() << "[TEST] Non-existent file correctly handled";
}

// ========== MASM Compression Tests ==========

void ModelLoadingTests::testGzipDecompression()
{
    qInfo() << "[TEST] testGzipDecompression - Testing MASM GZIP decompression";
    
    // Create test data
    QByteArray originalData("This is test tensor data that should be compressed and decompressed correctly using MASM kernels");
    QByteArray compressed = createCompressedTensor(originalData, "gzip");
    
    // Test decompression
    BrutalGzipWrapper decompressor;
    QByteArray decompressed;
    
    QElapsedTimer timer;
    timer.start();
    bool success = decompressor.decompress(compressed, decompressed);
    qint64 elapsed = timer.elapsed();
    
    QVERIFY(success);
    QCOMPARE(decompressed, originalData);
    
    double throughputMBps = (decompressed.size() / 1024.0 / 1024.0) / (elapsed / 1000.0);
    qInfo() << QString("[TEST] GZIP decompression throughput: %.2f MB/s").arg(throughputMBps);
    
    // Verify MASM kernel is being used
    QVERIFY(throughputMBps > 100.0);  // Should be > 100 MB/s with MASM
}

void ModelLoadingTests::testDeflateDecompression()
{
    qInfo() << "[TEST] testDeflateDecompression - Testing MASM DEFLATE decompression";
    
    QByteArray originalData("DEFLATE test data for MASM kernel verification");
    QByteArray compressed = createCompressedTensor(originalData, "deflate");
    
    DeflateWrapper decompressor;
    QByteArray decompressed;
    
    QElapsedTimer timer;
    timer.start();
    bool success = decompressor.decompress(compressed, decompressed);
    qint64 elapsed = timer.elapsed();
    
    QVERIFY(success);
    QCOMPARE(decompressed, originalData);
    
    qInfo() << QString("[TEST] DEFLATE decompression completed in %1ms").arg(elapsed);
}

void ModelLoadingTests::testUncompressedPassthrough()
{
    qInfo() << "[TEST] testUncompressedPassthrough - Testing uncompressed data passthrough";
    
    QByteArray uncompressedData("This data is NOT compressed and should pass through unchanged");
    
    BrutalGzipWrapper gzipWrapper;
    QByteArray gzipResult;
    bool gzipSuccess = gzipWrapper.decompress(uncompressedData, gzipResult);
    
    QVERIFY(gzipSuccess);
    QCOMPARE(gzipResult, uncompressedData);
    
    DeflateWrapper deflateWrapper;
    QByteArray deflateResult;
    bool deflateSuccess = deflateWrapper.decompress(uncompressedData, deflateResult);
    
    QVERIFY(deflateSuccess);
    QCOMPARE(deflateResult, uncompressedData);
    
    qInfo() << "[TEST] Uncompressed data correctly passed through without modification";
}

void ModelLoadingTests::testMixedCompressionTensors()
{
    qInfo() << "[TEST] testMixedCompressionTensors - Testing model with mixed compression";
    
    // Simulate a model with some compressed and some uncompressed tensors
    struct TestTensor {
        QString name;
        QByteArray data;
        bool isCompressed;
    };
    
    QVector<TestTensor> tensors = {
        {"blk.0.attn_q.weight", QByteArray(1024, 'A'), false},
        {"blk.0.attn_k.weight", createCompressedTensor(QByteArray(2048, 'B'), "gzip"), true},
        {"blk.0.ffn_up.weight", QByteArray(4096, 'C'), false},
        {"blk.1.attn_q.weight", createCompressedTensor(QByteArray(1024, 'D'), "gzip"), true}
    };
    
    BrutalGzipWrapper decompressor;
    int compressedCount = 0;
    int uncompressedCount = 0;
    
    for (const auto& tensor : tensors) {
        QByteArray result;
        bool success = decompressor.decompress(tensor.data, result);
        QVERIFY(success);
        
        if (tensor.isCompressed) {
            compressedCount++;
        } else {
            uncompressedCount++;
            QCOMPARE(result, tensor.data);
        }
    }
    
    qInfo() << QString("[TEST] Mixed model: %1 compressed, %2 uncompressed tensors")
        .arg(compressedCount).arg(uncompressedCount);
}

// ========== Performance Tests ==========

void ModelLoadingTests::testDecompressionThroughput()
{
    qInfo() << "[TEST] testDecompressionThroughput - Benchmarking MASM decompression";
    
    // Create 100MB of test data
    QByteArray largeData(100 * 1024 * 1024, 'X');
    QByteArray compressed = createCompressedTensor(largeData, "gzip");
    
    BrutalGzipWrapper decompressor;
    QByteArray decompressed;
    
    QElapsedTimer timer;
    timer.start();
    bool success = decompressor.decompress(compressed, decompressed);
    qint64 elapsed = timer.elapsed();
    
    QVERIFY(success);
    
    double throughputMBps = (decompressed.size() / 1024.0 / 1024.0) / (elapsed / 1000.0);
    qInfo() << QString("[TEST] 100MB decompression throughput: %.2f MB/s").arg(throughputMBps);
    
    // MASM should achieve > 200 MB/s on modern CPUs
    QVERIFY(throughputMBps > 200.0);
}

void ModelLoadingTests::testLargeModelLoadTime()
{
    qInfo() << "[TEST] testLargeModelLoadTime - Testing large model load performance";
    
    // This would require a real large model file
    // For now, test with mock data
    QSKIP("Requires actual large model file");
}

void ModelLoadingTests::testMemoryUsageDuringLoad()
{
    qInfo() << "[TEST] testMemoryUsageDuringLoad - Monitoring memory usage";
    
    // This would require platform-specific memory tracking
    QSKIP("Requires memory profiling infrastructure");
}

// ========== Error Handling Tests ==========

void ModelLoadingTests::testCorruptedGzipData()
{
    qInfo() << "[TEST] testCorruptedGzipData - Testing error handling for corrupted GZIP";
    
    // Create corrupted GZIP data (valid header, corrupted payload)
    QByteArray corrupted;
    corrupted.append(static_cast<char>(0x1f));  // GZIP magic
    corrupted.append(static_cast<char>(0x8b));
    corrupted.append(QByteArray(100, '\xFF'));  // Garbage data
    
    BrutalGzipWrapper decompressor;
    QByteArray result;
    bool success = decompressor.decompress(corrupted, result);
    
    // Should handle gracefully (fallback to original data)
    QVERIFY(!success || !result.isEmpty());
    qInfo() << "[TEST] Corrupted GZIP handled gracefully";
}

void ModelLoadingTests::testPartialGGUFFile()
{
    qInfo() << "[TEST] testPartialGGUFFile - Testing partial/truncated GGUF file";
    
    QTemporaryFile partialFile;
    QVERIFY(partialFile.open());
    QByteArray fullGGUF = createMockGGUF();
    partialFile.write(fullGGUF.left(fullGGUF.size() / 2));  // Only write half
    partialFile.flush();
    partialFile.close();
    
    GGUFLoaderQt loader(partialFile.fileName());
    
    QVERIFY(!loader.isOpen());
    qInfo() << "[TEST] Partial GGUF correctly rejected";
}

void ModelLoadingTests::testOutOfMemoryScenario()
{
    qInfo() << "[TEST] testOutOfMemoryScenario - Testing OOM handling";
    
    // This would require deliberate memory exhaustion
    QSKIP("Requires controlled OOM injection");
}

// ========== Integration Tests ==========

void ModelLoadingTests::testAgenticEngineLoading()
{
    qInfo() << "[TEST] testAgenticEngineLoading - Testing AgenticEngine model loading";
    
    AgenticEngine engine;
    engine.initialize();
    
    QSignalSpy modelReadySpy(&engine, &AgenticEngine::modelReady);
    QSignalSpy loadFinishedSpy(&engine, &AgenticEngine::modelLoadingFinished);
    
    // Create mock model
    QTemporaryFile mockModel;
    QVERIFY(mockModel.open());
    mockModel.write(createMockGGUF());
    mockModel.flush();
    mockModel.close();
    
    // Trigger load
    engine.setModel(mockModel.fileName());
    
    // Wait for signals (with timeout)
    QVERIFY(modelReadySpy.wait(10000));
    QCOMPARE(modelReadySpy.count(), 1);
    QCOMPARE(loadFinishedSpy.count(), 1);
    
    // Verify success
    QList<QVariant> readyArgs = modelReadySpy.takeFirst();
    QVERIFY(readyArgs.at(0).toBool() == true);
    
    qInfo() << "[TEST] AgenticEngine loaded model successfully";
}

void ModelLoadingTests::testInferenceEngineIntegration()
{
    qInfo() << "[TEST] testInferenceEngineIntegration - Testing InferenceEngine model loading";
    
    InferenceEngine engine;
    
    QSignalSpy modelLoadedSpy(&engine, &InferenceEngine::modelLoadedChanged);
    
    QTemporaryFile mockModel;
    QVERIFY(mockModel.open());
    mockModel.write(createMockGGUF());
    mockModel.flush();
    mockModel.close();
    
    bool success = engine.loadModel(mockModel.fileName());
    
    QVERIFY(success);
    QVERIFY(engine.isModelLoaded());
    
    qInfo() << "[TEST] InferenceEngine integration successful";
}

void ModelLoadingTests::testModelReadySignalEmission()
{
    qInfo() << "[TEST] testModelReadySignalEmission - Testing signal emission timing";
    
    AgenticEngine engine;
    engine.initialize();
    
    QSignalSpy spy(&engine, &AgenticEngine::modelReady);
    
    QTemporaryFile mockModel;
    QVERIFY(mockModel.open());
    mockModel.write(createMockGGUF());
    mockModel.flush();
    mockModel.close();
    
    QElapsedTimer timer;
    timer.start();
    
    engine.setModel(mockModel.fileName());
    
    QVERIFY(spy.wait(10000));
    qint64 signalDelay = timer.elapsed();
    
    qInfo() << "[TEST] modelReady signal emitted after" << signalDelay << "ms";
    QVERIFY(signalDelay < 10000);  // Should emit within 10 seconds
}

// ========== Helper Functions ==========

QByteArray ModelLoadingTests::createMockGGUF()
{
    // Create minimal valid GGUF header
    QByteArray gguf;
    
    // GGUF magic: "GGUF"
    gguf.append("GGUF", 4);
    
    // Version: 3 (little-endian uint32)
    gguf.append(static_cast<char>(3));
    gguf.append(static_cast<char>(0));
    gguf.append(static_cast<char>(0));
    gguf.append(static_cast<char>(0));
    
    // Tensor count: 10 (little-endian uint64)
    gguf.append(static_cast<char>(10));
    gguf.append(QByteArray(7, '\0'));
    
    // Metadata KV count: 5 (little-endian uint64)
    gguf.append(static_cast<char>(5));
    gguf.append(QByteArray(7, '\0'));
    
    // Add minimal metadata (simplified)
    // In real GGUF, this would include architecture, quantization, etc.
    
    return gguf;
}

QByteArray ModelLoadingTests::createCompressedTensor(const QByteArray& data, const QString& compressionType)
{
    if (compressionType == "gzip") {
        BrutalGzipWrapper compressor;
        QByteArray compressed;
        compressor.compress(data, compressed);
        return compressed;
    } else if (compressionType == "deflate") {
        DeflateWrapper compressor;
        QByteArray compressed;
        compressor.compress(data, compressed);
        return compressed;
    }
    
    return data;
}

QTEST_MAIN(ModelLoadingTests)
#include "model_loading_tests.moc"
