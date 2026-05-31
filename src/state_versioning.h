#pragma once
/**
 * StateVersioning - Enhancement #6: Schema Migration Support
 * 
 * Handles versioned state schemas with automatic migration.
 * Ensures backward compatibility and smooth upgrades.
 * 
 * Symbols: SV_SCHEMA_V1, SV_SCHEMA_V2, SV_SCHEMA_CURRENT
 */

#include <string>
#include <vector>
#include <functional>
#include <nlohmann/json.hpp>

// Schema version symbols
#define SV_SCHEMA_V1            1
#define SV_SCHEMA_V2            2
#define SV_SCHEMA_V3            3
#define SV_SCHEMA_CURRENT       SV_SCHEMA_V2

// Migration flags
#define SV_MIGRATE_AUTO         0x01  // Auto-migrate on load
#define SV_MIGRATE_MANUAL       0x02  // Require explicit migration
#define SV_MIGRATE_READONLY     0x04  // Don't modify original

namespace StateVersioning {

    /**
     * Schema version info
     */
    struct SchemaVersion {
        int major = 1;
        int minor = 0;
        int patch = 0;
        std::string name;
        std::string description;
    };

    /**
     * Migration function type
     */
    using MigrationFunc = std::function<nlohmann::json(const nlohmann::json&)>;

    /**
     * Migration step
     */
    struct MigrationStep {
        int fromVersion;
        int toVersion;
        std::string name;
        MigrationFunc migrate;
        bool isLossless;
    };

    /**
     * Schema registry
     */
    class SchemaRegistry {
    public:
        SchemaRegistry();
        ~SchemaRegistry();

        // Register schema version
        void registerSchema(const SchemaVersion& version);
        
        // Register migration
        void registerMigration(const MigrationStep& step);
        
        // Get current schema version
        SchemaVersion getCurrentSchema() const;
        
        // Get schema for version
        SchemaVersion getSchema(int version) const;
        
        // Check if migration path exists
        bool canMigrate(int fromVersion, int toVersion) const;
        
        // Get migration path
        std::vector<MigrationStep> getMigrationPath(int fromVersion, int toVersion) const;

    private:
        class Impl;
        std::unique_ptr<Impl> m_impl;
    };

    /**
     * Migration engine
     */
    class MigrationEngine {
    public:
        explicit MigrationEngine(SchemaRegistry& registry);
        ~MigrationEngine();

        // Migrate state to target version
        nlohmann::json migrate(
            const nlohmann::json& state,
            int targetVersion = SV_SCHEMA_CURRENT,
            uint32_t flags = SV_MIGRATE_AUTO);
        
        // Validate state against schema
        bool validate(const nlohmann::json& state, int version) const;
        
        // Get validation errors
        std::vector<std::string> getValidationErrors() const;

        // Dry-run migration (no changes)
        std::vector<std::string> dryRunMigrate(
            const nlohmann::json& state,
            int targetVersion) const;

    private:
        SchemaRegistry& m_registry;
        std::vector<std::string> m_validationErrors;
    };

    /**
     * Versioned state wrapper
     */
    class VersionedState {
    public:
        VersionedState();
        ~VersionedState();

        // Load with automatic migration
        bool load(
            const std::string& filePath,
            MigrationEngine& engine,
            uint32_t flags = SV_MIGRATE_AUTO);
        
        // Save with current schema
        bool save(const std::string& filePath) const;
        
        // Get underlying state
        nlohmann::json& getState();
        const nlohmann::json& getState() const;
        
        // Get schema version
        int getSchemaVersion() const;
        
        // Get original version (before migration)
        int getOriginalVersion() const;
        
        // Check if migrated
        bool wasMigrated() const;

    private:
        nlohmann::json m_state;
        int m_originalVersion = 0;
        bool m_wasMigrated = false;
    };

    /**
     * Built-in migrations
     */
    namespace Migrations {
        // V1 -> V2: Add checkpoint metadata
        nlohmann::json v1_to_v2(const nlohmann::json& v1State);
        
        // V2 -> V3: Add compression support markers
        nlohmann::json v2_to_v3(const nlohmann::json& v2State);
    }

    /**
     * Global registry
     */
    SchemaRegistry& getGlobalRegistry();
    void registerBuiltInMigrations();

} // namespace StateVersioning
