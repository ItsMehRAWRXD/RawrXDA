/**
 * DigestionDatabase unit tests — C++20 only (no Qt).
 */
#include "../digestion_db.h"
#include <cassert>
#include <cstdio>
#include <string>

#define TEST_VERIFY(cond) do { \
    if (!(cond)) { std::fprintf(stderr, "FAIL: %s\n", #cond); return false; } \
} while(0)

static int g_run = 0;
static int g_fail = 0;

static bool createSchemaAndInsert() {
    DigestionDatabase db;
    std::string error;

    TEST_VERIFY(db.open(":memory:", &error));
    TEST_VERIFY(db.ensureSchema(std::string(), &error));

    DigestionMetrics metrics;
    metrics.startMs = 1;
    metrics.endMs = 2;
    metrics.elapsedMs = 1;
    metrics.totalFiles = 1;
    metrics.scannedFiles = 1;
    metrics.stubsFound = 2;
    metrics.fixesApplied = 1;
    metrics.bytesProcessed = 64;

    std::string reportJson = "{\"files\":[]}";
    int runId = 0;
    TEST_VERIFY(db.insertRun("root", metrics, reportJson, &runId, &error));
    TEST_VERIFY(runId > 0);

    return true;
}

int main() {
    std::fprintf(stdout, "DigestionDatabase tests (C++20, no Qt)\n");
    ++g_run;
    if (!createSchemaAndInsert()) { ++g_fail; }
    std::fprintf(stdout, "Done: %d run, %d failed\n", g_run, g_fail);
    return g_fail ? 1 : 0;
}
