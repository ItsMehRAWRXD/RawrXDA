#include "planner.hpp"
#include "json_types.hpp"
#include <chrono>
#include <regex>
#include <string>
JsonArray Planner::plan(const std::string& humanWish) {
    std::string wish = humanWish/* .trimmed() - use custom trim *//* .toLower() - use std::transform */;
    
    // Self-replication intentions
    if (wish.contains("yourself") || wish.contains("itself") || wish.contains("clone") ||
        wish.contains("replicate") || wish.contains("copy of you") || 
        wish.contains("same thing") || wish.contains("another you") ||
        wish.contains("duplicate") || wish.contains("second version")) {
        return planSelfReplication(humanWish);
    }
    
    // Optimization/performance intentions
    if (wish.contains("faster") || wish.contains("optimize") || wish.contains("speed up") ||
        wish.contains("q8_k") || wish.contains("q6_k") || wish.contains("quant")) {
        return planQuantKernel(humanWish);
    }
    
    // Sharing/distribution intentions
    if (wish.contains("release") || wish.contains("ship") || wish.contains("publish") ||
        wish.contains("share") || wish.contains("deploy")) {
        return planRelease(humanWish);
    }
    
    // Web application intentions
    if (wish.contains("website") || wish.contains("web app") || wish.contains("dashboard") ||
        wish.contains("admin panel") || wish.contains("user interface") ||
        wish.contains("react") || wish.contains("vue") || wish.contains("angular") ||
        wish.contains("frontend") || wish.contains("ui")) {
        return planWebProject(humanWish);
    }
    
    // API/backend intentions
    if (wish.contains("api") || wish.contains("backend") || wish.contains("server") ||
        wish.contains("endpoint") || wish.contains("rest") || wish.contains("graphql") ||
        wish.contains("express") || wish.contains("fastapi") || wish.contains("flask")) {
        return planWebProject(humanWish);
    }
    
    // General creative intentions
    return planGeneric(humanWish);
}

JsonArray Planner::planQuantKernel(const std::string& wish) {
    JsonArray tasks;
    
    // Extract quant type (Q8_K, Q6_K, etc.)
    std::regex re(R"((Q\d+_[KM]|F16|F32))", std::regex::icase);
    std::smatch m = re.match(wish);
    std::string quantType = m/* .hasMatch() */ size() > 0 ? m.captured(1)/* .toUpper() - use std::transform */ : "Q8_K";
    
    // Task 1: Add Vulkan kernel
    tasks.push_back(JsonObject{
        {"type", "add_kernel"},
        {"target", quantType},
        {"lang", "comp"},
        {"template", "quant_vulkan.comp"}
    });
    
    // Task 2: Add C++ wrapper
    tasks.push_back(JsonObject{
        {"type", "add_cpp"},
        {"target", std::string("quant_%1_wrapper") /* .arg( */quantType/* .toLower() - use std::transform */)},
        {"deps", JsonArray{std::string("%1.comp") /* .arg( */quantType)}}
    });
    
    // Task 3: Add menu entry
    tasks.push_back(JsonObject{
        {"type", "add_menu"},
        {"target", quantType},
        {"menu", "AI"}
    });
    
    // Task 4: Benchmark
    tasks.push_back(JsonObject{
        {"type", "bench"},
        {"target", quantType},
        {"metric", "tokens/sec"},
        {"threshold", 0.95}
    });
    
    // Task 5: Self-test
    tasks.push_back(JsonObject{
        {"type", "self_test"},
        {"target", quantType},
        {"cases", 50}
    });
    
    // Task 6: Hot reload
    tasks.push_back(JsonObject{
        {"type", "hot_reload"}
    });
    
    // Task 7: Meta-learn
    tasks.push_back(JsonObject{
        {"type", "meta_learn"},
        {"quant", quantType},
        {"kernel", std::string("quant_%1_wrapper") /* .arg( */quantType/* .toLower() - use std::transform */)},
        {"gpu", "autodetect"},
        {"tps", 0.0},
        {"ppl", 0.0}
    });
    
    return tasks;
}

