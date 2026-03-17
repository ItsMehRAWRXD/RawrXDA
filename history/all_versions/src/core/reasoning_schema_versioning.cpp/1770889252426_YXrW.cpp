// ============================================================================
// reasoning_schema_versioning.cpp — Versioned Reasoning Schema System
// ============================================================================
//
// Full implementation: built-in schema versions, migration paths,
// validation, changelog tracking, and version detection.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "reasoning_schema_versioning.hpp"
#include <algorithm>
#include <sstream>
#include <cstring>
#include <cctype>

// ============================================================================
// Singleton
// ============================================================================
ReasoningSchemaRegistry& ReasoningSchemaRegistry::instance() {
    static ReasoningSchemaRegistry s;
    return s;
}

ReasoningSchemaRegistry::ReasoningSchemaRegistry() {
    m_minSupported = SemanticVersion(1, 0, 0);
    initializeBuiltinSchemas();
    initializeBuiltinMigrations();
}

// ============================================================================
// Schema hash
// ============================================================================
uint64_t ReasoningSchema::computeHash() const {
    uint64_t hash = 0xCBF29CE484222325ULL;
    auto mix = [&hash](const std::string& s) {
        for (char c : s) { hash ^= c; hash *= 0x100000001B3ULL; }
    };
    mix(name);
    mix(version.toString());
    for (const auto& f : fields) {
        mix(f.name);
        mix(f.type);
    }
    return hash;
}

