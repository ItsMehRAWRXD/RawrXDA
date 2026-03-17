#include <iostream>
#include <QCoreApplication>
#include <QDebug>
#include "gguf_parser.hpp"

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <gguf_file>" << std::endl;
        return 1;
    }
    
    QString modelPath = QString::fromUtf8(argv[1]);
    
    std::cout << "\n=== GGUF Parser Integration Test ===" << std::endl;
    std::cout << "Model: " << modelPath.toStdString() << std::endl;
    std::cout << std::endl;
    
    // Parse GGUF file
    GGUFParser parser(modelPath);
    
    if (!parser.isValid()) {
        std::cerr << "❌ Failed to parse GGUF file!" << std::endl;
        return 1;
    }
    
    std::cout << "✅ GGUF parsed successfully" << std::endl;
    std::cout << "   Version: " << parser.version() << std::endl;
    std::cout << std::endl;
    
    // Display metadata
    const GGUFMetadata& meta = parser.metadata();
    std::cout << "📋 Metadata:" << std::endl;
    std::cout << "   Architecture: " << meta.architecture.toStdString() << std::endl;
    std::cout << "   Vocab Size:   " << meta.vocab_size << std::endl;
    std::cout << "   Embedding:    " << meta.n_embd << std::endl;
    std::cout << "   Heads:        " << meta.n_head << std::endl;
    std::cout << "   Layers:       " << meta.n_layer << std::endl;
    std::cout << "   Context:      " << meta.n_ctx << std::endl;
    std::cout << "   Tokenizer:    " << meta.tokenizer_model.toStdString() << std::endl;
    std::cout << std::endl;
    
    // Display tensor statistics
    const QVector<GGUFTensorInfo>& tensors = parser.tensors();
    std::cout << "📊 Tensors: " << tensors.size() << " total" << std::endl;
    std::cout << std::endl;
    
    // Count quantization types
    QHash<QString, int> typeCount;
    uint64_t totalSize = 0;
    
    for (const GGUFTensorInfo& tensor : tensors) {
        QString typeName = GGUFParser::typeName(tensor.type);
        typeCount[typeName]++;
        totalSize += tensor.size;
    }
    
    std::cout << "Quantization Types:" << std::endl;
    for (auto it = typeCount.constBegin(); it != typeCount.constEnd(); ++it) {
        std::cout << "   " << it.key().toStdString() << ": " << it.value() << " tensors" << std::endl;
    }
    std::cout << std::endl;
    
    std::cout << "Total tensor data: " << (totalSize / 1024.0 / 1024.0 / 1024.0) << " GB" << std::endl;
    std::cout << std::endl;
    
    // Show sample tensors
    std::cout << "Sample Tensors (first 5):" << std::endl;
    int sampleCount = (tensors.size() < 5) ? tensors.size() : 5;
    for (int i = 0; i < sampleCount; ++i) {
        const GGUFTensorInfo& t = tensors[i];
        std::cout << "   [" << GGUFParser::typeName(t.type).toStdString() << "] " 
                  << t.name.toStdString() << " (";
        for (int d = 0; d < t.dimensions.size(); ++d) {
            if (d > 0) std::cout << " x ";
            std::cout << t.dimensions[d];
        }
        std::cout << ") " << (t.size / 1024.0 / 1024.0) << " MB" << std::endl;
    }
    std::cout << std::endl;
    
    // Test reading a tensor
    if (!tensors.isEmpty()) {
        const GGUFTensorInfo& firstTensor = tensors[0];
        std::cout << "🔍 Testing tensor read: " << firstTensor.name.toStdString() << std::endl;
        
        QByteArray data = parser.readTensorData(firstTensor);
        if (!data.isEmpty()) {
            std::cout << "✅ Read " << data.size() << " bytes successfully" << std::endl;
            
            // Show first few bytes
            std::cout << "   First 16 bytes (hex): ";
            int byteCount = (data.size() < 16) ? data.size() : 16;
            for (int i = 0; i < byteCount; ++i) {
                printf("%02x ", (unsigned char)data[i]);
            }
            std::cout << std::endl;
        } else {
            std::cout << "❌ Failed to read tensor data" << std::endl;
        }
    }
    std::cout << std::endl;
    
    // Check for Q2_K/Q3_K tensors
    bool hasQ2K = typeCount.contains("Q2_K");
    bool hasQ3K = typeCount.contains("Q3_K");
    
    if (hasQ2K || hasQ3K) {
        std::cout << "🎯 Q2_K/Q3_K DETECTION:" << std::endl;
        if (hasQ2K) {
            std::cout << "   ✅ Found " << typeCount["Q2_K"] << " Q2_K tensors" << std::endl;
        }
        if (hasQ3K) {
            std::cout << "   ✅ Found " << typeCount["Q3_K"] << " Q3_K tensors" << std::endl;
        }
        std::cout << "\n   This model is compatible with Q2_K/Q3_K inference!" << std::endl;
    } else {
        std::cout << "⚠ No Q2_K/Q3_K tensors found" << std::endl;
        std::cout << "   Model uses: ";
        for (auto it = typeCount.constBegin(); it != typeCount.constEnd(); ++it) {
            std::cout << it.key().toStdString() << " ";
        }
        std::cout << std::endl;
    }
    
    std::cout << "\n=== TEST COMPLETE ===" << std::endl;
    
    return 0;
}