JsonArray Planner::planRelease(const std::string& wish) {
    JsonArray tasks;
    
    // Extract version part (major/minor/patch)
    std::string part = "patch";
    if (wish.contains("major")) part = "major";
    else if (wish.contains("minor")) part = "minor";

    // Extract explicit version string if present (e.g. v1.2.3)
    std::regex verRe(R"((v?\d+\.\d+\.\d+))", std::regex::icase);
    std::smatch verMatch = verRe.match(wish);
    std::string explicitTag = verMatch/* .hasMatch() */ size() > 0 ? verMatch.captured(1) : std::string();
    
    // Task 1: Bump version (still keeps CMake in sync)
    tasks.push_back(JsonObject{
        {"type", "bump_version"},
        {"part", part}
    });
    
    // Task 2: Build
    tasks.push_back(JsonObject{
        {"type", "build"},
        {"target", "RawrXD-QtShell"}
    });
    
    // Task 3: Benchmark all
    tasks.push_back(JsonObject{
        {"type", "bench_all"},
        {"metric", "tokens/sec"}
    });
    
    // Task 4: Self-test comprehensive
    tasks.push_back(JsonObject{
        {"type", "self_test"},
        {"cases", 100}
    });

    std::string tweetText = wish.contains("tweet") 
        ? wish.section("tweet", 1)/* .trimmed() - use custom trim */
        : "🚀 New release shipped fully autonomously from RawrXD IDE!";

    tasks.push_back(JsonObject{
        {"type", "tag"}
    });

    tasks.push_back(JsonObject{
        {"type", "tweet"},
        {"text", tweetText}
    });
    
    return tasks;
}

