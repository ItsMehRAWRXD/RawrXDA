// ============================================================================
// Codebase Intelligence Engine — Full Project Indexing
// Semantic understanding of entire codebase with @codebase queries
// ============================================================================
#pragma once
#include "../inference/SovereignInferenceClient.h"
#include "../search/search_engine.h"
#include "../editor/syntax_highlighter.h"
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <mutex>
#include <queue>

namespace RawrXD::Intelligence {

enum class EntityType {
    FUNCTION,
    CLASS,
    VARIABLE,
    INTERFACE,
    ENUM,
    NAMESPACE,
    MODULE,
    IMPORT
};

enum class RelationshipType {
    CALLS,
    INHERITS,
    IMPLEMENTS,
    IMPORTS,
    REFERENCES,
    CONTAINS,
    DEPENDS_ON
};

struct CodeEntity {
    std::string id;
    std::string name;
    EntityType type;
    std::string filePath;
    int lineNumber;
    int column;
    std::string signature;
    std::string documentation;
    std::vector<std::string> modifiers;
    std::map<std::string, std::string> metadata;
};

struct Relationship {
    std::string sourceId;
    std::string targetId;
    RelationshipType type;
    std::string filePath;
    int lineNumber;
};

struct CodebaseQuery {
    std::string query;
    std::vector<EntityType> entityTypes;
    std::vector<std::string> filePatterns;
    int maxResults;
    bool includeDocumentation;
};

struct QueryResult {
    CodeEntity entity;
    double relevance;
    std::string matchedContext;
    std::vector<Relationship> relationships;
};

struct DependencyGraph {
    std::map<std::string, CodeEntity> entities;
    std::map<std::string, std::vector<Relationship>> outgoing;
    std::map<std::string, std::vector<Relationship>> incoming;
};

class CodebaseIntelligenceEngine {
public:
    explicit CodebaseIntelligenceEngine(std::shared_ptr<SovereignInferenceClient> aiClient)
        : m_aiClient(aiClient)
        , m_indexingComplete(false) {}

    void IndexCodebase(const std::string& rootPath) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        m_rootPath = rootPath;
        m_indexingComplete = false;
        
        // Scan all source files
        auto files = ScanSourceFiles(rootPath);
        
        // Parse and index each file
        for (const auto& file : files) {
            IndexFile(file);
        }
        
        // Build relationships
        BuildRelationships();
        
        // Build dependency graph
        BuildDependencyGraph();
        
        m_indexingComplete = true;
    }

    void IndexFile(const std::string& filePath) {
        // Read file content
        std::ifstream file(filePath);
        std::string content((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
        file.close();
        
        // Extract entities
        auto entities = ExtractEntities(filePath, content);
        
        // Store entities
        for (const auto& entity : entities) {
            m_entities[entity.id] = entity;
            m_entitiesByFile[filePath].push_back(entity.id);
            m_entitiesByName[entity.name].push_back(entity.id);
        }
        
        // Index file content for search
        m_fileContents[filePath] = content;
    }

    std::vector<QueryResult> QueryCodebase(const CodebaseQuery& query) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        std::vector<QueryResult> results;
        
        // Search by entity name
        auto nameResults = SearchByName(query.query);
        results.insert(results.end(), nameResults.begin(), nameResults.end());
        
        // Search by semantic meaning (AI-powered)
        if (m_aiClient && m_aiClient->IsLoaded()) {
            auto semanticResults = SemanticSearch(query);
            results.insert(results.end(), semanticResults.begin(), semanticResults.end());
        }
        
        // Filter by entity type
        if (!query.entityTypes.empty()) {
            results.erase(std::remove_if(results.begin(), results.end(),
                [&query](const QueryResult& r) {
                    for (const auto& type : query.entityTypes) {
                        if (r.entity.type == type) return false;
                    }
                    return true;
                }), results.end());
        }
        
        // Filter by file pattern
        if (!query.filePatterns.empty()) {
            results.erase(std::remove_if(results.begin(), results.end(),
                [&query](const QueryResult& r) {
                    for (const auto& pattern : query.filePatterns) {
                        if (r.entity.filePath.find(pattern) != std::string::npos) {
                            return false;
                        }
                    }
                    return true;
                }), results.end());
        }
        
        // Sort by relevance
        std::sort(results.begin(), results.end(),
                 [](const QueryResult& a, const QueryResult& b) {
                     return a.relevance > b.relevance;
                 });
        
        // Limit results
        if (results.size() > static_cast<size_t>(query.maxResults)) {
            results.resize(query.maxResults);
        }
        
        // Add relationships
        for (auto& result : results) {
            result.relationships = GetRelationships(result.entity.id);
        }
        
        return results;
    }

    std::vector<QueryResult> FindReferences(const std::string& entityId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        std::vector<QueryResult> results;
        
        auto it = m_dependencyGraph.incoming.find(entityId);
        if (it != m_dependencyGraph.incoming.end()) {
            for (const auto& rel : it->second) {
                auto entityIt = m_entities.find(rel.sourceId);
                if (entityIt != m_entities.end()) {
                    QueryResult result;
                    result.entity = entityIt->second;
                    result.relevance = 1.0;
                    results.push_back(result);
                }
            }
        }
        
        return results;
    }

    std::vector<QueryResult> FindDependencies(const std::string& entityId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        std::vector<QueryResult> results;
        
        auto it = m_dependencyGraph.outgoing.find(entityId);
        if (it != m_dependencyGraph.outgoing.end()) {
            for (const auto& rel : it->second) {
                auto entityIt = m_entities.find(rel.targetId);
                if (entityIt != m_entities.end()) {
                    QueryResult result;
                    result.entity = entityIt->second;
                    result.relevance = 1.0;
                    results.push_back(result);
                }
            }
        }
        
        return results;
    }

    DependencyGraph GetDependencyGraph() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_dependencyGraph;
    }

