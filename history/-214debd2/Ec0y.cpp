#include "planner.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>

QJsonArray Planner::plan(const QString& humanWish) {
    QString wish = humanWish.trimmed().toLower();
    
    // Route to specialized planners based on keywords
    if (wish.contains("q8_k") || wish.contains("q6_k") || wish.contains("quant")) {
        return planQuantKernel(humanWish);
    } else if (wish.contains("release") || wish.contains("ship") || wish.contains("publish")) {
        return planRelease(humanWish);
    } else {
        return planGeneric(humanWish);
    }
}

QJsonArray Planner::planQuantKernel(const QString& wish) {
    QJsonArray tasks;
    
    // Extract quant type (Q8_K, Q6_K, etc.)
    QRegularExpression re(R"((Q\d+_[KM]|F16|F32))", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch m = re.match(wish);
    QString quantType = m.hasMatch() ? m.captured(1).toUpper() : "Q8_K";
    
    // Task 1: Add Vulkan kernel
    tasks.append(QJsonObject{
        {"type", "add_kernel"},
        {"target", quantType},
        {"lang", "comp"},
        {"template", "quant_vulkan.comp"}
    });
    
    // Task 2: Add C++ wrapper
    tasks.append(QJsonObject{
        {"type", "add_cpp"},
        {"target", QString("quant_%1_wrapper").arg(quantType.toLower())},
        {"deps", QJsonArray{QString("%1.comp").arg(quantType)}}
    });
    
    // Task 3: Add menu entry
    tasks.append(QJsonObject{
        {"type", "add_menu"},
        {"target", quantType},
        {"menu", "AI"}
    });
    
    // Task 4: Benchmark
    tasks.append(QJsonObject{
        {"type", "bench"},
        {"target", quantType},
        {"metric", "tokens/sec"},
        {"threshold", 0.95}
    });
    
    // Task 5: Self-test
    tasks.append(QJsonObject{
        {"type", "self_test"},
        {"target", quantType},
        {"cases", 50}
    });
    
    // Task 6: Hot reload
    tasks.append(QJsonObject{
        {"type", "hot_reload"}
    });
    
    // Task 7: Meta-learn
    tasks.append(QJsonObject{
        {"type", "meta_learn"},
        {"quant", quantType},
        {"kernel", QString("quant_%1_wrapper").arg(quantType.toLower())},
        {"gpu", "autodetect"},
        {"tps", 0.0},
        {"ppl", 0.0}
    });
    
    return tasks;
}

QJsonArray Planner::planRelease(const QString& wish) {
    QJsonArray tasks;
    
    // Extract version part (major/minor/patch)
    QString part = "patch";
    if (wish.contains("major")) part = "major";
    else if (wish.contains("minor")) part = "minor";

    // Extract explicit version string if present (e.g. v1.2.3)
    QRegularExpression verRe(R"((v?\d+\.\d+\.\d+))", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch verMatch = verRe.match(wish);
    QString explicitTag = verMatch.hasMatch() ? verMatch.captured(1) : QString();
    
    // Task 1: Bump version (still keeps CMake in sync)
    tasks.append(QJsonObject{
        {"type", "bump_version"},
        {"part", part}
    });
    
    // Task 2: Build
    tasks.append(QJsonObject{
        {"type", "build"},
        {"target", "RawrXD-QtShell"}
    });
    
    // Task 3: Benchmark all
    tasks.append(QJsonObject{
        {"type", "bench_all"},
        {"metric", "tokens/sec"}
    });
    
    // Task 4: Self-test comprehensive
    tasks.append(QJsonObject{
        {"type", "self_test"},
        {"cases", 100}
    });
    
    // Path to built QtShell binary
    const QString exePath = "build/bin/Release/RawrXD-QtShell.exe";

    // Task 5: Sign binary
    tasks.append(QJsonObject{
        {"type", "sign_binary"},
        {"path", exePath}
    });

    // Decide release tag: explicit vX.Y.Z overrides bumped version if given
    QString tagForRelease = explicitTag.isEmpty() ? QString() : explicitTag;

    // Task 6: Upload signed binary to CDN
    // Blob name derived from tag if provided, else generic
    QString blobName = tagForRelease.isEmpty()
        ? QStringLiteral("RawrXD-QtShell-latest.exe")
        : QStringLiteral("RawrXD-QtShell-%1.exe").arg(tagForRelease);
    tasks.append(QJsonObject{
        {"type", "cdn_upload"},
        {"local", exePath},
        {"remote", blobName}
    });

    // Changelog text from wish (fallback to generic)
    QString changelog = wish;
    if (changelog.isEmpty()) {
        changelog = QStringLiteral("Autonomous release from RawrXD Agent");
    }

    // Task 7: GitHub release
    tasks.append(QJsonObject{
        {"type", "github_release"},
        {"tag", tagForRelease.isEmpty() ? QStringLiteral("auto") : tagForRelease},
        {"changelog", changelog}
    });

    // Task 8: Update manifest (sha256 placeholder; real pipeline can compute beforehand)
    tasks.append(QJsonObject{
        {"type", "update_manifest"},
        {"tag", tagForRelease.isEmpty() ? QStringLiteral("auto") : tagForRelease},
        {"sha256", QStringLiteral("TODO_COMPUTE_SHA256")}
    });

    // Task 9: Tweet announcement using dedicated release tweet path
    QString tweetText = wish.contains("tweet") 
        ? wish.section("tweet", 1).trimmed()
        : QStringLiteral("🚀 New release shipped fully autonomously from RawrXD IDE!");
    tasks.append(QJsonObject{
        {"type", "tweet_release"},
        {"text", tweetText}
    });
    
    return tasks;
}

QJsonArray Planner::planGeneric(const QString& wish) {
    QJsonArray tasks;
    
    // Extract filename if mentioned
    QRegularExpression fileRe(R"(([\w_]+\.\w+))");
    QRegularExpressionMatch m = fileRe.match(wish);
    QString filename = m.hasMatch() ? m.captured(1) : "new_file.txt";
    
    // Task 1: Add/modify file
    if (wish.contains("add") || wish.contains("create")) {
        tasks.append(QJsonObject{
            {"type", "add_file"},
            {"target", filename}
        });
    } else if (wish.contains("fix") || wish.contains("patch")) {
        tasks.append(QJsonObject{
            {"type", "patch_file"},
            {"target", filename}
        });
    }
    
    // Task 2: Build
    tasks.append(QJsonObject{
        {"type", "build"},
        {"target", "RawrXD-QtShell"}
    });
    
    // Task 3: Self-test
    tasks.append(QJsonObject{
        {"type", "self_test"},
        {"cases", 10}
    });
    
    // Task 4: Hot reload if appropriate
    if (wish.contains("reload") || wish.contains("restart")) {
        tasks.append(QJsonObject{
            {"type", "hot_reload"}
        });
    }
    
    return tasks;
}
