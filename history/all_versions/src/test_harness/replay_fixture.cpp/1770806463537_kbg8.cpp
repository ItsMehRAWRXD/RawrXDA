// ============================================================================
// replay_fixture.cpp — Fixture Serialization & Validation
// ============================================================================
// Pattern:  Structured results, no exceptions
// Rule:     NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "replay_fixture.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <sstream>

using json = nlohmann::json;
namespace fs = std::filesystem;

// ============================================================================
// Helpers
// ============================================================================
namespace {

std::string isoNow() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    struct tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm_buf);
    return std::string(buf);
}

json tolerancesToJson(const ReplayTolerances& t) {
    return {
        {"minSuccessRate",       t.minSuccessRate},
        {"exactFileMatch",       t.exactFileMatch},
        {"allowExtraFixes",      t.allowExtraFixes},
        {"confidenceFloor",      t.confidenceFloor},
        {"maxSequenceDeviation", t.maxSequenceDeviation}
    };
}

ReplayTolerances tolerancesFromJson(const json& j) {
    ReplayTolerances t;
    if (j.contains("minSuccessRate"))       t.minSuccessRate       = j["minSuccessRate"].get<float>();
    if (j.contains("exactFileMatch"))       t.exactFileMatch       = j["exactFileMatch"].get<bool>();
    if (j.contains("allowExtraFixes"))      t.allowExtraFixes      = j["allowExtraFixes"].get<bool>();
    if (j.contains("confidenceFloor"))      t.confidenceFloor      = j["confidenceFloor"].get<float>();
    if (j.contains("maxSequenceDeviation")) t.maxSequenceDeviation = j["maxSequenceDeviation"].get<size_t>();
    return t;
}

json targetToJson(const FixtureTarget& ft) {
    return {
        {"id",       ft.id},
        {"path",     ft.path},
        {"context",  ft.context},
        {"category", ft.category}
    };
}

FixtureTarget targetFromJson(const json& j) {
    FixtureTarget ft;
    ft.id       = j.value("id", "");
    ft.path     = j.value("path", "");
    ft.context  = j.value("context", "");
    ft.category = j.value("category", "");
    return ft;
}

} // anonymous namespace

// ============================================================================
// ReplayFixture Implementation
// ============================================================================

bool ReplayFixture::save(const std::string& fixtureDir) const {
    fs::create_directories(fixtureDir);
    std::string filePath = (fs::path(fixtureDir) / "fixture.json").string();

    json j;
    j["id"]                     = id;
    j["description"]            = description;
    j["snapshotBeforeDir"]      = "snapshot_before";   // Relative within fixture
    j["snapshotAfterDir"]       = "snapshot_after";     // Relative within fixture
    j["journalPath"]            = "journal.replay";     // Relative within fixture

    // Strategy
    j["strategy"]["name"]                  = strategyName;
    j["strategy"]["promptTemplate"]        = strategyPromptTemplate;
    j["strategy"]["maxRetries"]            = strategyMaxRetries;
    j["strategy"]["maxParallel"]           = strategyMaxParallel;
    j["strategy"]["perTargetTimeoutMs"]    = strategyPerTargetTimeoutMs;
    j["strategy"]["autoVerify"]            = strategyAutoVerify;
    j["strategy"]["selfHeal"]              = strategySelfHeal;

    // Targets
    json tarr = json::array();
    for (const auto& t : targets) {
        tarr.push_back(targetToJson(t));
    }
    j["targets"] = tarr;

    // Tolerances
    j["tolerances"] = tolerancesToJson(tolerances);

    // Metadata
    j["createdDate"]            = createdDate.empty() ? isoNow() : createdDate;
    j["engineVersion"]          = engineVersion.empty() ? REPLAY_ENGINE_VERSION : engineVersion;
    j["journalSchemaVersion"]   = journalSchemaVersion;
    j["originalDurationMs"]     = originalDurationMs;

    std::ofstream out(filePath);
    if (!out.is_open()) return false;
    out << j.dump(2);
    return out.good();
}