// ============================================================================
// Built-in Schema Definitions
// ============================================================================
void ReasoningSchemaRegistry::initializeBuiltinSchemas() {
    // ---- Schema v1.0.0 (baseline) ----
    {
        ReasoningSchema s;
        s.version = {1, 0, 0};
        s.name = "reasoning_profile";

        s.fields.push_back({"name", "string", "Profile name", "normal", true, false, "",
                            {1,0,0}, {}, {}});
        s.fields.push_back({"reasoningDepth", "int", "Reasoning depth 0-8", "1", true, false, "",
                            {1,0,0}, {}, {"0", "8", {}}});
        s.fields.push_back({"mode", "enum", "Operating mode", "Normal", true, false, "",
                            {1,0,0}, {}, {"", "",
                            {"Fast","Normal","Deep","Critical","Swarm","Adaptive"}}});
        s.fields.push_back({"enableCritic", "bool", "Use Critic agent", "true", false, false, "",
                            {1,0,0}, {}, {}});
        s.fields.push_back({"enableThinker", "bool", "Use Thinker agent", "true", false, false, "",
                            {1,0,0}, {}, {}});
        s.fields.push_back({"enableSynthesizer", "bool", "Use Synthesizer", "true", false, false, "",
                            {1,0,0}, {}, {}});
        s.fields.push_back({"confidenceThreshold", "float", "Min confidence for auto-accept",
                            "0.7", false, false, "", {1,0,0}, {}, {"0.0", "1.0", {}}});

        m_schemas.push_back(std::move(s));
    }

    // ---- Schema v1.1.0 (adaptive + thermal) ----
    {
        ReasoningSchema s;
        s.version = {1, 1, 0};
        s.name = "reasoning_profile";

        // Copy all v1.0.0 fields
        s.fields = m_schemas[0].fields;

        // Add new fields
        s.fields.push_back({"adaptiveEnabled", "bool", "Enable adaptive mode", "false",
                            false, false, "", {1,1,0}, {}, {}});
        s.fields.push_back({"adaptiveStrategy", "enum", "Adaptive strategy",
                            "Hybrid", false, false, "", {1,1,0}, {},
                            {"", "", {"LatencyAware","ThermalAware","ConfidenceBased","Hybrid"}}});
        s.fields.push_back({"thermalEnabled", "bool", "Enable thermal monitoring", "false",
                            false, false, "", {1,1,0}, {}, {}});
        s.fields.push_back({"latencyTargetMs", "float", "Target latency in ms", "2000",
                            false, false, "", {1,1,0}, {}, {"100", "30000", {}}});

        s.changelog.push_back({SchemaChangeEntry::ChangeType::FieldAdded,
                               {1,1,0}, "adaptiveEnabled", "", "false",
                               "Added adaptive mode master switch", false});
        s.changelog.push_back({SchemaChangeEntry::ChangeType::FieldAdded,
                               {1,1,0}, "thermalEnabled", "", "false",
                               "Added thermal monitoring", false});

        m_schemas.push_back(std::move(s));
    }

    // ---- Schema v2.0.0 (swarm + self-tune + workspace profiles) ----
    {
        ReasoningSchema s;
        s.version = {2, 0, 0};
        s.name = "reasoning_profile";

        // Copy all v1.1.0 fields
        s.fields = m_schemas[1].fields;

        // Add swarm fields
        s.fields.push_back({"swarmEnabled", "bool", "Enable swarm reasoning", "false",
                            false, false, "", {2,0,0}, {}, {}});
        s.fields.push_back({"swarmAgentCount", "int", "Number of swarm agents", "3",
                            false, false, "", {2,0,0}, {}, {"2", "16", {}}});
        s.fields.push_back({"swarmMode", "enum", "Swarm distribution strategy",
                            "ParallelVote", false, false, "", {2,0,0}, {},
                            {"", "", {"ParallelVote","Sequential","Tournament","Ensemble"}}});

        // Add self-tune fields
        s.fields.push_back({"selfTuneEnabled", "bool", "Enable PID self-tuning", "false",
                            false, false, "", {2,0,0}, {}, {}});
        s.fields.push_back({"selfTuneObjective", "enum", "Self-tune optimization target",
                            "BalancedQoS", false, false, "", {2,0,0}, {},
                            {"", "", {"MinLatency","MaxQuality","BalancedQoS","MinCost","MaxThroughput"}}});

        // Add workspace-level fields
        s.fields.push_back({"workspacePath", "string", "Associated workspace path", "",
                            false, false, "", {2,0,0}, {}, {}});
        s.fields.push_back({"autoLoadOnOpen", "bool", "Auto-apply when workspace opens", "true",
                            false, false, "", {2,0,0}, {}, {}});

        // Add deterministic reproducibility fields
        s.fields.push_back({"deterministicSeed", "int", "Master seed for reproducibility", "0",
                            false, false, "", {2,0,0}, {}, {"0", "18446744073709551615", {}}});
        s.fields.push_back({"enforceOrdering", "bool", "Force deterministic task ordering", "true",
                            false, false, "", {2,0,0}, {}, {}});

        // Deprecate old field
        // (none to deprecate in this version)

        s.changelog.push_back({SchemaChangeEntry::ChangeType::FieldAdded,
                               {2,0,0}, "swarmEnabled", "", "false",
                               "Added swarm reasoning support", false});
        s.changelog.push_back({SchemaChangeEntry::ChangeType::FieldAdded,
                               {2,0,0}, "selfTuneEnabled", "", "false",
                               "Added PID self-tuning controller", false});
        s.changelog.push_back({SchemaChangeEntry::ChangeType::FieldAdded,
                               {2,0,0}, "deterministicSeed", "", "0",
                               "Added deterministic reproducibility seed", false});
        s.changelog.push_back({SchemaChangeEntry::ChangeType::FieldAdded,
                               {2,0,0}, "workspacePath", "", "",
                               "Added per-workspace profile binding", false});

        m_schemas.push_back(std::move(s));
    }

    // Sort schemas by version
    std::sort(m_schemas.begin(), m_schemas.end(),
              [](const ReasoningSchema& a, const ReasoningSchema& b) {
                  return a.version < b.version;
              });
}

