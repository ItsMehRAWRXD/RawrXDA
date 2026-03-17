#include "rollback.hpp"
#include "json_types.hpp"
#include <cstdio>
#include <string>
#include "meta_learn.hpp"
// ---------- 1. detect regression ----------
bool Rollback::detectRegression() {
    // load before/after from perf_db.json via MetaLearn
    bool ok = false;
    JsonArray db = MetaLearn::loadDB(&ok);
    if (!ok) {
        fprintf(stderr, "[WARN] %s\\n", std::string("Rollback: unable to read perf_db.json";
        return false;
    }
    if (db.size() < 2)
        return false; // need at least 2 records

    // last commit = most recent, previous = second-most recent
    JsonObject last = db.last();
    JsonObject prev = db.at(db.size() - 2);

    double lastTPS = last.value("tps").toDouble();
    double prevTPS = prev.value("tps").toDouble();
    double lastPPL = last.value("ppl").toDouble();
    double prevPPL = prev.value("ppl").toDouble();

    // regression: TPS drop > 5 % OR PPL increase > 2 %
    bool tpsReg = lastTPS < prevTPS * 0.95;
    bool pplReg = lastPPL > prevPPL * 1.02;

    fprintf(stderr, "[INFO] Rollback::detectRegression tpsReg= pplReg= lastTPS= prevTPS= lastPPL= prevPPL=\n");

    return tpsReg || pplReg;
}

// ---------- 2. git revert ----------
bool Rollback::revertLastCommit() {
    void/*Process*/ proc;
    proc.start("git", {"revert", "--no-edit", "HEAD"});
    if (!proc.waitForFinished(60000)) {
        fprintf(stderr, "[WARN] %s\\n", std::string("Rollback: git revert timed out";
        return false;
    }
    if (proc.exitCode() != 0) {
        fprintf(stderr, "[WARN] %s\\n", std::string("Rollback: git revert failed" << proc.readAllStandardError();
        return false;
    }
    fprintf(stderr, "[INFO] Rollback: git revert SUCCESS\n");
    return true;
}

// ---------- 3. open GitHub issue ----------
bool Rollback::openIssue(const std::string& title, const std::string& body) {
    std::string token = qEnvironmentVariable("GITHUB_TOKEN");
    if (token.empty()) {
        fprintf(stderr, "[WARN] %s\\n", std::string("Rollback: GITHUB_TOKEN not set  skipping issue";
        return true; // allow in dev
    }

    JsonObject issue{
        {"title", title},
        {"body", body},
        {"labels", JsonArray{"regression", "auto"}}
    };

    void/*NetRequest*/ req(std::string/*url*/("https://api.github.com/repos/ItsMehRAWRXD/RawrXD-ModelLoader/issues"));
    req.setRawHeader("Authorization", std::string("Bearer %1") /* .arg( */token)/* .c_str() */);
    req.setHeader(void/*NetRequest*/::ContentTypeHeader, "application/json");

    // TODO: Implement Win32 HTTP via WinHTTP for GitHub API
    // void** reply = httpPost(req, JsonDoc(issue).toJson());
    bool ok = false; // Stub: HTTP not yet implemented
    if (!ok) {
        fprintf(stderr, "[WARN] Rollback: GitHub issue creation not yet implemented (requires WinHTTP)\n");
    } else {
        fprintf(stderr, "[INFO] Rollback: GitHub issue opened\n");
    }
    // reply cleanup deferred to connection destructor;
    return ok;
}


