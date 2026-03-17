#include "rollback.hpp"
#include "json_types.hpp"
#include "process_utils.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <nlohmann/json.hpp>
#include "meta_learn.hpp"

// ---------- 1. detect regression ----------
bool Rollback::detectRegression() {
    bool ok = false;
    nlohmann::json db = MetaLearn::loadDB(&ok);
    if (!ok) {
        fprintf(stderr, "[WARN] Rollback: unable to read perf_db.json\n");
        return false;
    }
    if (!db.is_array() || db.size() < 2)
        return false;

    const auto& last = db[db.size() - 1];
    const auto& prev = db[db.size() - 2];

    double lastTPS = last.value("tps", 0.0);
    double prevTPS = prev.value("tps", 0.0);
    double lastPPL = last.value("ppl", 0.0);
    double prevPPL = prev.value("ppl", 0.0);

    bool tpsReg = (prevTPS > 0.0) && (lastTPS < prevTPS * 0.95);
    bool pplReg = (prevPPL > 0.0) && (lastPPL > prevPPL * 1.02);

    fprintf(stderr, "[INFO] Rollback::detectRegression tpsReg=%d pplReg=%d lastTPS=%.2f prevTPS=%.2f lastPPL=%.2f prevPPL=%.2f\n",
            tpsReg, pplReg, lastTPS, prevTPS, lastPPL, prevPPL);

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
