// ============================================================================
// reasoning_schema_versioning.hpp — Versioned Reasoning Schema System
// ============================================================================
// Architecture: C++20, Win32, no exceptions, no Qt
//
// Ensures that the reasoning pipeline's behavior is:
//   1. Versioned — every schema change gets a semantic version
//   2. Stable — old configs continue to work via migration functions
//   3. Auditable — full changelog of what changed between versions
//   4. Backwards-compatible — v1 profiles load in v3 runtime
//
// Why this matters for valuation:
//   - Makes the reasoning control plane enterprise-grade
//   - Prevents "it used to work" regressions
//   - Enables reproducible customer deployments
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>
#include <unordered_map>
#include "../core/model_memory_hotpatch.hpp"

// ============================================================================
// SemanticVersion — Standard semver
// ============================================================================
struct SemanticVersion {
    int major;
    int minor;
    int patch;

    SemanticVersion() : major(1), minor(0), patch(0) {}
    SemanticVersion(int ma, int mi, int pa) : major(ma), minor(mi), patch(pa) {}

    bool operator==(const SemanticVersion& o) const {
        return major == o.major && minor == o.minor && patch == o.patch;
    }
    bool operator<(const SemanticVersion& o) const {
        if (major != o.major) return major < o.major;
        if (minor != o.minor) return minor < o.minor;
        return patch < o.patch;
    }
    bool operator>(const SemanticVersion& o) const { return o < *this; }
    bool operator<=(const SemanticVersion& o) const { return !(o < *this); }
    bool operator>=(const SemanticVersion& o) const { return !(*this < o); }

    bool isCompatibleWith(const SemanticVersion& o) const {
        // Same major version → backwards compatible
        return major == o.major;
    }

    std::string toString() const {
        return std::to_string(major) + "." +
               std::to_string(minor) + "." +
               std::to_string(patch);
    }

    static SemanticVersion parse(const std::string& s) {
        SemanticVersion v;
        int parts[3] = {0, 0, 0};
        int idx = 0;
        std::string num;
        for (char c : s) {
            if (c == '.' && idx < 2) {
                parts[idx++] = std::stoi(num.empty() ? "0" : num);
                num.clear();
            } else if (c >= '0' && c <= '9') {
                num += c;
            }
        }
        if (!num.empty() && idx < 3) parts[idx] = std::stoi(num);
        v.major = parts[0]; v.minor = parts[1]; v.patch = parts[2];
        return v;
    }
};

// ============================================================================
// SchemaField — One field in the reasoning schema
// ============================================================================
struct SchemaField {
    std::string     name;               // Field identifier
    std::string     type;               // "int", "float", "bool", "string", "enum"
    std::string     description;        // Human-readable description
    std::string     defaultValue;       // Default value as string
    bool            required;           // Whether field is mandatory
    bool            deprecated;         // Marked for removal in next major version
    std::string     deprecationNote;    // Why and what to use instead
    SemanticVersion addedIn;            // Version when this field was added
    SemanticVersion deprecatedIn;       // Version when deprecated (if applicable)

    struct Constraint {
        std::string minValue;
        std::string maxValue;
        std::vector<std::string> allowedValues;
    } constraint;
};

// ============================================================================
// SchemaChangeEntry — One change in the changelog
// ============================================================================
struct SchemaChangeEntry {
    enum class ChangeType : uint8_t {
        FieldAdded,
        FieldRemoved,
        FieldRenamed,
        FieldTypeChanged,
        FieldDeprecated,
        DefaultChanged,
        ConstraintChanged,
        EnumValueAdded,
        EnumValueRemoved,
        MigrationNote
    };

    ChangeType      type;
    SemanticVersion version;
    std::string     fieldName;
    std::string     oldValue;
    std::string     newValue;
    std::string     description;
    bool            breakingChange;
};

// ============================================================================
// ReasoningSchema — Complete versioned schema definition
// ============================================================================
struct ReasoningSchema {
    SemanticVersion         version;
    std::string             name;           // e.g. "reasoning_profile"
    std::vector<SchemaField> fields;
    std::vector<SchemaChangeEntry> changelog;

    // Compute hash for integrity
    uint64_t computeHash() const;
};

// ============================================================================
// Migration function type — transforms old-version JSON to new-version JSON
// ============================================================================
using SchemaMigrationFn = std::function<std::string(const std::string& oldJSON)>;

