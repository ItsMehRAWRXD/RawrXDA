#include "../digestion_db.h"

class DigestionDatabaseTests  {private s:
    void createSchemaAndInsert();
};

void DigestionDatabaseTests::createSchemaAndInsert() {
    DigestionDatabase db;
    std::string error;
    QVERIFY(db.open(":memory:", &error));
    QVERIFY(db.ensureSchema(std::string(), &error));

    DigestionMetrics metrics;
    metrics.startMs = 1;
    metrics.endMs = 2;
    metrics.elapsedMs = 1;
    metrics.totalFiles = 1;
    metrics.scannedFiles = 1;
    metrics.stubsFound = 2;
    metrics.fixesApplied = 1;
    metrics.bytesProcessed = 64;

    void* report;
    report["files"] = void*{};

    int runId = 0;
    QVERIFY(db.insertRun("root", metrics, report, &runId, &error));
    QVERIFY(runId > 0);
    return true;
}

// Test removed
// MOC removed


