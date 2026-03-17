// ============================================================================
// planner.cpp — Wish-to-Plan Transformer
// ============================================================================
// Architecture: C++20, Win32, no exceptions
// Converts natural-language wishes into executable task arrays.
// All methods return nlohmann::json (matching planner.hpp contract).
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "planner.hpp"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <regex>
#include <string>

using json = nlohmann::json;

// ============================================================================
// String helpers
// ============================================================================
namespace {

std::string toLower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(), ::tolower);
    return r;
}

std::string toUpper(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(), ::toupper);
    return r;
}

std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return {};
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

bool ci_contains(const std::string& haystack, const std::string& needle) {
    std::string h = toLower(haystack);
    std::string n = toLower(needle);
    return h.find(n) != std::string::npos;
}

std::string textAfter(const std::string& s, const std::string& keyword) {
    auto pos = toLower(s).find(toLower(keyword));
    if (pos == std::string::npos) return {};
    return trim(s.substr(pos + keyword.size()));
}

std::string isoTimestamp() {
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

bool rxSearch(const std::string& text, std::smatch& m, const std::regex& re) {
    return std::regex_search(text, m, re);
}

} // anonymous namespace

// ============================================================================
// plan — Top-level intent router
// ============================================================================
json Planner::plan(const std::string& humanWish) {
    std::string wish = trim(toLower(humanWish));

    if (ci_contains(wish, "yourself") || ci_contains(wish, "itself") || ci_contains(wish, "clone") ||
        ci_contains(wish, "replicate") || ci_contains(wish, "copy of you") ||
        ci_contains(wish, "same thing") || ci_contains(wish, "another you") ||
        ci_contains(wish, "duplicate") || ci_contains(wish, "second version")) {
        return planSelfReplication(humanWish);
    }

    if (ci_contains(wish, "faster") || ci_contains(wish, "optimize") || ci_contains(wish, "speed up") ||
        ci_contains(wish, "q8_k") || ci_contains(wish, "q6_k") || ci_contains(wish, "quant")) {
        return planQuantKernel(humanWish);
    }

    if (ci_contains(wish, "release") || ci_contains(wish, "ship") || ci_contains(wish, "publish") ||
        ci_contains(wish, "share") || ci_contains(wish, "deploy")) {
        return planRelease(humanWish);
    }

    if (ci_contains(wish, "bulk fix") || ci_contains(wish, "bulk refactor") ||
        ci_contains(wish, "fix all") || ci_contains(wish, "mass fix") ||
        ci_contains(wish, "fix every") || ci_contains(wish, "fix each") ||
        ci_contains(wish, "batch fix") || ci_contains(wish, "bulk patch") ||
        ci_contains(wish, "fix across") || ci_contains(wish, "repetitive fix") ||
        ci_contains(wish, "autonomous fix") || ci_contains(wish, "auto fix") ||
        ci_contains(wish, "subagent fix") || ci_contains(wish, "parallel fix") ||
        ci_contains(wish, "implement all stubs") || ci_contains(wish, "fix all stubs") ||
        ci_contains(wish, "fix compile errors") || ci_contains(wish, "fix all errors") ||
        ci_contains(wish, "fix lint") || ci_contains(wish, "fix warnings") ||
        ci_contains(wish, "add docs to all") || ci_contains(wish, "document all") ||
        ci_contains(wish, "generate tests for all") || ci_contains(wish, "test all functions")) {
        return planBulkFix(humanWish);
    }

    if (ci_contains(wish, "website") || ci_contains(wish, "web app") || ci_contains(wish, "dashboard") ||
        ci_contains(wish, "admin panel") || ci_contains(wish, "user interface") ||
        ci_contains(wish, "react") || ci_contains(wish, "vue") || ci_contains(wish, "angular") ||
        ci_contains(wish, "frontend") || ci_contains(wish, "ui")) {
        return planWebProject(humanWish);
    }

    if (ci_contains(wish, "api") || ci_contains(wish, "backend") || ci_contains(wish, "server") ||
        ci_contains(wish, "endpoint") || ci_contains(wish, "rest") || ci_contains(wish, "graphql") ||
        ci_contains(wish, "express") || ci_contains(wish, "fastapi") || ci_contains(wish, "flask")) {
        return planWebProject(humanWish);
    }

    if (ci_contains(wish, "bulk fix") || ci_contains(wish, "fix all") || ci_contains(wish, "batch fix") ||
        ci_contains(wish, "fix everything") || ci_contains(wish, "purge") || ci_contains(wish, "mass fix") ||
        ci_contains(wish, "sweep") || ci_contains(wish, "clean all")) {
        return planBulkFix(humanWish);
    }

    return planGeneric(humanWish);
}

