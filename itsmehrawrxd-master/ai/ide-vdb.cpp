#include <iostream>
#include <vector>
#include <map>
#include <cmath>
#include <limits>
#include <algorithm>
#include <fstream>
#include <sstream>
#include "serializer.hpp"

namespace IDE_AI {

using Embedding = std::vector<float>;

class VectorDatabase {
public:
    VectorDatabase() : next_id_(0) {}
    
    void add(const std::string& id, const Embedding& embedding, const std::string& metadata) {
        data_.push_back({id, embedding, metadata});
        std::cout << "Added embedding for ID: " << id << " (dim: " << embedding.size() << ")\n";
    }

    std::vector<std::string> search(const Embedding& query, int k) {
        std::vector<std::pair<double, std::string>> results;
        
        for (const auto& item : data_) {
            double distance = cosine_similarity(query, item.embedding);
            results.push_back({distance, item.id});
        }
        
        // Sort results by similarity (distance) in descending order
        std::sort(results.rbegin(), results.rend());
        
        std::vector<std::string> top_k;
        for (int i = 0; i < std::min((int)results.size(), k); ++i) {
            top_k.push_back(results[i].second);
        }
        
        return top_k;
    }
    
    std::string get_metadata(const std::string& id) const {
        for (const auto& item : data_) {
            if (item.id == id) {
                return item.metadata;
            }
        }
        return "";
    }
    
    void save(const std::string& path) {
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Error: Could not open file for writing: " << path << "\n";
            return;
        }
        
        // Write number of items
        size_t num_items = data_.size();
        file.write(reinterpret_cast<const char*>(&num_items), sizeof(num_items));
        
        // Write each item
        for (const auto& item : data_) {
            // Write ID length and ID
            size_t id_length = item.id.length();
            file.write(reinterpret_cast<const char*>(&id_length), sizeof(id_length));
            file.write(item.id.c_str(), id_length);
            
            // Write embedding size and data
            size_t embedding_size = item.embedding.size();
            file.write(reinterpret_cast<const char*>(&embedding_size), sizeof(embedding_size));
            file.write(reinterpret_cast<const char*>(item.embedding.data()), 
                      embedding_size * sizeof(float));
            
            // Write metadata length and metadata
            size_t metadata_length = item.metadata.length();
            file.write(reinterpret_cast<const char*>(&metadata_length), sizeof(metadata_length));
            file.write(item.metadata.c_str(), metadata_length);
        }
        
        file.close();
        std::cout << "Vector database saved to " << path << " (" << num_items << " items)\n";
    }
    
    void load(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Warning: Could not open file for reading: " << path << "\n";
            return;
        }
        
        data_.clear();
        
        // Read number of items
        size_t num_items;
        file.read(reinterpret_cast<char*>(&num_items), sizeof(num_items));
        
        // Read each item
        for (size_t i = 0; i < num_items; ++i) {
            Item item;
            
            // Read ID
            size_t id_length;
            file.read(reinterpret_cast<char*>(&id_length), sizeof(id_length));
            item.id.resize(id_length);
            file.read(&item.id[0], id_length);
            
            // Read embedding
            size_t embedding_size;
            file.read(reinterpret_cast<char*>(&embedding_size), sizeof(embedding_size));
            item.embedding.resize(embedding_size);
            file.read(reinterpret_cast<char*>(item.embedding.data()), 
                     embedding_size * sizeof(float));
            
            // Read metadata
            size_t metadata_length;
            file.read(reinterpret_cast<char*>(&metadata_length), sizeof(metadata_length));
            item.metadata.resize(metadata_length);
            file.read(&item.metadata[0], metadata_length);
            
            data_.push_back(item);
        }
        
        file.close();
        std::cout << "Vector database loaded from " << path << " (" << num_items << " items)\n";
    }
    
    size_t size() const { return data_.size(); }
    
    void clear() { data_.clear(); }

private:
    struct Item {
        std::string id;
        Embedding embedding;
        std::string metadata;
    };
    std::vector<Item> data_;
    size_t next_id_;

    double cosine_similarity(const Embedding& a, const Embedding& b) {
        if (a.size() != b.size()) {
            return 0.0;
        }
        
        double dot_product = 0.0;
        double a_magnitude = 0.0;
        double b_magnitude = 0.0;
        
        for (size_t i = 0; i < a.size(); ++i) {
            dot_product += a[i] * b[i];
            a_magnitude += a[i] * a[i];
            b_magnitude += b[i] * b[i];
        }
        
        if (a_magnitude == 0.0 || b_magnitude == 0.0) return 0.0;
        
        return dot_product / (std::sqrt(a_magnitude) * std::sqrt(b_magnitude));
    }
};

} // namespace IDE_AI
