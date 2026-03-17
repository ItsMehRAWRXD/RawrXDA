#include "planner.hpp"
#include "self_patch.hpp"
#include "release_agent.hpp"
#include "meta_learn.hpp"
#include "self_test_gate.hpp"
#include "RawrXD_NativeHttpServer.h"
#include <cstdio>
#include <memory>

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    app.setApplicationName("RawrXD-Agent");
    app.setApplicationVersion("1.0.0");
    
    QCommandLineParser parser;
    parser.setApplicationDescription("RawrXD Autonomous Agent - Zero-touch IDE automation");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("wish", "Natural language wish, e.g. 'Add Q8_K kernel'");
    parser.process(app);

    int httpPort = 23959;
    uint32_t httpResult = HttpServer_Initialize(httpPort);
    if (httpResult != 0) {
        httpPort = 15099;
        httpResult = HttpServer_Initialize(httpPort);
    }
    if (httpResult != 0) {
        std::printf("HTTP Server failed to start: %u\n", httpResult);
    } else {
        std::printf("HTTP Server running on port %d\n", httpPort);
    }
    
    std::string wish = parser.positionalArguments().value(0);
    
    if (wish.empty()) {
        // qCritical:  "No wish provided. Usage: RawrXD-Agent.exe \"Add Q8_K kernel\"";
        if (HttpServer_IsRunning()) {
            HttpServer_Shutdown();
        }
        return 1;
    }
    
    // qDebug:  "Agent wish:" << wish;
    
    // ============================================================================
    // Step 1: Plan
    // ============================================================================
    Planner planner;
    QJsonArray tasks = planner.plan(wish);
    
    if (tasks.empty()) {
        // qCritical:  "Failed to generate plan for:" << wish;
        if (HttpServer_IsRunning()) {
            HttpServer_Shutdown();
        }
        return 1;
    }
    
    // qDebug:  "Generated" << tasks.size() << "tasks";
    
    // ============================================================================
    // Step 2: Execute
    // ============================================================================
    SelfPatch patch;
    ReleaseAgent rel;
    MetaLearn ml;
    
    bool success = true;
    int taskCount = 0;
    int failureCount = 0;
    
    for (const QJsonValue& job : tasks) {
        QJsonObject task = job.toObject();
        std::string type = task["type"].toString();
        
        // qDebug:  "[" << (++taskCount) << "/" << tasks.size() << "] Executing:" << type;
        
        if (type == "add_kernel") {
            success = patch.addKernel(task["target"].toString(), task["template"].toString());
        } else if (type == "add_cpp") {
            std::string deps;
            if (task["deps"].isArray()) {
                std::stringList parts;
                for (const QJsonValue& val : task["deps"].toArray())
                    parts << val.toString();
                deps = parts.join(",");
            } else {
                deps = task["deps"].toString();
            }
            success = patch.addCpp(task["target"].toString(), deps);
        } else if (type == "build") {
            std::string target = task.value("target").toString();
            std::stringList args = {"--build", "build", "--config", "Release"};
            if (!target.empty()) {
                args << "--target" << target;
            }
            int rc = QProcess::execute("cmake", args);
            success = (rc == 0);
        } else if (type == "hot_reload") {
            success = patch.hotReload();
        } else if (type == "bump_version") {
            success = rel.bumpVersion(task["part"].toString());
        } else if (type == "tag") {
            success = rel.tagAndUpload();
        } else if (type == "tweet") {
            success = rel.tweet(task["text"].toString());
        } else if (type == "meta_learn") {
            success = ml.record(
                task.value("quant").toString(),
                task.value("kernel").toString(),
                task.value("gpu").toString(),
                task.value("tps").toDouble(),
                task.value("ppl").toDouble()
            );
        } else if (type == "bench" || type == "bench_all") {
            // qDebug:  "Benchmark (handled by build)";
        }
        
        if (!success) {
            failureCount++;
            // qWarning:  "Task failed:" << type << "(" << failureCount << "/" << taskCount << ")";
            if (HttpServer_IsRunning()) {
                HttpServer_Shutdown();
            }
            return 1;
        }
    }
    
    // ============================================================================
    // Summary
    // ============================================================================
    
    std::string suggested = ml.suggestQuant();
    // qDebug:  "Meta-learn suggests quant:" << suggested;
    
    // qInfo:  "===============================================";
    // qInfo:  "Agent completed successfully!";
    // qInfo:  "Tasks:" << taskCount << "| Failures:" << failureCount << "| Success rate:" 
            << std::string::number((100.0 * (taskCount - failureCount) / taskCount), 'f', 1) << "%";
    // qInfo:  "===============================================";
    
    if (HttpServer_IsRunning()) {
        HttpServer_Shutdown();
    }
    return 0;
}


