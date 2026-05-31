/**
 * StateVersioning Implementation
 * Enhancement #6: Schema Migration Support
 */

#include "state_versioning.h"
#include <unordered_map>
#include <queue>
#include <set>
#include <fstream>
#include <filesystem>

namespace StateVersioning {

    // ===== SchemaRegistry Implementation =====

    class SchemaRegistry::Impl {
    public:
        std::unordered_map<int, SchemaVersion> schemas;
        std::unordered_map<int, std::vector<MigrationStep>> migrations;
    };

    SchemaRegistry::SchemaRegistry() 
        : m_impl(std::make_unique<Impl>()) {
    }

    SchemaRegistry::~SchemaRegistry() = default;

    void SchemaRegistry::registerSchema(const SchemaVersion& version) {
        m_impl->schemas[version.major] = version;
    }

    void SchemaRegistry::registerMigration(const MigrationStep& step) {
        m_impl->migrations[step.fromVersion].push_back(step);
    }

    SchemaVersion SchemaRegistry::getCurrentSchema() const {
        auto it = m_impl->schemas.find(SV_SCHEMA_CURRENT);
        if (it != m_impl->schemas.end()) {
            return it->second;
        }
        return SchemaVersion{};
    }

    SchemaVersion SchemaRegistry::getSchema(int version) const {
        auto it = m_impl->schemas.find(version);
        if (it != m_impl->schemas.end()) {
            return it->second;
        }
        return SchemaVersion{};
    }

    bool SchemaRegistry::canMigrate(int fromVersion, int toVersion) const {
        if (fromVersion == toVersion) return true;
        if (fromVersion > toVersion) return false;
        
        // BFS to find path
        std::set<int> visited;
        std::queue<int> queue;
        queue.push(fromVersion);
        
        while (!queue.empty()) {
            int current = queue.front();
            queue.pop();
            
            if (current == toVersion) return true;
            if (visited.count(current)) continue;
            visited.insert(current);
            
            auto it = m_impl->migrations.find(current);
            if (it != m_impl->migrations.end()) {
                for (const auto& step : it->second) {
                    queue.push(step.toVersion);
                }
            }
        }
        
        return false;
    }

    std::vector<MigrationStep> SchemaRegistry::getMigrationPath(int fromVersion, int toVersion) const {
        std::vector<MigrationStep> path;
        
        if (fromVersion == toVersion) return path;
        if (fromVersion > toVersion) return path;
        
        // BFS with path tracking
        std::unordered_map<int, MigrationStep> parent;
        std::set<int> visited;
        std::queue<int> queue;
        queue.push(fromVersion);
        
        while (!queue.empty()) {
            int current = queue.front();
            queue.pop();
            
            if (current == toVersion) break;
            if (visited.count(current)) continue;
            visited.insert(current);
            
            auto it = m_impl->migrations.find(current);
            if (it != m_impl->migrations.end()) {
                for (const auto& step : it->second) {
                    if (!visited.count(step.toVersion)) {
                        parent[step.toVersion] = step;
                        queue.push(step.toVersion);
                    }
                }
            }
        }
        
        // Reconstruct path
        if (!parent.count(toVersion)) return path;
        
        int current = toVersion;
        while (current != fromVersion) {
            path.push_back(parent[current]);
            current = parent[current].fromVersion;
        }
        
        std::reverse(path.begin(), path.end());
        return path;
    }

    // ===== MigrationEngine Implementation =====

    MigrationEngine::MigrationEngine(SchemaRegistry& registry)
        : m_registry(registry) {
    }

    MigrationEngine::~MigrationEngine() = default;

    nlohmann::json MigrationEngine::migrate(
        const nlohmann::json& state,
        int targetVersion,
        uint32_t flags) {
        
        m_validationErrors.clear();
        
        // Detect current version
        int currentVersion = SV_SCHEMA_V1;
        if (state.contains("schemaVersion")) {
            currentVersion = state["schemaVersion"].get<int>();
        } else if (state.contains("version")) {
            currentVersion = state["version"].get<int>();
        }
        
        if (currentVersion == targetVersion) {
            return state;
        }
        
        if (currentVersion > targetVersion) {
            m_validationErrors.push_back("Cannot downgrade from v" + 
                std::to_string(currentVersion) + " to v" + std::to_string(targetVersion));
            return state;
        }
        
        // Get migration path
        auto path = m_registry.getMigrationPath(currentVersion, targetVersion);
        if (path.empty()) {
            m_validationErrors.push_back("No migration path from v" + 
                std::to_string(currentVersion) + " to v" + std::to_string(targetVersion));
            return state;
        }
        
        // Apply migrations
        nlohmann::json result = state;
        for (const auto& step : path) {
            if (step.migrate) {
                result = step.migrate(result);
            }
            
            if (!validate(result, step.toVersion)) {
                m_validationErrors.push_back("Validation failed after migration to v" + 
                    std::to_string(step.toVersion));
                break;
            }
        }
        
        // Update schema version
        result["schemaVersion"] = targetVersion;
        
        return result;
    }