    std::string GenerateArchitectureDiagram() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        std::ostringstream diagram;
        diagram << "```mermaid\n";
        diagram << "graph TD\n";
        
        // Add nodes
        for (const auto& [id, entity] : m_entities) {
            if (entity.type == EntityType::CLASS || entity.type == EntityType::INTERFACE) {
                diagram << "    " << id << "[" << entity.name << "]\n";
            }
        }
        
        // Add relationships
        for (const auto& [id, relationships] : m_dependencyGraph.outgoing) {
            for (const auto& rel : relationships) {
                if (rel.type == RelationshipType::INHERITS ||
                    rel.type == RelationshipType::IMPLEMENTS) {
                    diagram << "    " << id << " -->|" << RelationshipToString(rel.type) 
                           << "| " << rel.targetId << "\n";
                }
            }
        }
        
        diagram << "```\n";
        
        return diagram.str();
    }

    std::string GenerateCodebaseReport() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        std::ostringstream report;
        report << "# Codebase Intelligence Report\n\n";
        report << "**Root Path:** " << m_rootPath << "\n";
        report << "**Total Entities:** " << m_entities.size() << "\n";
        report << "**Indexed Files:** " << m_fileContents.size() << "\n\n";
        
        // Count by type
        std::map<EntityType, int> typeCounts;
        for (const auto& [id, entity] : m_entities) {
            typeCounts[entity.type]++;
        }
        
        report << "## Entity Types\n";
        for (const auto& [type, count] : typeCounts) {
            report << "- " << TypeToString(type) << ": " << count << "\n";
        }
        
        report << "\n## Architecture\n";
        report << GenerateArchitectureDiagram() << "\n";
        
        return report.str();
    }

    bool IsIndexingComplete() const {
        return m_indexingComplete;
    }

