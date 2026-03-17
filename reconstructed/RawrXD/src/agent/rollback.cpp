/**
 * @file rollback.cpp
 * @brief Rollback implementation — Qt-free, nlohmann-free (C++20 / Win32)
 *
 * Detects performance regressions, reverts commits, opens GitHub issues.
 * Uses json_types.hpp for serialization, manual parse for perf_db.json.
 */

#include "rollback.hpp"
#include "json_types.hpp"
#include "process_utils.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

// ═══════════════════════════════════════════════════════════════════════════
// Minimal perf_db.json parser — extracts tps/ppl from last two entries
// The file is a JSON array of objects: [{ "tps": 42.0, "ppl": 3.2, ... }, ...]
// We only need the numeric tps/ppl fields from the last two entries.
// ═══════════════════════════════════════════════════════════════════════════

struct PerfEntry {
    double tps = 0.0;
    double ppl = 0.0;
};

/// Extract a numeric value after "key": in a JSON string fragment
static double extractDouble(const std::string& obj, const char* key) {
    std::string pat = std::string("\"") + key + "\"";
    size_t pos = obj.find(pat);
    if (pos == std::string::npos) return 0.0;
    pos = obj.find(':', pos + pat.size());
    if (pos == std::string::npos) return 0.0;
    ++pos;
    // skip whitespace
    while (pos < obj.size() && (obj[pos] == ' ' || obj[pos] == '\t')) ++pos;
    if (pos >= obj.size()) return 0.0;
    return std::strtod(obj.c_str() + pos, nullptr);
}

/// Parse perf_db.json: find last two top-level objects in the array
static bool loadPerfEntries(std::vector<PerfEntry>& out) {
    // Resolve path — check CWD/perf_db.json, then CWD/data/perf_db.json
    std::string path;
    if (fs::exists("perf_db.json"))
        path = "perf_db.json";
    else if (fs::exists("data/perf_db.json"))
        path = "data/perf_db.json";
    else {
        // Try environment override
        std::string envPath = getEnvVar("RAWRXD_PERF_DB");
        if (!envPath.empty() && fs::exists(envPath))
            path = envPath;
        else
            return false;
    }

    std::string raw = fileutil::readAll(path);
    if (raw.empty()) return false;

    // Find all top-level { ... } blocks inside the outer [ ... ]
    int depth = 0;
    size_t objStart = std::string::npos;
    std::vector<std::string> objects;

    for (size_t i = 0; i < raw.size(); ++i) {
        char c = raw[i];
        if (c == '{') {
            if (depth == 1 || (depth == 0 && objStart == std::string::npos)) {
                if (depth == 0) {
                    // hadn't seen '[' yet; this might be the first object
                }
                objStart = i;
            }
            ++depth;
        } else if (c == '}') {
            --depth;
            if ((depth == 0 || depth == 1) && objStart != std::string::npos) {
                // We only keep the last N objects to save memory
                objects.push_back(raw.substr(objStart, i - objStart + 1));
                // Keep only last 2 to conserve memory on large DBs
                if (objects.size() > 2)
                    objects.erase(objects.begin());
                objStart = std::string::npos;
            }
        } else if (c == '[' && depth == 0) {
            ++depth; // enter array
        } else if (c == ']' && depth == 1) {
            break;
        }
    }

    for (const auto& obj : objects) {
        PerfEntry e;
        e.tps = extractDouble(obj, "tps");
        e.ppl = extractDouble(obj, "ppl");
        out.push_back(e);
    }
    return !out.empty();
}

// ---------- 1. detect regression ----------
bool Rollback::detectRegression() {
    std::vector<PerfEntry> entries;
    if (!loadPerfEntries(entries)) {
        fprintf(stderr, "[WARN] Rollback: unable to read perf_db.json\n");
        return false;
    }
    if (entries.size() < 2)
        return false;

    const PerfEntry& last = entries[entries.size() - 1];
    const PerfEntry& prev = entries[entries.size() - 2];

    bool tpsReg = (prev.tps > 0.0) && (last.tps < prev.tps * 0.95);
    bool pplReg = (prev.ppl > 0.0) && (last.ppl > prev.ppl * 1.02);

    fprintf(stderr, "[INFO] Rollback::detectRegression tpsReg=%d pplReg=%d lastTPS=%.2f prevTPS=%.2f lastPPL=%.2f prevPPL=%.2f\n",
            tpsReg, pplReg, last.tps, prev.tps, last.ppl, prev.ppl);

    return tpsReg || pplReg;
}

// ---------- 2. git revert ----------
bool Rollback::revertLastCommit() {
    ProcResult pr = proc::run("git", {"revert", "--no-edit", "HEAD"}, 60000);
    if (pr.timedOut) {
        fprintf(stderr, "[WARN] Rollback: git revert timed out\n");
        return false;
    }
    if (pr.exitCode != 0) {
        fprintf(stderr, "[WARN] Rollback: git revert failed: %s\n", pr.stderrStr.c_str());
        return false;
    }
    fprintf(stderr, "[INFO] Rollback: git revert SUCCESS\n");
    return true;
}

// ---------- 3. open GitHub issue ----------
bool Rollback::openIssue(const std::string& title, const std::string& body) {
    std::string token = getEnvVar("GITHUB_TOKEN");
    if (token.empty()) {
        fprintf(stderr, "[WARN] Rollback: GITHUB_TOKEN not set, skipping issue\n");
        return true; // allow in dev
    }

    JsonObject issue;
    issue["title"] = JsonValue(title);
    issue["body"] = JsonValue(body);

    JsonArray labels;
    labels.push_back(JsonValue("regression"));
    labels.push_back(JsonValue("auto"));
    issue["labels"] = JsonValue(std::move(labels));

    JsonDoc doc(issue);
    std::string payload = doc.toJson();

    http::Response resp = http::post(
        "https://api.github.com/repos/ItsMehRAWRXD/RawrXD-ModelLoader/issues",
        payload,
        {
            {"Authorization", "Bearer " + token},
            {"Content-Type", "application/json"},
            {"Accept", "application/vnd.github+json"}
        }
    );

    if (!resp.ok()) {
        fprintf(stderr, "[WARN] Rollback: GitHub issue failed (HTTP %d): %s\n",
                resp.statusCode, resp.error.c_str());
        return false;
    }

    fprintf(stderr, "[INFO] Rollback: GitHub issue opened\n");
    return true;
}