    bool MigrationEngine::validate(const nlohmann::json& state, int version) const {
        // Basic validation
        if (!state.is_object()) {
            return false;
        }
        
        // Version-specific validation
        switch (version) {
            case SV_SCHEMA_V1:
                return state.contains("executionId") && state.contains("status");
                
            case SV_SCHEMA_V2:
                return state.contains("executionId") 
                    && state.contains("status")
                    && state.contains("checkpoints");
                    
            case SV_SCHEMA_V3:
                return state.contains("executionId") 
                    && state.contains("status")
                    && state.contains("checkpoints")
                    && state.contains("compression");
                    
            default:
                return true;
        }
    }

    std::vector<std::string> MigrationEngine::getValidationErrors() const {
        return m_validationErrors;
    }

    std::vector<std::string> MigrationEngine::dryRunMigrate(
        const nlohmann::json& state,
        int targetVersion) const {
        
        std::vector<std::string> errors;
        
        int currentVersion = SV_SCHEMA_V1;
        if (state.contains("schemaVersion")) {
            currentVersion = state["schemaVersion"].get<int>();
        }
        
        auto path = m_registry.getMigrationPath(currentVersion, targetVersion);
        if (path.empty()) {
            errors.push_back("No migration path available");
        }
        
        for (const auto& step : path) {
            errors.push_back("Would apply: " + step.name + " (v" + 
                std::to_string(step.fromVersion) + " -> v" + 
                std::to_string(step.toVersion) + ")");
            if (!step.isLossless) {
                errors.push_back("  WARNING: This migration is lossy");
            }
        }
        
        return errors;
    }

    // ===== VersionedState Implementation =====

    VersionedState::VersionedState() = default;
    VersionedState::~VersionedState() = default;

    bool VersionedState::load(
        const std::string& filePath,
        MigrationEngine& engine,
        uint32_t flags) {
        
        std::ifstream file(filePath);
        if (!file) return false;
        
        nlohmann::json loaded;
        try {
            std::string content((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
            loaded = nlohmann::json::parse(content);
        } catch (...) {
            return false;
        }
        
        m_originalVersion = loaded.value("schemaVersion", SV_SCHEMA_V1);
        
        if (flags & SV_MIGRATE_AUTO) {
            m_state = engine.migrate(loaded, SV_SCHEMA_CURRENT, flags);
            m_wasMigrated = (m_state != loaded);
        } else {
            m_state = loaded;
            m_wasMigrated = false;
        }
        
        return true;
    }

    bool VersionedState::save(const std::string& filePath) const {
        std::ofstream file(filePath);
        if (!file) return false;
        
        file << m_state.dump(2);
        return file.good();
    }

    nlohmann::json& VersionedState::getState() {
        return m_state;
    }

    const nlohmann::json& VersionedState::getState() const {
        return m_state;
    }

    int VersionedState::getSchemaVersion() const {
        return m_state.value("schemaVersion", SV_SCHEMA_V1);
    }

    int VersionedState::getOriginalVersion() const {
        return m_originalVersion;
    }

    bool VersionedState::wasMigrated() const {
        return m_wasMigrated;
    }

    // ===== Built-in Migrations =====

    namespace Migrations {
        
        nlohmann::json v1_to_v2(const nlohmann::json& v1State) {
            nlohmann::json v2State = v1State;
            
            // Add checkpoint metadata if missing
            if (!v2State.contains("checkpoints")) {
                v2State["checkpoints"] = nlohmann::json::array();
            }
            
            // Add currentCheckpointIndex if missing
            if (!v2State.contains("currentCheckpointIndex")) {
                v2State["currentCheckpointIndex"] = -1;
            }
            
            // Add metadata if missing
            if (!v2State.contains("metadata")) {
                v2State["metadata"] = nlohmann::json::object();
            }
            
            return v2State;
        }
        
        nlohmann::json v2_to_v3(const nlohmann::json& v2State) {
            nlohmann::json v3State = v2State;
            
            // Add compression markers
            if (!v3State.contains("compression")) {
                v3State["compression"] = {
                    {"algorithm", "none"},
                    {"originalSize", 0},
                    {"compressedSize", 0}
                };
            }
            
            // Add encryption markers
            if (!v3State.contains("encryption")) {
                v3State["encryption"] = {
                    {"algorithm", "none"},
                    {"encrypted", false}
                };
            }
            
            return v3State;
        }
        
    } // namespace Migrations

    // ===== Global Registry =====

    SchemaRegistry& getGlobalRegistry() {
        static SchemaRegistry instance;
        return instance;
    }

    void registerBuiltInMigrations() {
        auto& registry = getGlobalRegistry();
        
        // Register schemas
        registry.registerSchema({1, 0, 0, "v1", "Initial schema"});
        registry.registerSchema({2, 0, 0, "v2", "Added checkpoint metadata"});
        registry.registerSchema({3, 0, 0, "v3", "Added compression/encryption markers"});
        
        // Register migrations
        registry.registerMigration({1, 2, "v1_to_v2", Migrations::v1_to_v2, true});
        registry.registerMigration({2, 3, "v2_to_v3", Migrations::v2_to_v3, true});
    }

} // namespace StateVersioning