private:
    std::shared_ptr<SovereignInferenceClient> m_aiClient;
    mutable std::mutex m_mutex;
    std::string m_rootPath;
    bool m_indexingComplete;
    
    std::map<std::string, CodeEntity> m_entities;
    std::map<std::string, std::vector<std::string>> m_entitiesByFile;
    std::map<std::string, std::vector<std::string>> m_entitiesByName;
    std::map<std::string, std::string> m_fileContents;
    std::vector<Relationship> m_relationships;
    DependencyGraph m_dependencyGraph;

    std::vector<std::string> ScanSourceFiles(const std::string& rootPath) {
        std::vector<std::string> files;
        
        for (const auto& entry : std::filesystem::recursive_directory_iterator(rootPath)) {
            if (entry.is_regular_file()) {
                auto ext = entry.path().extension().string();
                if (ext == ".cpp" || ext == ".h" || ext == ".hpp" ||
                    ext == ".c" || ext == ".cc" || ext == ".py" ||
                    ext == ".js" || ext == ".ts" || ext == ".java") {
                    files.push_back(entry.path().string());
                }
            }
        }
        
        return files;
    }

    std::vector<CodeEntity> ExtractEntities(const std::string& filePath, 
                                            const std::string& content) {
        std::vector<CodeEntity> entities;
        
        // Extract functions
        std::regex funcPattern(R"((\w+)\s+(\w+)\s*\(([^)]*)\)\s*\{)");
        std::sregex_iterator iter(content.begin(), content.end(), funcPattern);
        std::sregex_iterator end;
        
        for (; iter != end; ++iter) {
            CodeEntity entity;
            entity.id = filePath + "::" + (*iter)[2].str();
            entity.name = (*iter)[2];
            entity.type = EntityType::FUNCTION;
            entity.filePath = filePath;
            entity.signature = (*iter)[0];
            entities.push_back(entity);
        }
        
        // Extract classes
        std::regex classPattern(R"(class\s+(\w+))");
        std::sregex_iterator classIter(content.begin(), content.end(), classPattern);
        
        for (; classIter != end; ++classIter) {
            CodeEntity entity;
            entity.id = filePath + "::" + (*classIter)[1].str();
            entity.name = (*classIter)[1];
            entity.type = EntityType::CLASS;
            entity.filePath = filePath;
            entities.push_back(entity);
        }
        
        return entities;
    }

    void BuildRelationships() {
        // Analyze code to find relationships
        for (const auto& [id, entity] : m_entities) {
            auto content = m_fileContents[entity.filePath];
            
            // Find references to other entities
            for (const auto& [otherId, otherEntity] : m_entities) {
                if (id != otherId && content.find(otherEntity.name) != std::string::npos) {
                    Relationship rel;
                    rel.sourceId = id;
                    rel.targetId = otherId;
                    rel.type = RelationshipType::REFERENCES;
                    rel.filePath = entity.filePath;
                    m_relationships.push_back(rel);
                }
            }
        }
    }

    void BuildDependencyGraph() {
        for (const auto& rel : m_relationships) {
            m_dependencyGraph.outgoing[rel.sourceId].push_back(rel);
            m_dependencyGraph.incoming[rel.targetId].push_back(rel);
        }
    }

    std::vector<QueryResult> SearchByName(const std::string& query) {
        std::vector<QueryResult> results;
        
        for (const auto& [name, ids] : m_entitiesByName) {
            if (name.find(query) != std::string::npos) {
                for (const auto& id : ids) {
                    auto it = m_entities.find(id);
                    if (it != m_entities.end()) {
                        QueryResult result;
                        result.entity = it->second;
                        result.relevance = 1.0;
                        results.push_back(result);
                    }
                }
            }
        }
        
        return results;
    }

    std::vector<QueryResult> SemanticSearch(const CodebaseQuery& query) {
        std::vector<QueryResult> results;
        
        if (!m_aiClient || !m_aiClient->IsLoaded()) {
            return results;
        }

        std::string prompt = "Find code related to: " + query.query + "\n\n" +
                            "Available entities:\n";
        
        for (const auto& [id, entity] : m_entities) {
            prompt += "- " + entity.name + " (" + TypeToString(entity.type) + ")\n";
        }
        
        std::vector<ChatMessage> messages = {
            {"system", "You are a code search expert. Find relevant code entities."},
            {"user", prompt}
        };

        auto result = m_aiClient->ChatSync(messages);
        
        if (result.success) {
            // Parse AI response
            for (const auto& [id, entity] : m_entities) {
                if (result.response.find(entity.name) != std::string::npos) {
                    QueryResult qr;
                    qr.entity = entity;
                    qr.relevance = 0.9;
                    results.push_back(qr);
                }
            }
        }
        
        return results;
    }

    std::vector<Relationship> GetRelationships(const std::string& entityId) {
        std::vector<Relationship> result;
        
        auto outIt = m_dependencyGraph.outgoing.find(entityId);
        if (outIt != m_dependencyGraph.outgoing.end()) {
            result.insert(result.end(), outIt->second.begin(), outIt->second.end());
        }
        
        return result;
    }

    std::string TypeToString(EntityType type) {
        switch (type) {
            case EntityType::FUNCTION: return "Function";
            case EntityType::CLASS: return "Class";
            case EntityType::VARIABLE: return "Variable";
            case EntityType::INTERFACE: return "Interface";
            case EntityType::ENUM: return "Enum";
            case EntityType::NAMESPACE: return "Namespace";
            case EntityType::MODULE: return "Module";
            case EntityType::IMPORT: return "Import";
            default: return "Unknown";
        }
    }

    std::string RelationshipToString(RelationshipType type) {
        switch (type) {
            case RelationshipType::CALLS: return "calls";
            case RelationshipType::INHERITS: return "inherits";
            case RelationshipType::IMPLEMENTS: return "implements";
            case RelationshipType::IMPORTS: return "imports";
            case RelationshipType::REFERENCES: return "references";
            case RelationshipType::CONTAINS: return "contains";
            case RelationshipType::DEPENDS_ON: return "depends on";
            default: return "unknown";
        }
    }
};

} // namespace RawrXD::Intelligence
