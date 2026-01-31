// In-memory vector database and project knowledge graph
#include <vector>
#include <map>
#include <string>
#include <memory>
#include <algorithm>
#include <cmath>

namespace IDE_AI {
    
    // In-memory vector database for semantic search
    class InMemoryVectorDatabase {
    private:
        std::vector<std::vector<float>> embeddings;
        std::vector<std::string> documents;
        std::vector<std::map<std::string, std::string>> metadata;
        int dimension;
        
    public:
        InMemoryVectorDatabase(int dim = 512) : dimension(dim) {}
        
        void add_document(const std::string& doc, const std::vector<float>& embedding, 
                         const std::map<std::string, std::string>& meta = {}) {
            documents.push_back(doc);
            embeddings.push_back(embedding);
            metadata.push_back(meta);
        }
        
        std::vector<std::pair<std::string, float>> similarity_search(
            const std::vector<float>& query_embedding, int top_k = 5) {
            
            std::vector<std::pair<std::string, float>> results;
            
            for (size_t i = 0; i < embeddings.size(); ++i) {
                float similarity = cosine_similarity(query_embedding, embeddings[i]);
                results.push_back({documents[i], similarity});
            }
            
            // Sort by similarity (descending)
            std::sort(results.begin(), results.end(), 
                     [](const auto& a, const auto& b) {
                         return a.second > b.second;
                     });
            
            // Return top_k results
            if (results.size() > static_cast<size_t>(top_k)) {
                results.resize(top_k);
            }
            
            return results;
        }
        
        std::vector<std::string> get_all_documents() const {
            return documents;
        }
        
        size_t size() const {
            return documents.size();
        }
        
    private:
        float cosine_similarity(const std::vector<float>& a, const std::vector<float>& b) {
            if (a.size() != b.size()) return 0.0f;
            
            float dot_product = 0.0f;
            float norm_a = 0.0f;
            float norm_b = 0.0f;
            
            for (size_t i = 0; i < a.size(); ++i) {
                dot_product += a[i] * b[i];
                norm_a += a[i] * a[i];
                norm_b += b[i] * b[i];
            }
            
            if (norm_a == 0.0f || norm_b == 0.0f) return 0.0f;
            
            return dot_product / (std::sqrt(norm_a) * std::sqrt(norm_b));
        }
    };
    
    // In-memory project knowledge graph
    class InMemoryProjectKnowledgeGraph {
    private:
        struct Node {
            std::string id;
            std::string type;
            std::map<std::string, std::string> properties;
        };
        
        struct Edge {
            std::string from;
            std::string to;
            std::string relationship;
            float weight;
        };
        
        std::map<std::string, Node> nodes;
        std::vector<Edge> edges;
        
    public:
        void add_node(const std::string& id, const std::string& type, 
                     const std::map<std::string, std::string>& properties = {}) {
            nodes[id] = {id, type, properties};
        }
        
        void add_edge(const std::string& from, const std::string& to, 
                     const std::string& relationship, float weight = 1.0f) {
            edges.push_back({from, to, relationship, weight});
        }
        
        std::vector<std::string> get_related_nodes(const std::string& node_id, 
                                                  const std::string& relationship = "") {
            std::vector<std::string> related;
            
            for (const auto& edge : edges) {
                if (edge.from == node_id && 
                    (relationship.empty() || edge.relationship == relationship)) {
                    related.push_back(edge.to);
                }
            }
            
            return related;
        }
        
        std::vector<std::string> find_path(const std::string& from, const std::string& to) {
            // Simple BFS path finding
            std::map<std::string, std::string> parent;
            std::vector<std::string> queue = {from};
            std::set<std::string> visited = {from};
            
            while (!queue.empty()) {
                std::string current = queue.front();
                queue.erase(queue.begin());
                
                if (current == to) {
                    // Reconstruct path
                    std::vector<std::string> path;
                    std::string node = to;
                    while (node != from) {
                        path.push_back(node);
                        node = parent[node];
                    }
                    path.push_back(from);
                    std::reverse(path.begin(), path.end());
                    return path;
                }
                
                for (const auto& edge : edges) {
                    if (edge.from == current && visited.find(edge.to) == visited.end()) {
                        visited.insert(edge.to);
                        parent[edge.to] = current;
                        queue.push_back(edge.to);
                    }
                }
            }
            
            return {}; // No path found
        }
        
        std::vector<std::string> get_nodes_by_type(const std::string& type) {
            std::vector<std::string> result;
            for (const auto& [id, node] : nodes) {
                if (node.type == type) {
                    result.push_back(id);
                }
            }
            return result;
        }
        
        size_t node_count() const {
            return nodes.size();
        }
        
        size_t edge_count() const {
            return edges.size();
        }
    };
    
} // namespace IDE_AI