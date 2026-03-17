#include "masm_compressed_gguf.hpp"
#include <fstream>
#include <iostream>
#include <chrono>
#include <vector>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input.gguf> <output.masm.gguf>\n";
        return 1;
    }
    
    try {
        std::ifstream file(argv[1], std::ios::binary | std::ios::ate);
        if (!file) throw std::runtime_error("Cannot open input");
        
        size_t file_size = file.tellg();
        file.seekg(0);
        
        std::vector<uint8_t> data(file_size);
        file.read(reinterpret_cast<char*>(data.data()), file_size);
        file.close();
        
        std::cout << "Compressing " << (file_size / 1048576.0) << " MB...\n";
        
        auto start = std::chrono::high_resolution_clock::now();
        auto compressed = rawr::MASMCompressedGGUF::compress(data.data(), file_size);
        auto end = std::chrono::high_resolution_clock::now();
        
        double elapsed = std::chrono::duration<double>(end - start).count();
        double speed = (file_size / 1048576.0) / elapsed;
        
        std::cout << "Compressed: " << (compressed.size() / 1048576.0) << " MB\n";
        std::cout << "Time: " << elapsed << "s, Speed: " << speed << " MB/s\n";
        std::cout << "Ratio: " << ((compressed.size() * 100.0) / file_size) << "%\n";
        
        std::ofstream out(argv[2], std::ios::binary);
        out.write(reinterpret_cast<const char*>(compressed.data()), compressed.size());
        std::cout << "Saved: " << argv[2] << "\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