// ============================================================================
// Built-in Migrations
// ============================================================================
void ReasoningSchemaRegistry::initializeBuiltinMigrations() {
    // v1.0.0 → v1.1.0: add default adaptive/thermal fields
    m_migrations.push_back({
        {1, 0, 0}, {1, 1, 0},
        [](const std::string& json) -> std::string {
            // Insert adaptive and thermal defaults before closing brace
            std::string result = json;
            size_t close = result.rfind('}');
            if (close != std::string::npos) {
                std::string additions =
                    ",\n  \"adaptiveEnabled\": false,"
                    "\n  \"adaptiveStrategy\": \"Hybrid\","
                    "\n  \"thermalEnabled\": false,"
                    "\n  \"latencyTargetMs\": 2000.0,"
                    "\n  \"schemaVersion\": \"1.1.0\"";
                result.insert(close, additions);
            }
            return result;
        },
        "Add adaptive and thermal fields with defaults"
    });

    // v1.1.0 → v2.0.0: add swarm, self-tune, workspace, deterministic fields
    m_migrations.push_back({
        {1, 1, 0}, {2, 0, 0},
        [](const std::string& json) -> std::string {
            std::string result = json;
            size_t close = result.rfind('}');
            if (close != std::string::npos) {
                std::string additions =
                    ",\n  \"swarmEnabled\": false,"
                    "\n  \"swarmAgentCount\": 3,"
                    "\n  \"swarmMode\": \"ParallelVote\","
                    "\n  \"selfTuneEnabled\": false,"
                    "\n  \"selfTuneObjective\": \"BalancedQoS\","
                    "\n  \"workspacePath\": \"\","
                    "\n  \"autoLoadOnOpen\": true,"
                    "\n  \"deterministicSeed\": 0,"
                    "\n  \"enforceOrdering\": true,"
                    "\n  \"schemaVersion\": \"2.0.0\"";
                result.insert(close, additions);
            }
            return result;
        },
        "Add swarm, self-tune, workspace, and deterministic fields"
    });

    // v1.0.0 → v2.0.0: composite migration
    m_migrations.push_back({
        {1, 0, 0}, {2, 0, 0},
        [this](const std::string& json) -> std::string {
            // Chain: v1.0.0 → v1.1.0 → v2.0.0
            std::string intermediate = migrate(json, {1,0,0}, {1,1,0});
            return migrate(intermediate, {1,1,0}, {2,0,0});
        },
        "Full migration from v1.0.0 to v2.0.0"
    });
}

// ============================================================================
// Schema Registration
// ============================================================================
PatchResult ReasoningSchemaRegistry::registerSchema(const ReasoningSchema& schema) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check for duplicate
    for (const auto& s : m_schemas) {
        if (s.version == schema.version) {
            return PatchResult::error("Schema version already registered");
        }
    }

    m_schemas.push_back(schema);
    std::sort(m_schemas.begin(), m_schemas.end(),
              [](const ReasoningSchema& a, const ReasoningSchema& b) {
                  return a.version < b.version;
              });

    return PatchResult::ok("Schema registered");
}

// ============================================================================
// Schema Queries
// ============================================================================
ReasoningSchema ReasoningSchemaRegistry::getCurrentSchema() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_schemas.empty()) return {};
    return m_schemas.back();
}

bool ReasoningSchemaRegistry::getSchema(const SemanticVersion& version,
                                         ReasoningSchema& out) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& s : m_schemas) {
        if (s.version == version) { out = s; return true; }
    }
    return false;
}

std::vector<SemanticVersion> ReasoningSchemaRegistry::getRegisteredVersions() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<SemanticVersion> versions;
    for (const auto& s : m_schemas) {
        versions.push_back(s.version);
    }
    return versions;
}

SemanticVersion ReasoningSchemaRegistry::getCurrentVersion() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_schemas.empty()) return {};
    return m_schemas.back().version;
}

SemanticVersion ReasoningSchemaRegistry::getMinSupportedVersion() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_minSupported;
}

void ReasoningSchemaRegistry::setMinSupportedVersion(const SemanticVersion& v) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_minSupported = v;
}

// ============================================================================
// Version Detection
// ============================================================================
SemanticVersion ReasoningSchemaRegistry::detectVersion(const std::string& json) const {
    // Look for "schemaVersion" field
    size_t pos = json.find("\"schemaVersion\"");
    if (pos != std::string::npos) {
        size_t colon = json.find(':', pos);
        size_t q1 = json.find('"', colon + 1);
        size_t q2 = json.find('"', q1 + 1);
        if (q1 != std::string::npos && q2 != std::string::npos) {
            return SemanticVersion::parse(json.substr(q1 + 1, q2 - q1 - 1));
        }
    }

    // Heuristic: detect by field presence
    if (json.find("\"swarmEnabled\"") != std::string::npos ||
        json.find("\"deterministicSeed\"") != std::string::npos) {
        return {2, 0, 0};
    }
    if (json.find("\"adaptiveEnabled\"") != std::string::npos ||
        json.find("\"thermalEnabled\"") != std::string::npos) {
        return {1, 1, 0};
    }
    return {1, 0, 0};
}

// ============================================================================
// Validation
// ============================================================================
SchemaValidationResult ReasoningSchemaRegistry::validate(const std::string& json) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.totalValidations.fetch_add(1, std::memory_order_relaxed);

    if (m_schemas.empty()) {
        return SchemaValidationResult::fail("No schemas registered");
    }

    SemanticVersion detectedVersion = detectVersion(json);
    return validateAgainst(json, detectedVersion);
}

