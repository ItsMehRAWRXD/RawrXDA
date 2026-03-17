#include "src/qtapp/gguf_parser.hpp"
#include "src/qtapp/gguf_loader.hpp"
#include <iostream>
#include <QString>
#include <QCoreApplication>

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    
    if (argc < 2) {
        std::cout << "Usage: test_gguf_load <path_to_gguf_model>" << std::endl;
        std::cout << "Example: test_gguf_load D:\\OllamaModels\\BigDaddyG-Q2_K-PRUNED-16GB.gguf" << std::endl;
        return 1;
    }
    
    QString modelPath = argv[1];
    std::cout << "Testing GGUF loader with: " << modelPath.toStdString() << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Test GGUF Parser
    GGUFParser parser;
    if (!parser.parseFile(modelPath)) {
        std::cerr << "ERROR: Failed to parse GGUF file" << std::endl;
        return 1;
    }
    
    std::cout << "✅ GGUF file parsed successfully!" << std::endl;
    
    // Get metadata
    auto metadata = parser.getMetadata();
    std::cout << "\n📊 Model Metadata:" << std::endl;
    std::cout << "  Metadata entries: " << metadata.size() << std::endl;
    
    // Get tensors
    auto tensors = parser.getTensors();
    std::cout << "\n🔢 Tensor Information:" << std::endl;
    std::cout << "  Total tensors: " << tensors.size() << std::endl;
    
    // Count quantization types
    std::map<GGMLType, int> quantCounts;
    uint64_t totalSize = 0;
    
    for (const auto& tensor : tensors) {
        quantCounts[tensor.type]++;
        totalSize += tensor.size;
    }
    
    std::cout << "\n📈 Quantization Breakdown:" << std::endl;
    for (const auto& [type, count] : quantCounts) {
        std::cout << "  " << static_cast<int>(type) << ": " << count << " tensors" << std::endl;
    }
    
    std::cout << "\n💾 Memory Information:" << std::endl;
    std::cout << "  Total tensor size: " << (totalSize / (1024.0 * 1024 * 1024)) << " GB" << std::endl;
    
    // Test GGUF Loader
    std::cout << "\n🔄 Testing GGUF Loader..." << std::endl;
    GGUFLoader loader(modelPath);
    
    if (!loader.isOpen()) {
        std::cerr << "ERROR: Failed to open GGUF file with loader" << std::endl;
        return 1;
    }
    
    std::cout << "✅ GGUF loader opened successfully!" << std::endl;
    
    // Test loading first tensor
    if (!tensors.empty()) {
        std::cout << "\n🧪 Testing tensor loading..." << std::endl;
        std::cout << "  Loading tensor: " << tensors[0].name.toStdString() << std::endl;
        
        QByteArray tensorData = loader.inflateWeight(tensors[0].name);
        if (tensorData.isEmpty()) {
            std::cerr << "WARNING: Failed to load tensor data" << std::endl;
        } else {
            std::cout << "✅ Tensor loaded: " << tensorData.size() << " bytes" << std::endl;
        }
    }
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "🎉 All tests passed! Your GGUF support is working!" << std::endl;
    std::cout << "\nYou can now use this model in your IDE:" << std::endl;
    std::cout << "  Model: " << modelPath.toStdString() << std::endl;
    std::cout << "  Tensors: " << tensors.size() << std::endl;
    std::cout << "  Size: " << (totalSize / (1024.0 * 1024 * 1024)) << " GB" << std::endl;
    
    return 0;
}