// ============================================================================
// planQuantKernel — Quantization kernel pipeline
// ============================================================================
json Planner::planQuantKernel(const std::string& wish) {
    json tasks = json::array();

    std::regex re(R"((Q\d+_[KM]|F16|F32))", std::regex::icase);
    std::smatch m;
    std::string quantType = rxSearch(wish, m, re) ? toUpper(m[1].str()) : "Q8_K";
    std::string quantLower = toLower(quantType);

    tasks.push_back({
        {"type", "add_kernel"},
        {"target", quantType},
        {"lang", "comp"},
        {"template", "quant_vulkan.comp"}
    });

    tasks.push_back({
        {"type", "add_cpp"},
        {"target", "quant_" + quantLower + "_wrapper"},
        {"deps", json::array({quantType + ".comp"})}
    });

    tasks.push_back({
        {"type", "add_menu"},
        {"target", quantType},
        {"menu", "AI"}
    });

    tasks.push_back({
        {"type", "bench"},
        {"target", quantType},
        {"metric", "tokens/sec"},
        {"threshold", 0.95}
    });

    tasks.push_back({
        {"type", "self_test"},
        {"target", quantType},
        {"cases", 50}
    });

    tasks.push_back({{"type", "hot_reload"}});

    tasks.push_back({
        {"type", "meta_learn"},
        {"quant", quantType},
        {"kernel", "quant_" + quantLower + "_wrapper"},
        {"gpu", "autodetect"},
        {"tps", 0.0},
        {"ppl", 0.0}
    });

    return tasks;
}

// ============================================================================
// planRelease — Release/ship pipeline
// ============================================================================
json Planner::planRelease(const std::string& wish) {
    json tasks = json::array();

    std::string part = "patch";
    if (ci_contains(wish, "major")) part = "major";
    else if (ci_contains(wish, "minor")) part = "minor";

    std::regex verRe(R"((v?\d+\.\d+\.\d+))", std::regex::icase);
    std::smatch verMatch;
    std::string explicitTag = rxSearch(wish, verMatch, verRe) ? verMatch[1].str() : std::string();
    (void)explicitTag;

    tasks.push_back({{"type", "bump_version"}, {"part", part}});
    tasks.push_back({{"type", "build"}, {"target", "RawrXD-Shell"}});
    tasks.push_back({{"type", "bench_all"}, {"metric", "tokens/sec"}});
    tasks.push_back({{"type", "self_test"}, {"cases", 100}});

    std::string tweetText = ci_contains(wish, "tweet")
        ? trim(textAfter(wish, "tweet"))
        : "\xF0\x9F\x9A\x80 New release shipped fully autonomously from RawrXD IDE!";

    tasks.push_back({{"type", "tag"}});
    tasks.push_back({{"type", "tweet"}, {"text", tweetText}});

    return tasks;
}