SchemaValidationResult ReasoningSchemaRegistry::validateAgainst(
    const std::string& json,
    const SemanticVersion& version) const
{
    // Find the schema
    const ReasoningSchema* schema = nullptr;
    for (const auto& s : m_schemas) {
        if (s.version == version) { schema = &s; break; }
    }

    if (!schema) {
        m_stats.validationFailures.fetch_add(1, std::memory_order_relaxed);
        return SchemaValidationResult::fail(
            "No schema registered for version " + version.toString());
    }

    SchemaValidationResult result = SchemaValidationResult::ok(version);

    // Check required fields
    for (const auto& field : schema->fields) {
        if (field.required) {
            std::string searchKey = "\"" + field.name + "\"";
            if (json.find(searchKey) == std::string::npos) {
                result.errors.push_back("Missing required field: " + field.name);
            }
        }
        if (field.deprecated) {
            std::string searchKey = "\"" + field.name + "\"";
            if (json.find(searchKey) != std::string::npos) {
                result.warnings.push_back(
                    "Using deprecated field '" + field.name + "': " + field.deprecationNote);
            }
        }
    }

    if (!result.errors.empty()) {
        result.valid = false;
        m_stats.validationFailures.fetch_add(1, std::memory_order_relaxed);
    }

    return result;
}

// ============================================================================
// Migration
// ============================================================================
PatchResult ReasoningSchemaRegistry::registerMigration(const MigrationPath& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_migrations.push_back(path);
    return PatchResult::ok("Migration registered");
}

std::string ReasoningSchemaRegistry::migrateToLatest(const std::string& json) const {
    SemanticVersion from = detectVersion(json);
    SemanticVersion latest = getCurrentVersion();

    if (from == latest) return json; // Already latest

    m_stats.totalMigrations.fetch_add(1, std::memory_order_relaxed);
    return migrate(json, from, latest);
}

std::string ReasoningSchemaRegistry::migrate(
    const std::string& json,
    const SemanticVersion& from,
    const SemanticVersion& to) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // Find direct migration path
    for (const auto& m : m_migrations) {
        if (m.from == from && m.to == to && m.migrator) {
            return m.migrator(json);
        }
    }

    // Try chained migration (A→B→C)
    for (const auto& m1 : m_migrations) {
        if (m1.from == from && m1.migrator) {
            for (const auto& m2 : m_migrations) {
                if (m2.from == m1.to && m2.to == to && m2.migrator) {
                    std::string intermediate = m1.migrator(json);
                    return m2.migrator(intermediate);
                }
            }
        }
    }

    m_stats.migrationFailures.fetch_add(1, std::memory_order_relaxed);
    return json; // No migration found — return unmodified
}

bool ReasoningSchemaRegistry::canMigrate(const SemanticVersion& from,
                                          const SemanticVersion& to) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& m : m_migrations) {
        if (m.from == from && m.to == to) return true;
    }
    // Check chained
    for (const auto& m1 : m_migrations) {
        if (m1.from == from) {
            for (const auto& m2 : m_migrations) {
                if (m2.from == m1.to && m2.to == to) return true;
            }
        }
    }
    return false;
}

// ============================================================================
// Changelog
// ============================================================================
std::vector<SchemaChangeEntry> ReasoningSchemaRegistry::getChangelog() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<SchemaChangeEntry> all;
    for (const auto& s : m_schemas) {
        all.insert(all.end(), s.changelog.begin(), s.changelog.end());
    }
    return all;
}

std::vector<SchemaChangeEntry> ReasoningSchemaRegistry::getChangelog(
    const SemanticVersion& from,
    const SemanticVersion& to) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<SchemaChangeEntry> filtered;
    for (const auto& s : m_schemas) {
        if (s.version > from && s.version <= to) {
            filtered.insert(filtered.end(), s.changelog.begin(), s.changelog.end());
        }
    }
    return filtered;
}

std::vector<SchemaChangeEntry> ReasoningSchemaRegistry::getBreakingChanges(
    const SemanticVersion& from,
    const SemanticVersion& to) const
{
    auto all = getChangelog(from, to);
    std::vector<SchemaChangeEntry> breaking;
    for (const auto& e : all) {
        if (e.breakingChange) breaking.push_back(e);
    }
    return breaking;
}