// ============================================================================
// MigrationPath — How to get from version A to version B
// ============================================================================
struct MigrationPath {
    SemanticVersion from;
    SemanticVersion to;
    SchemaMigrationFn migrator;
    std::string description;
};

// ============================================================================
// SchemaValidationResult — Outcome of validating data against a schema
// ============================================================================
struct SchemaValidationResult {
    bool            valid;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;      // e.g. deprecated fields used
    SemanticVersion detectedVersion;

    static SchemaValidationResult ok(const SemanticVersion& v) {
        SchemaValidationResult r;
        r.valid = true;
        r.detectedVersion = v;
        return r;
    }

    static SchemaValidationResult fail(const std::string& error) {
        SchemaValidationResult r;
        r.valid = false;
        r.errors.push_back(error);
        return r;
    }
};

// ============================================================================
// ReasoningSchemaRegistry — Singleton
// ============================================================================
class ReasoningSchemaRegistry {
public:
    static ReasoningSchemaRegistry& instance();

    // ---- Schema Management ----

    /// Register a schema version
    PatchResult registerSchema(const ReasoningSchema& schema);

    /// Get the current (latest) schema
    ReasoningSchema getCurrentSchema() const;

    /// Get a specific version of the schema
    bool getSchema(const SemanticVersion& version, ReasoningSchema& out) const;

    /// Get all registered schema versions
    std::vector<SemanticVersion> getRegisteredVersions() const;

    // ---- Version tracking ----
    SemanticVersion getCurrentVersion() const;
    SemanticVersion getMinSupportedVersion() const;
    void setMinSupportedVersion(const SemanticVersion& v);

    // ---- Validation ----

    /// Validate a profile JSON against the current schema
    SchemaValidationResult validate(const std::string& profileJSON) const;

    /// Validate against a specific version
    SchemaValidationResult validateAgainst(const std::string& profileJSON,
                                            const SemanticVersion& version) const;

    /// Detect the version of a profile JSON
    SemanticVersion detectVersion(const std::string& profileJSON) const;

    // ---- Migration ----

    /// Register a migration path from one version to another
    PatchResult registerMigration(const MigrationPath& path);

    /// Migrate a profile JSON from its version to the latest
    std::string migrateToLatest(const std::string& profileJSON) const;

    /// Migrate between specific versions
    std::string migrate(const std::string& profileJSON,
                        const SemanticVersion& from,
                        const SemanticVersion& to) const;

    /// Check if migration is possible between versions
    bool canMigrate(const SemanticVersion& from, const SemanticVersion& to) const;

    // ---- Changelog ----

    /// Get the full changelog
    std::vector<SchemaChangeEntry> getChangelog() const;

    /// Get changelog between two versions
    std::vector<SchemaChangeEntry> getChangelog(const SemanticVersion& from,
                                                 const SemanticVersion& to) const;

    /// Get all breaking changes between versions
    std::vector<SchemaChangeEntry> getBreakingChanges(const SemanticVersion& from,
                                                       const SemanticVersion& to) const;

    // ---- Serialization ----
    std::string schemaToJSON(const ReasoningSchema& schema) const;
    PatchResult schemaFromJSON(const std::string& json, ReasoningSchema& out) const;

    // ---- Statistics ----
    struct Stats {
        std::atomic<uint64_t> totalValidations{0};
        std::atomic<uint64_t> totalMigrations{0};
        std::atomic<uint64_t> validationFailures{0};
        std::atomic<uint64_t> migrationFailures{0};
    };
    const Stats& getStats() const { return m_stats; }

private:
    ReasoningSchemaRegistry();
    ~ReasoningSchemaRegistry() = default;
    ReasoningSchemaRegistry(const ReasoningSchemaRegistry&) = delete;
    ReasoningSchemaRegistry& operator=(const ReasoningSchemaRegistry&) = delete;

    /// Build the initial schema definitions
    void initializeBuiltinSchemas();

    /// Build the initial migration paths
    void initializeBuiltinMigrations();

    mutable std::mutex m_mutex;

    // Schema versions ordered by version
    std::vector<ReasoningSchema> m_schemas;

    // Migration paths
    std::vector<MigrationPath> m_migrations;

    // Minimum supported version for validation
    SemanticVersion m_minSupported;

    mutable Stats m_stats;
};