ReplayFixture ReplayFixture::load(const std::string& fixtureDir) {
    ReplayFixture f;
    std::string filePath = (fs::path(fixtureDir) / "fixture.json").string();

    std::ifstream in(filePath);
    if (!in.is_open()) {
        f.id = "__LOAD_FAILED__";
        return f;
    }

    json j;
    in >> j;

    f.id                = j.value("id", "");
    f.description       = j.value("description", "");

    // Resolve relative paths against fixture directory
    fs::path base(fixtureDir);
    f.snapshotBeforeDir = (base / j.value("snapshotBeforeDir", "snapshot_before")).string();
    f.snapshotAfterDir  = (base / j.value("snapshotAfterDir", "snapshot_after")).string();
    f.journalPath       = (base / j.value("journalPath", "journal.replay")).string();

    // Strategy
    if (j.contains("strategy")) {
        const auto& s = j["strategy"];
        f.strategyName              = s.value("name", "");
        f.strategyPromptTemplate    = s.value("promptTemplate", "");
        f.strategyMaxRetries        = s.value("maxRetries", 3);
        f.strategyMaxParallel       = s.value("maxParallel", 4);
        f.strategyPerTargetTimeoutMs = s.value("perTargetTimeoutMs", 60000);
        f.strategyAutoVerify        = s.value("autoVerify", true);
        f.strategySelfHeal          = s.value("selfHeal", true);
    }

    // Targets
    if (j.contains("targets") && j["targets"].is_array()) {
        for (const auto& tj : j["targets"]) {
            f.targets.push_back(targetFromJson(tj));
        }
    }

    // Tolerances
    if (j.contains("tolerances")) {
        f.tolerances = tolerancesFromJson(j["tolerances"]);
    }

    // Metadata
    f.createdDate           = j.value("createdDate", "");
    f.engineVersion         = j.value("engineVersion", "");
    f.journalSchemaVersion  = j.value("journalSchemaVersion", (uint32_t)0);
    f.originalDurationMs    = j.value("originalDurationMs", (uint64_t)0);

    return f;
}

bool ReplayFixture::validateStructure() const {
    if (id.empty()) return false;
    if (!fs::exists(snapshotBeforeDir)) return false;
    if (!fs::exists(snapshotAfterDir)) return false;
    if (!fs::exists(journalPath)) return false;
    if (targets.empty()) return false;
    return true;
}

bool ReplayFixture::isSchemaCompatible() const {
    return journalSchemaVersion == REPLAY_JOURNAL_SCHEMA_VERSION;
}

// ============================================================================
// FixtureManifest Implementation
// ============================================================================

FixtureManifest FixtureManifest::load(const std::string& manifestPath) {
    FixtureManifest m;

    std::ifstream in(manifestPath);
    if (!in.is_open()) return m;

    json j;
    in >> j;

    if (j.contains("fixtures") && j["fixtures"].is_array()) {
        for (const auto& fj : j["fixtures"]) {
            FixtureManifestEntry e;
            e.id          = fj.value("id", "");
            e.path        = fj.value("path", "");
            e.description = fj.value("description", "");
            e.tag         = fj.value("tag", "full");
            e.enabled     = fj.value("enabled", true);
            m.fixtures.push_back(e);
        }
    }

    return m;
}

bool FixtureManifest::save(const std::string& manifestPath) const {
    json j;
    json arr = json::array();
    for (const auto& e : fixtures) {
        arr.push_back({
            {"id",          e.id},
            {"path",        e.path},
            {"description", e.description},
            {"tag",         e.tag},
            {"enabled",     e.enabled}
        });
    }
    j["fixtures"] = arr;
    j["schemaVersion"] = REPLAY_JOURNAL_SCHEMA_VERSION;

    std::ofstream out(manifestPath);
    if (!out.is_open()) return false;
    out << j.dump(2);
    return out.good();
}

std::vector<FixtureManifestEntry> FixtureManifest::getByTag(const std::string& tag) const {
    std::vector<FixtureManifestEntry> result;
    for (const auto& e : fixtures) {
        if (e.tag == tag && e.enabled) {
            result.push_back(e);
        }
    }
    return result;
}

std::vector<FixtureManifestEntry> FixtureManifest::getEnabled() const {
    std::vector<FixtureManifestEntry> result;
    for (const auto& e : fixtures) {
        if (e.enabled) {
            result.push_back(e);
        }
    }
    return result;
}