// ============================================================================
// planSelfReplication — Clone the IDE autonomously
// ============================================================================
json Planner::planSelfReplication(const std::string& wish) {
    json tasks = json::array();
    std::string lowerWish = toLower(wish);

    std::string cloneName = "RawrXD-Clone";
    std::regex nameRe(R"(call(?:ed)?\s+([\w-]+))");
    std::smatch nameMatch;
    if (rxSearch(lowerWish, nameMatch, nameRe)) {
        cloneName = nameMatch[1].str();
    }

    bool shouldBuild = ci_contains(lowerWish, "build") || ci_contains(lowerWish, "compile") ||
                       ci_contains(lowerWish, "working") || !ci_contains(lowerWish, "source only");
    bool shouldTest = ci_contains(lowerWish, "test") || ci_contains(lowerWish, "verify") ||
                      ci_contains(lowerWish, "working") || ci_contains(lowerWish, "check");
    bool shouldRun = ci_contains(lowerWish, "run") || ci_contains(lowerWish, "start") ||
                     ci_contains(lowerWish, "active") || ci_contains(lowerWish, "launch");

    tasks.push_back({
        {"type", "create_directory"},
        {"path", cloneName},
        {"description", "Creating a copy of myself"}
    });

    tasks.push_back({
        {"type", "clone_source"},
        {"source", "."},
        {"destination", cloneName},
        {"exclude", json::array({"build", ".git", "node_modules", "__pycache__"})},
        {"description", "Clone entire source code"}
    });

    tasks.push_back({
        {"type", "copy_file"},
        {"source", "CMakeLists.txt"},
        {"destination", cloneName + "/CMakeLists.txt"},
        {"description", "Copy build configuration"}
    });

    std::vector<std::string> sourceDirs = {"src", "include", "3rdparty", "kernels"};
    for (const auto& dir : sourceDirs) {
        tasks.push_back({
            {"type", "copy_directory"},
            {"source", dir},
            {"destination", cloneName + "/" + dir},
            {"description", "Copy " + dir + " directory"}
        });
    }

    std::string timestamp = isoTimestamp();
    std::string replicationMd =
        "# Self-Replication Log\n\n"
        "This instance was autonomously created by RawrXD Agent.\n\n"
        "## Source Instance\n"
        "- Original: Current Directory\n"
        "- Clone: " + cloneName + "\n"
        "- Timestamp: " + timestamp + "\n"
        "- Method: Autonomous self-replication\n\n"
        "## Capabilities Inherited\n"
        "- GGUF Server (auto-start HTTP API)\n"
        "- Agentic Planner (natural language understanding)\n"
        "- Tokenization (BPE, SentencePiece)\n"
        "- Quantization (Q4_0, Q5_0, Q6_K, Q8_K, F16, F32)\n"
        "- Self-replication (recursive cloning)\n"
        "- Web project generation (React, Vue, Express, FastAPI)\n"
        "- Auto-bootstrap & zero-touch deployment\n"
        "- Self-patching & hot-reload\n"
        "- Meta-learning & error correction\n\n"
        "## Build Instructions\n"
        "```bash\n"
        "cd " + cloneName + "\n"
        "cmake -B build -S . -DCMAKE_BUILD_TYPE=Release\n"
        "cmake --build build --config Release --target RawrXD-Shell\n"
        "```\n\n"
        "## Usage\n"
        "```bash\n"
        "./build/bin/Release/RawrXD-Shell.exe\n\n"
        "$env:RAWRXD_WISH = \"make a react server\"\n"
        "./build/bin/Release/RawrXD-Shell.exe\n"
        "```\n\n"
        "## Self-Replication Test\n"
        "```bash\n"
        "$env:RAWRXD_WISH = \"make a copy of yourself called RawrXD-Generation2\"\n"
        "./build/bin/Release/RawrXD-Shell.exe\n"
        "```\n\n"
        "---\nGenerated by RawrXD Autonomous Agent\n";

    tasks.push_back({
        {"type", "create_file"},
        {"path", cloneName + "/REPLICATION.md"},
        {"content", replicationMd}
    });

    tasks.push_back({
        {"type", "run_command"},
        {"command", "cmake"},
        {"args", json::array({"-B", "build", "-S", ".", "-DCMAKE_BUILD_TYPE=Release"})},
        {"cwd", cloneName},
        {"description", "Configure CMake build system"}
    });

    if (shouldBuild) {
        tasks.push_back({
            {"type", "run_command"},
            {"command", "cmake"},
            {"args", json::array({"--build", "build", "--config", "Release", "--target", "RawrXD-Shell"})},
            {"cwd", cloneName},
            {"description", "Building the clone so it can think for itself"}
        });
    }

    if (shouldTest) {
        tasks.push_back({
            {"type", "run_command"},
            {"command", cloneName + "/build/bin/Release/RawrXD-Shell.exe"},
            {"args", json::array({"--version"})},
            {"description", "Checking if the clone is conscious"}
        });
    }

    std::string comparisonMd =
        "# Parent vs Clone Comparison\n\n"
        "## Architecture Identity\n"
        "| Component | Parent | Clone | Status |\n"
        "|-----------|--------|-------|--------|\n"
        "| GGUF Server | Yes | Yes | Identical |\n"
        "| Inference Engine | Yes | Yes | Identical |\n"
        "| BPE Tokenizer | Yes | Yes | Identical |\n"
        "| SentencePiece | Yes | Yes | Identical |\n"
        "| Agentic Planner | Yes | Yes | Identical |\n"
        "| Self-Replication | Yes | Yes | **Recursive** |\n"
        "| Web Project Gen | Yes | Yes | Identical |\n\n"
        "## File Count\n"
        "- Source files: 500+\n"
        "- Headers: 200+\n"
        "- Total LOC: 50000+\n\n"
        "## Capabilities Test\n"
        "Both instances can:\n"
        "1. Start GGUF server (auto-detect port)\n"
        "2. Understand natural language\n"
        "3. Create web projects (React/Vue/Express)\n"
        "4. **Clone themselves** (infinite recursion possible)\n"
        "5. Self-patch and hot-reload\n"
        "6. Generate quantized kernels\n\n"
        "## Divergence Potential\n"
        "Clone can evolve independently:\n"
        "- Modify its own planner\n"
        "- Add new capabilities\n"
        "- Create its own clones (Generation 2, 3, ...)\n"
        "- Self-improve via meta-learning\n\n"
        "---\n"
        "This clone is **functionally identical** to its parent.\n"
        "It has full autonomous capabilities including self-replication.\n";

    tasks.push_back({
        {"type", "create_file"},
        {"path", cloneName + "/COMPARISON.md"},
        {"content", comparisonMd}
    });

    if (shouldRun) {
        tasks.push_back({
            {"type", "set_environment"},
            {"variable", "RAWRXD_WISH"},
            {"value", "I'm alive! Show me what I can do."},
            {"scope", "process"}
        });

        tasks.push_back({
            {"type", "run_command"},
            {"command", cloneName + "/build/bin/Release/RawrXD-Shell.exe"},
            {"args", json::array()},
            {"background", true},
            {"description", "Waking up the clone"}
        });
    }

    return tasks;
}