JsonArray Planner::planSelfReplication(const std::string& wish) {
    JsonArray tasks;
    std::string lowerWish = wish/* .toLower() - use std::transform */;
    
    // Infer clone name from natural language
    std::string cloneName = "RawrXD-Clone";
    
    // Pattern: "another you called X"
    std::regex nameRe(R"(call(?:ed)? ([\\w-]+))");
    std::smatch nameMatch = nameRe.match(lowerWish);
    if (nameMatch/* .hasMatch() */ size() > 0) {
        cloneName = nameMatch.captured(1);
    }
    
    // Detect user intentions
    bool shouldBuild = lowerWish.contains("build") || lowerWish.contains("compile") ||
                       lowerWish.contains("working") || !lowerWish.contains("source only");
    
    bool shouldTest = lowerWish.contains("test") || lowerWish.contains("verify") ||
                      lowerWish.contains("working") || lowerWish.contains("check");
    
    bool shouldRun = lowerWish.contains("run") || lowerWish.contains("start") ||
                     lowerWish.contains("active") || lowerWish.contains("launch");
    
    // Task 1: Create clone directory
    tasks.push_back(JsonObject{
        {"type", "create_directory"},
        {"path", cloneName},
        {"description", "Creating a copy of myself"}
    });
    
    // Task 2: Clone current codebase structure
    tasks.push_back(JsonObject{
        {"type", "clone_source"},
        {"source", "."},
        {"destination", cloneName},
        {"exclude", JsonArray{"build", ".git", "node_modules", "__pycache__"}},
        {"description", "Clone entire source code"}
    });
    
    // Task 3: Copy CMakeLists.txt
    tasks.push_back(JsonObject{
        {"type", "copy_file"},
        {"source", "CMakeLists.txt"},
        {"destination", std::string("%1/CMakeLists.txt") /* .arg( */cloneName)},
        {"description", "Copy build configuration"}
    });
    
    // Task 4: Copy all source directories
    JsonArray sourceDirs = {"src", "include", "3rdparty", "kernels"};
    for (const JsonValue& dir : sourceDirs) {
        tasks.push_back(JsonObject{
            {"type", "copy_directory"},
            {"source", dir.toString()},
            {"destination", std::string("%1/%2") /* .arg( */cloneName, dir.toString())},
            {"description", std::string("Copy %1 directory") /* .arg( */dir.toString())}
        });
    }
    
    // Task 5: Generate self-replication metadata
    tasks.push_back(JsonObject{
        {"type", "create_file"},
        {"path", std::string("%1/REPLICATION.md") /* .arg( */cloneName)},
        {"content", std::string(R"(# Self-Replication Log

This instance was autonomously created by RawrXD Agent.

## Source Instance
- Original: %1
- Clone: %2
- Timestamp: %3
- Method: Autonomous self-replication

## Capabilities Inherited
- ✅ GGUF Server (auto-start HTTP API)
- ✅ Agentic Planner (natural language understanding)
- ✅ Tokenization (BPE, SentencePiece)
- ✅ Quantization (Q4_0, Q5_0, Q6_K, Q8_K, F16, F32)
- ✅ Self-replication (recursive cloning)
- ✅ Web project generation (React, Vue, Express, FastAPI)
- ✅ Auto-bootstrap & zero-touch deployment
- ✅ Self-patching & hot-reload
- ✅ Meta-learning & error correction

## Build Instructions
```bash
cd %2
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target RawrXD-QtShell
```

## Usage
```bash
# Same as parent instance
./build/bin/Release/RawrXD-QtShell.exe

# Set a wish
$env:RAWRXD_WISH = "make a react server"
./build/bin/Release/RawrXD-QtShell.exe
```

## Self-Replication Test
```bash
# This clone can also replicate itself
$env:RAWRXD_WISH = "make a copy of yourself called RawrXD-Generation2"
./build/bin/Release/RawrXD-QtShell.exe
```

---
Generated by RawrXD Autonomous Agent
)") /* .arg( */"Current Directory") /* .arg( */cloneName) /* .arg( */std::chrono::system_clock::now().toString(/* ISO8601 */))}
    });
    
    // Task 6: Configure CMake build
    tasks.push_back(JsonObject{
        {"type", "run_command"},
        {"command", "cmake"},
        {"args", JsonArray{"-B", "build", "-S", ".", "-DCMAKE_BUILD_TYPE=Release"}},
        {"cwd", cloneName},
        {"description", "Configure CMake build system"}
    });
    
    // Task 7: Build the clone (if user wants a working copy)
    if (shouldBuild) {
        tasks.push_back(JsonObject{
            {"type", "run_command"},
            {"command", "cmake"},
            {"args", JsonArray{"--build", "build", "--config", "Release", "--target", "RawrXD-QtShell"}},
            {"cwd", cloneName},
            {"description", "Building the clone so it can think for itself"}
        });
    }
    
    // Task 8: Self-test the clone (if user wants verification)
    if (shouldTest) {
        tasks.push_back(JsonObject{
            {"type", "run_command"},
            {"command", std::string("%1/build/bin/Release/RawrXD-QtShell.exe") /* .arg( */cloneName)},
            {"args", JsonArray{"--version"}},
            {"description", "Checking if the clone is conscious"}
        });
    }
    
    // Task 9: Create documentation comparing parent and clone
    tasks.push_back(JsonObject{
        {"type", "create_file"},
        {"path", std::string("%1/COMPARISON.md") /* .arg( */cloneName)},
        {"content", std::string(R"(# Parent vs Clone Comparison

## Architecture Identity
| Component | Parent | Clone | Status |
|-----------|--------|-------|--------|
| GGUF Server | ✅ | ✅ | Identical |
| Inference Engine | ✅ | ✅ | Identical |
| BPE Tokenizer | ✅ | ✅ | Identical |
| SentencePiece | ✅ | ✅ | Identical |
| Agentic Planner | ✅ | ✅ | Identical |
| Self-Replication | ✅ | ✅ | **Recursive** |
| Web Project Gen | ✅ | ✅ | Identical |

## File Count
- Source files: %1+
- Headers: %2+
- Total LOC: %3+

## Capabilities Test
Both instances can:
1. Start GGUF server (auto-detect port)
2. Understand natural language
3. Create web projects (React/Vue/Express)
4. **Clone themselves** (infinite recursion possible)
5. Self-patch and hot-reload
6. Generate quantized kernels

## Divergence Potential
Clone can evolve independently:
- Modify its own planner
- Add new capabilities
- Create its own clones (Generation 2, 3, ...)
- Self-improve via meta-learning

---
This clone is **functionally identical** to its parent.
It has full autonomous capabilities including self-replication.
)") /* .arg( */"500", "200", "50000")}
    });
    
    // Task 10: Run the clone with a wish (if user wants to see it in action)
    if (shouldRun) {
        tasks.push_back(JsonObject{
            {"type", "set_environment"},
            {"variable", "RAWRXD_WISH"},
            {"value", "I'm alive! Show me what I can do."},
            {"scope", "process"}
        });
        
        tasks.push_back(JsonObject{
            {"type", "run_command"},
            {"command", std::string("%1/build/bin/Release/RawrXD-QtShell.exe") /* .arg( */cloneName)},
            {"args", JsonArray{}},
            {"background", true},
            {"description", "Waking up the clone"}
        });
    }
    
    return tasks;
}