// ============================================================================
// Serialization
// ============================================================================
std::string ReasoningSchemaRegistry::schemaToJSON(const ReasoningSchema& schema) const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"name\": \"" << schema.name << "\",\n";
    ss << "  \"version\": \"" << schema.version.toString() << "\",\n";
    ss << "  \"fieldCount\": " << schema.fields.size() << ",\n";
    ss << "  \"fields\": [\n";

    for (size_t i = 0; i < schema.fields.size(); ++i) {
        const auto& f = schema.fields[i];
        ss << "    {\"name\": \"" << f.name << "\", \"type\": \"" << f.type
           << "\", \"required\": " << (f.required ? "true" : "false")
           << ", \"deprecated\": " << (f.deprecated ? "true" : "false")
           << ", \"default\": \"" << f.defaultValue << "\"}";
        if (i + 1 < schema.fields.size()) ss << ",";
        ss << "\n";
    }

    ss << "  ]\n}\n";
    return ss.str();
}

PatchResult ReasoningSchemaRegistry::schemaFromJSON(const std::string& json,
                                                     ReasoningSchema& out) const {
    // Manual JSON parser for reasoning schema format
    if (json.empty() || json[0] != '{') {
        return PatchResult::error("Invalid JSON: expected '{'" , -1);
    }

    auto extractStr = [&json](const std::string& key) -> std::string {
        std::string pat = "\"" + key + "\": \"";
        auto pos = json.find(pat);
        if (pos == std::string::npos) {
            pat = "\"" + key + "\":\"";
            pos = json.find(pat);
        }
        if (pos == std::string::npos) return "";
        auto start = pos + pat.size();
        auto end = json.find('"', start);
        if (end == std::string::npos) return "";
        return json.substr(start, end - start);
    };

    auto extractInt = [&json](const std::string& key) -> int {
        std::string pat = "\"" + key + "\": ";
        auto pos = json.find(pat);
        if (pos == std::string::npos) {
            pat = "\"" + key + "\":";
            pos = json.find(pat);
        }
        if (pos == std::string::npos) return 0;
        auto start = pos + pat.size();
        return atoi(json.c_str() + start);
    };

    out.name = extractStr("name");
    if (out.name.empty()) {
        return PatchResult::error("Missing 'name' field", -2);
    }

    // Parse version string "M.m.p.b"
    std::string verStr = extractStr("version");
    if (!verStr.empty()) {
        sscanf(verStr.c_str(), "%u.%u.%u.%u",
               &out.version.major, &out.version.minor,
               &out.version.patch, &out.version.build);
    }

    // Parse fields array
    out.fields.clear();
    size_t fieldsStart = json.find("\"fields\"");
    if (fieldsStart != std::string::npos) {
        size_t arrStart = json.find('[', fieldsStart);
        size_t arrEnd = json.find(']', arrStart);
        if (arrStart != std::string::npos && arrEnd != std::string::npos) {
            std::string fieldsJson = json.substr(arrStart, arrEnd - arrStart + 1);
            size_t fpos = 0;
            while ((fpos = fieldsJson.find('{', fpos)) != std::string::npos) {
                size_t fend = fieldsJson.find('}', fpos);
                if (fend == std::string::npos) break;

                std::string entry = fieldsJson.substr(fpos, fend - fpos + 1);

                ReasoningSchemaField field;
                // Extract field properties from entry
                auto eStr = [&entry](const std::string& key) -> std::string {
                    std::string p = "\"" + key + "\": \"";
                    auto pp = entry.find(p);
                    if (pp == std::string::npos) { p = "\"" + key + "\":\""; pp = entry.find(p); }
                    if (pp == std::string::npos) return "";
                    auto s = pp + p.size();
                    auto e = entry.find('"', s);
                    return (e != std::string::npos) ? entry.substr(s, e - s) : "";
                };

                field.name = eStr("name");
                field.type = eStr("type");
                field.defaultValue = eStr("default");
                field.required = (entry.find("\"required\": true") != std::string::npos ||
                                  entry.find("\"required\":true") != std::string::npos);
                field.deprecated = (entry.find("\"deprecated\": true") != std::string::npos ||
                                    entry.find("\"deprecated\":true") != std::string::npos);

                if (!field.name.empty()) {
                    out.fields.push_back(field);
                }
                fpos = fend + 1;
            }
        }
    }

    return PatchResult::ok("Schema parsed successfully");
}