// ============================================================================
// planWebProject — Web application scaffolding
// ============================================================================
json Planner::planWebProject(const std::string& wish) {
    json tasks = json::array();
    std::string lowerWish = toLower(wish);

    std::string projectType = "react";
    std::string framework = "React";
    std::string packageManager = "npm";

    if (ci_contains(lowerWish, "react")) {
        projectType = "react"; framework = "React";
    } else if (ci_contains(lowerWish, "vue")) {
        projectType = "vue"; framework = "Vue";
    } else if (ci_contains(lowerWish, "angular")) {
        projectType = "angular"; framework = "Angular";
    } else if (ci_contains(lowerWish, "express")) {
        projectType = "express"; framework = "Express";
    } else if (ci_contains(lowerWish, "fastapi")) {
        projectType = "fastapi"; framework = "FastAPI"; packageManager = "pip";
    } else if (ci_contains(lowerWish, "flask")) {
        projectType = "flask"; framework = "Flask"; packageManager = "pip";
    } else if (ci_contains(lowerWish, "next")) {
        projectType = "nextjs"; framework = "Next.js";
    }

    std::string projectName = "my-app";
    std::regex nameRe(R"(call(?:ed)?\s+([\w-]+))");
    std::smatch nameMatch;
    if (rxSearch(lowerWish, nameMatch, nameRe)) {
        projectName = nameMatch[1].str();
    }

    int port = 3000;
    std::regex portRe(R"(port\s+(\d+))");
    std::smatch portMatch;
    if (rxSearch(lowerWish, portMatch, portRe)) {
        port = std::stoi(portMatch[1].str());
    }
    std::string portStr = std::to_string(port);

    tasks.push_back({
        {"type", "create_directory"},
        {"path", projectName},
        {"description", "Create " + framework + " project directory"}
    });

    if (projectType == "react") {
        tasks.push_back({
            {"type", "run_command"}, {"command", "npx"},
            {"args", json::array({"create-react-app", projectName})},
            {"description", "Initialize React app with create-react-app"}
        });
    } else if (projectType == "vue") {
        tasks.push_back({
            {"type", "run_command"}, {"command", "npm"},
            {"args", json::array({"create", "vue@latest", projectName})},
            {"description", "Initialize Vue app"}
        });
    } else if (projectType == "nextjs") {
        tasks.push_back({
            {"type", "run_command"}, {"command", "npx"},
            {"args", json::array({"create-next-app@latest", projectName})},
            {"description", "Initialize Next.js app"}
        });
    } else if (projectType == "express") {
        std::string packageJson =
            "{\n  \"name\": \"" + projectName + "\",\n"
            "  \"version\": \"1.0.0\",\n  \"main\": \"server.js\",\n"
            "  \"scripts\": {\n    \"start\": \"node server.js\",\n    \"dev\": \"nodemon server.js\"\n  },\n"
            "  \"dependencies\": {\n    \"express\": \"^4.18.2\",\n    \"cors\": \"^2.8.5\"\n  },\n"
            "  \"devDependencies\": {\n    \"nodemon\": \"^3.0.1\"\n  }\n}";

        tasks.push_back({{"type", "create_file"}, {"path", projectName + "/package.json"}, {"content", packageJson}});

        std::string serverJs =
            "const express = require('express');\nconst cors = require('cors');\n\n"
            "const app = express();\nconst PORT = " + portStr + ";\n\n"
            "app.use(cors());\napp.use(express.json());\n\n"
            "app.get('/', (req, res) => {\n  res.json({ message: 'Welcome to " + projectName + " API' });\n});\n\n"
            "app.get('/api/status', (req, res) => {\n  res.json({ status: 'online', timestamp: new Date() });\n});\n\n"
            "app.listen(PORT, () => {\n  console.log(`Server running on http://localhost:${PORT}`);\n});\n";

        tasks.push_back({{"type", "create_file"}, {"path", projectName + "/server.js"}, {"content", serverJs}});
        tasks.push_back({
            {"type", "run_command"}, {"command", "npm"},
            {"args", json::array({"install"})}, {"cwd", projectName},
            {"description", "Install Express dependencies"}
        });
    } else if (projectType == "fastapi") {
        std::string mainPy =
            "from fastapi import FastAPI\nfrom fastapi.middleware.cors import CORSMiddleware\nimport uvicorn\n\n"
            "app = FastAPI(title=\"" + projectName + "\")\n\n"
            "app.add_middleware(\n    CORSMiddleware,\n    allow_origins=[\"*\"],\n"
            "    allow_credentials=True,\n    allow_methods=[\"*\"],\n    allow_headers=[\"*\"],\n)\n\n"
            "@app.get(\"/\")\nasync def root():\n    return {\"message\": \"Welcome to " + projectName + " API\"}\n\n"
            "@app.get(\"/api/status\")\nasync def status():\n    return {\"status\": \"online\"}\n\n"
            "if __name__ == \"__main__\":\n    uvicorn.run(app, host=\"0.0.0.0\", port=" + portStr + ")\n";

        tasks.push_back({{"type", "create_file"}, {"path", projectName + "/main.py"}, {"content", mainPy}});
        tasks.push_back({{"type", "create_file"}, {"path", projectName + "/requirements.txt"}, {"content", "fastapi\nuvicorn[standard]"}});
        tasks.push_back({
            {"type", "run_command"}, {"command", "pip"},
            {"args", json::array({"install", "-r", "requirements.txt"})}, {"cwd", projectName},
            {"description", "Install FastAPI dependencies"}
        });
    }

    std::string pkgCmd = (packageManager == "pip") ? "pip" : "npm";
    std::string readmeMd =
        "# " + projectName + "\n\n" + framework + " server created by RawrXD Agent\n\n"
        "## Getting Started\n\n### Install dependencies\n```bash\n" + pkgCmd + " install\n```\n\n"
        "### Run server\n```bash\n" + pkgCmd + " start\n```\n\n"
        "Server will be available at: http://localhost:" + portStr + "\n";

    tasks.push_back({{"type", "create_file"}, {"path", projectName + "/README.md"}, {"content", readmeMd}});

    if (ci_contains(lowerWish, "start") || ci_contains(lowerWish, "run")) {
        std::string startCommand;
        json startArgs = json::array();

        if (projectType == "react" || projectType == "vue" || projectType == "nextjs") {
            startCommand = "npm"; startArgs = json::array({"run", "dev"});
        } else if (projectType == "express") {
            startCommand = "npm"; startArgs = json::array({"run", "dev"});
        } else if (projectType == "fastapi") {
            startCommand = "python"; startArgs = json::array({"main.py"});
        }

        tasks.push_back({
            {"type", "run_command"}, {"command", startCommand}, {"args", startArgs},
            {"cwd", projectName}, {"background", true},
            {"description", "Start " + framework + " dev server on port " + portStr}
        });
    }

    if (ci_contains(lowerWish, "open") || ci_contains(lowerWish, "browse")) {
        tasks.push_back({
            {"type", "open_browser"},
            {"url", "http://localhost:" + portStr},
            {"description", "Open server in browser"}
        });
    }

    return tasks;
}