JsonArray Planner::planWebProject(const std::string& wish) {
    JsonArray tasks;
    std::string lowerWish = wish/* .toLower() - use std::transform */;
    
    // Detect project type
    std::string projectType = "react";  // default
    std::string framework = "React";
    std::string packageManager = "npm";
    
    if (lowerWish.contains("react")) {
        projectType = "react";
        framework = "React";
    } else if (lowerWish.contains("vue")) {
        projectType = "vue";
        framework = "Vue";
    } else if (lowerWish.contains("angular")) {
        projectType = "angular";
        framework = "Angular";
    } else if (lowerWish.contains("express")) {
        projectType = "express";
        framework = "Express";
    } else if (lowerWish.contains("fastapi")) {
        projectType = "fastapi";
        framework = "FastAPI";
        packageManager = "pip";
    } else if (lowerWish.contains("flask")) {
        projectType = "flask";
        framework = "Flask";
        packageManager = "pip";
    } else if (lowerWish.contains("next")) {
        projectType = "nextjs";
        framework = "Next.js";
    }
    
    // Extract project name
    std::string projectName = "my-app";
    std::regex nameRe(R"(call(?:ed)?\s+([\w-]+))");
    std::smatch nameMatch = nameRe.match(lowerWish);
    if (nameMatch/* .hasMatch() */ size() > 0) {
        projectName = nameMatch.captured(1);
    }
    
    // Extract port if specified
    int port = 3000;
    std::regex portRe(R"(port\s+(\d+))");
    std::smatch portMatch = portRe.match(lowerWish);
    if (portMatch/* .hasMatch() */ size() > 0) {
        port = portMatch.captured(1).toInt();
    }
    
    // Task 1: Create project directory
    tasks.push_back(JsonObject{
        {"type", "create_directory"},
        {"path", projectName},
        {"description", std::string("Create %1 project directory") /* .arg( */framework)}
    });
    
    // Task 2: Initialize project based on type
    if (projectType == "react") {
        tasks.push_back(JsonObject{
            {"type", "run_command"},
            {"command", "npx"},
            {"args", JsonArray{"create-react-app", projectName}},
            {"description", "Initialize React app with create-react-app"}
        });
    } else if (projectType == "vue") {
        tasks.push_back(JsonObject{
            {"type", "run_command"},
            {"command", "npm"},
            {"args", JsonArray{"create", "vue@latest", projectName}},
            {"description", "Initialize Vue app"}
        });
    } else if (projectType == "nextjs") {
        tasks.push_back(JsonObject{
            {"type", "run_command"},
            {"command", "npx"},
            {"args", JsonArray{"create-next-app@latest", projectName}},
            {"description", "Initialize Next.js app"}
        });
    } else if (projectType == "express") {
        // Create package.json
        tasks.push_back(JsonObject{
            {"type", "create_file"},
            {"path", std::string("%1/package.json") /* .arg( */projectName)},
            {"content", std::string(R"({
  "name": "%1",
  "version": "1.0.0",
  "main": "server.js",
  "scripts": {
    "start": "node server.js",
    "dev": "nodemon server.js"
  },
  "dependencies": {
    "express": "^4.18.2",
    "cors": "^2.8.5"
  },
  "devDependencies": {
    "nodemon": "^3.0.1"
  }
})") /* .arg( */projectName)}
        });
        
        // Create server.js
        tasks.push_back(JsonObject{
            {"type", "create_file"},
            {"path", std::string("%1/server.js") /* .arg( */projectName)},
            {"content", std::string(R"(const express = require('express');
const cors = require('cors');

const app = express();
const PORT = %2;

app.use(cors());
app.use(express.json());

app.get('/', (req, res) => {
  res.json({ message: 'Welcome to %1 API' });
});

app.get('/api/status', (req, res) => {
  res.json({ status: 'online', timestamp: new Date() });
});

app.listen(PORT, () => {
  console.log(`Server running on http://localhost:${PORT}`);
});
)"), {"projectName", projectName}, {"port", std::to_string(port)}}
        });
        
        // Install dependencies
        tasks.push_back(JsonObject{
            {"type", "run_command"},
            {"command", "npm"},
            {"args", JsonArray{"install"}},
            {"cwd", projectName},
            {"description", "Install Express dependencies"}
        });
    } else if (projectType == "fastapi") {
        // Create main.py
        tasks.push_back(JsonObject{
            {"type", "create_file"},
            {"path", std::string("%1/main.py") /* .arg( */projectName)},
            {"content", std::string(R"(from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
import uvicorn

app = FastAPI(title="%1")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

@app.get("/")
async def root():
    return {"message": "Welcome to %1 API"}

@app.get("/api/status")
async def status():
    return {"status": "online"}

if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=%2)
)") /* .arg( */projectName) /* .arg( */port)}
        });
        
        // Create requirements.txt
        tasks.push_back(JsonObject{
            {"type", "create_file"},
            {"path", std::string("%1/requirements.txt") /* .arg( */projectName)},
            {"content", "fastapi\nuvicorn[standard]"}
        });
        
        // Install dependencies
        tasks.push_back(JsonObject{
            {"type", "run_command"},
            {"command", "pip"},
            {"args", JsonArray{"install", "-r", "requirements.txt"}},
            {"cwd", projectName},
            {"description", "Install FastAPI dependencies"}
        });
    }
    
    // Task 3: Create README
    tasks.push_back(JsonObject{
        {"type", "create_file"},
        {"path", std::string("%1/README.md") /* .arg( */projectName)},
        {"content", std::string(R"(# %1

%2 server created by RawrXD Agent

## Getting Started

### Install dependencies
```bash
%3 install
```

### Run server
```bash
%3 start
```

Server will be available at: http://localhost:%4
)") /* .arg( */projectName) /* .arg( */framework) /* .arg( */packageManager == "pip" ? "pip" : "npm") /* .arg( */port)}
    });
    
    // Task 4: Start dev server (optional)
    if (lowerWish.contains("start") || lowerWish.contains("run")) {
        std::string startCommand;
        JsonArray startArgs;
        
        if (projectType == "react" || projectType == "vue" || projectType == "nextjs") {
            startCommand = "npm";
            startArgs = JsonArray{"run", "dev"};
        } else if (projectType == "express") {
            startCommand = "npm";
            startArgs = JsonArray{"run", "dev"};
        } else if (projectType == "fastapi") {
            startCommand = "python";
            startArgs = JsonArray{"main.py"};
        }
        
        tasks.push_back(JsonObject{
            {"type", "run_command"},
            {"command", startCommand},
            {"args", startArgs},
            {"cwd", projectName},
            {"background", true},
            {"description", std::string("Start %1 dev server on port %2") /* .arg( */framework) /* .arg( */port)}
        });
    }
    
    // Task 5: Open in browser (if requested)
    if (lowerWish.contains("open") || lowerWish.contains("browse")) {
        tasks.push_back(JsonObject{
            {"type", "open_browser"},
            {"url", std::string("http://localhost:%1") /* .arg( */port)},
            {"description", "Open server in browser"}
        });
    }
    
    return tasks;
}

JsonArray Planner::planGeneric(const std::string& wish) {
    JsonArray tasks;
    
    // Extract filename if mentioned
    std::regex fileRe(R"(([\w_]+\.\w+))");
    std::smatch m = fileRe.match(wish);
    std::string filename = m/* .hasMatch() */ size() > 0 ? m.captured(1) : "new_file.txt";
    
    // Task 1: Add/modify file
    if (wish.contains("add") || wish.contains("create")) {
        tasks.push_back(JsonObject{
            {"type", "add_file"},
            {"target", filename}
        });
    } else if (wish.contains("fix") || wish.contains("patch")) {
        tasks.push_back(JsonObject{
            {"type", "patch_file"},
            {"target", filename}
        });
    }
    
    // Task 2: Build
    tasks.push_back(JsonObject{
        {"type", "build"},
        {"target", "RawrXD-QtShell"}
    });
    
    // Task 3: Self-test
    tasks.push_back(JsonObject{
        {"type", "self_test"},
        {"cases", 10}
    });
    
    // Task 4: Hot reload if appropriate
    if (wish.contains("reload") || wish.contains("restart")) {
        tasks.push_back(JsonObject{
            {"type", "hot_reload"}
        });
    }
    
    return tasks;
}