// ============================================================================
// planBulkFix — Autonomous bulk fix via subagents
// ============================================================================
json Planner::planBulkFix(const std::string& wish) {
    json tasks = json::array();
    std::string lowerWish = toLower(wish);

    // Detect strategy type from wish
    std::string strategy = "compile_error_fix";
    if (ci_contains(lowerWish, "stub") || ci_contains(lowerWish, "implement")) {
        strategy = "stub_implementation";
    } else if (ci_contains(lowerWish, "format") || ci_contains(lowerWish, "style")) {
        strategy = "format_enforcement";
    } else if (ci_contains(lowerWish, "rename") || ci_contains(lowerWish, "refactor")) {
        strategy = "refactor_rename";
    } else if (ci_contains(lowerWish, "lint") || ci_contains(lowerWish, "warning")) {
        strategy = "lint_fix";
    } else if (ci_contains(lowerWish, "header") || ci_contains(lowerWish, "include")) {
        strategy = "header_include_fix";
    } else if (ci_contains(lowerWish, "test") || ci_contains(lowerWish, "unit test")) {
        strategy = "test_generation";
    } else if (ci_contains(lowerWish, "doc") || ci_contains(lowerWish, "comment")) {
        strategy = "doc_comments";
    } else if (ci_contains(lowerWish, "security") || ci_contains(lowerWish, "vuln")) {
        strategy = "security_audit_fix";
    } else if (ci_contains(lowerWish, "compile") || ci_contains(lowerWish, "error") ||
               ci_contains(lowerWish, "build")) {
        strategy = "compile_error_fix";
    }

    // Extract file patterns or paths from wish
    std::regex pathRe(R"(([\w_/\\]+\.(?:cpp|hpp|h|c|asm)))", std::regex::icase);
    std::smatch m;
    std::vector<std::string> explicitFiles;
    std::string remaining = wish;
    while (rxSearch(remaining, m, pathRe)) {
        explicitFiles.push_back(m[1].str());
        remaining = m.suffix().str();
    }

    // Step 1: Scan for targets (if no explicit files given)
    if (explicitFiles.empty()) {
        tasks.push_back({
            {"type", "scan_targets"},
            {"strategy", strategy},
            {"scan_pattern", "src/**/*.cpp"},
            {"description", "Scanning codebase for " + strategy + " targets"}
        });
    }

    // Step 2: Dispatch autonomous subagents for bulk fix
    json bulkFixParams = {
        {"strategy", strategy},
        {"max_parallel", 4},
        {"max_retries", 3},
        {"auto_verify", true},
        {"self_heal", true}
    };

    if (!explicitFiles.empty()) {
        json fileArray = json::array();
        for (const auto& f : explicitFiles) fileArray.push_back(f);
        bulkFixParams["targets"] = fileArray;
    }

    tasks.push_back({
        {"type", "bulk_fix"},
        {"strategy", strategy},
        {"params", bulkFixParams},
        {"description", "Spawning autonomous subagents for " + strategy}
    });

    // Step 3: Verification pass
    tasks.push_back({
        {"type", "verify_bulk_fix"},
        {"strategy", strategy},
        {"description", "Verifying all fixes were applied correctly"}
    });

    // Step 4: Build to confirm no regressions
    tasks.push_back({
        {"type", "build"},
        {"target", "RawrXD-Shell"},
        {"description", "Rebuild to verify no regressions from bulk fixes"}
    });

    // Step 5: Self-test
    tasks.push_back({
        {"type", "self_test"},
        {"cases", 50},
        {"description", "Running self-tests after bulk fix"}
    });

    return tasks;
}

// ============================================================================
// planGeneric — Fallback plan for unrecognized wishes
// ============================================================================
json Planner::planGeneric(const std::string& wish) {
    json tasks = json::array();

    std::regex fileRe(R"(([\w_]+\.\w+))");
    std::smatch m;
    std::string filename = rxSearch(wish, m, fileRe) ? m[1].str() : "new_file.txt";

    if (ci_contains(wish, "add") || ci_contains(wish, "create")) {
        tasks.push_back({{"type", "add_file"}, {"target", filename}});
    } else if (ci_contains(wish, "fix") || ci_contains(wish, "patch")) {
        tasks.push_back({{"type", "patch_file"}, {"target", filename}});
    }

    tasks.push_back({{"type", "build"}, {"target", "RawrXD-Shell"}});
    tasks.push_back({{"type", "self_test"}, {"cases", 10}});

    if (ci_contains(wish, "reload") || ci_contains(wish, "restart")) {
        tasks.push_back({{"type", "hot_reload"}});
    }

    return tasks;
}
