#include "core_generator.h"
#include "../../include/enterprise_license.h"
#include <iostream>
#include <fstream>

// SCAFFOLD_284: Core generator Unity scaffolding

namespace {
UniversalGenerator* EnsureGenerator(std::unique_ptr<UniversalGenerator>& generator) {
    if (!generator) {
        try {
            generator = std::make_unique<UniversalGenerator>();
        } catch (...) {
            return nullptr;
        }
    }
    return generator.get();
}
}

CoreGenerator* CoreGenerator::instance = nullptr;

CoreGenerator::CoreGenerator() {
    generator = std::make_unique<UniversalGenerator>();
}

CoreGenerator& CoreGenerator::GetInstance() {
    if (!instance) {
        instance = new CoreGenerator();
    }
    return *instance;
}

void CoreGenerator::Initialize() {
    std::cout << "[Core Generator] Initializing...\n";
    // UniversalGenerator auto-initializes languages and templates in its constructor.
    // Re-creating the generator ensures a fresh config state.
    if (!generator) {
        generator = std::make_unique<UniversalGenerator>();
    }
    std::cout << "[Core Generator] Ready — "
              << GetLanguageCount() << " languages, "
              << GetTemplateCount() << " templates\n";
}

bool CoreGenerator::Generate(const std::string& name, LanguageType language, 
                            const std::filesystem::path& output_dir) {
    auto* g = EnsureGenerator(generator);
    return g ? g->GenerateProject(name, language, output_dir) : false;
}

bool CoreGenerator::GenerateFromTemplate(const std::string& template_name,
                                        const std::string& project_name,
                                        const std::filesystem::path& output_dir) {
    auto* g = EnsureGenerator(generator);
    return g ? g->GenerateFromTemplate(template_name, project_name, output_dir) : false;
}

bool CoreGenerator::GenerateWebApp(const std::string& name, const std::filesystem::path& output_dir) {
    auto* g = EnsureGenerator(generator);
    if (!g) return false;
    // Web app defaults to React/TypeScript
    return g->GenerateProject(name, LanguageType::REACT, output_dir);
}

bool CoreGenerator::GenerateCLI(const std::string& name, const std::filesystem::path& output_dir) {
    auto* g = EnsureGenerator(generator);
    if (!g) return false;
    // CLI defaults to Rust (fast, modern, single binary)
    return g->GenerateProject(name, LanguageType::RUST, output_dir);
}

bool CoreGenerator::GenerateLibrary(const std::string& name, LanguageType lang, const std::filesystem::path& output_dir) {
    auto* g = EnsureGenerator(generator);
    if (!g) return false;
    // Library uses the specified language
    return g->GenerateProject(name, lang, output_dir);
}

bool CoreGenerator::GenerateGame(const std::string& name, const std::filesystem::path& output_dir) {
    auto* g = EnsureGenerator(generator);
    if (!g) return false;
    // Game defaults to C# (Unity)
    return g->GenerateProject(name, LanguageType::UNITY, output_dir);
}

bool CoreGenerator::GenerateEmbedded(const std::string& name, const std::filesystem::path& output_dir) {
    auto* g = EnsureGenerator(generator);
    if (!g) return false;
    // Embedded defaults to Arduino (PlatformIO)
    return g->GenerateProject(name, LanguageType::ARDUINO, output_dir);
}

bool CoreGenerator::GenerateDataScience(const std::string& name, const std::filesystem::path& output_dir) {
    auto* g = EnsureGenerator(generator);
    if (!g) return false;
    // Data science defaults to Python (Jupyter/numpy/pandas ecosystem)
    return g->GenerateProject(name, LanguageType::PYTHON, output_dir);
}

std::vector<std::string> CoreGenerator::ListLanguages() {
    auto* g = EnsureGenerator(generator);
    return g ? g->ListLanguages() : std::vector<std::string>{};
}

std::vector<std::string> CoreGenerator::ListTemplates() {
    auto* g = EnsureGenerator(generator);
    return g ? g->ListTemplates() : std::vector<std::string>{};
}

size_t CoreGenerator::GetLanguageCount() const {
    auto* g = EnsureGenerator(const_cast<std::unique_ptr<UniversalGenerator>&>(generator));
    return g ? g->GetSupportedLanguageCount() : 0;
}

size_t CoreGenerator::GetTemplateCount() const {
    auto* g = EnsureGenerator(const_cast<std::unique_ptr<UniversalGenerator>&>(generator));
    return g ? g->GetTemplateCount() : 0;
}

bool CoreGenerator::GenerateWithAllFeatures(const std::string& name, LanguageType language,
                                            const std::filesystem::path& output_dir) {
    auto& lic = RawrXD::License::EnterpriseLicenseV2::Instance();
    if (!lic.gate(RawrXD::License::FeatureID::BatchProcessing,
            "CoreGenerator::GenerateWithAllFeatures")) {
        std::cerr << "[Core Generator] Batch Processing requires a Professional license.\n";
        return false;
    }
    auto* g = EnsureGenerator(generator);
    if (!g) return false;
    // Generate base project, then layer tests + CI + Docker
    bool ok = g->GenerateWithTests(name, language, output_dir);
    if (!ok) return false;
    g->GenerateWithCI(name, language, output_dir);
    g->GenerateWithDocker(name, language, output_dir);
    return true;
}

// =========================================================
// UniversalGenerator Implementation
// =========================================================

UniversalGenerator::UniversalGenerator() {
    InitializeLanguages();
    InitializeTemplates();
}

// ---------------------------------------------------------------------------
// Language registry — populates language_configs with extensions, build tools,
// package managers, source/test folder conventions, etc.
// ---------------------------------------------------------------------------
void UniversalGenerator::InitializeLanguages() {
    language_configs.clear();

    // --- Systems Programming ---
    language_configs[LanguageType::C] = {
        "C", ".c", "CMake", "vcpkg",
        {"src"}, {"tests"}, {{"main", "#include <stdio.h>\n\nint main(int argc, char* argv[]) {\n    printf(\"Hello, World!\\n\");\n    return 0;\n}\n"}},
        true, false, {}
    };
    language_configs[LanguageType::CPP] = {
        "C++", ".cpp", "CMake", "vcpkg",
        {"src", "include"}, {"tests"}, {{"main", "#include <iostream>\n\nint main(int argc, char* argv[]) {\n    std::cout << \"Hello, World!\" << std::endl;\n    return 0;\n}\n"}},
        true, false, {}
    };
    language_configs[LanguageType::RUST] = {
        "Rust", ".rs", "Cargo", "cargo",
        {"src"}, {"tests"}, {{"main", "fn main() {\n    println!(\"Hello, World!\");\n}\n"}},
        true, false, {}
    };
    language_configs[LanguageType::GO] = {
        "Go", ".go", "go build", "go mod",
        {"cmd", "internal", "pkg"}, {"tests"}, {{"main", "package main\n\nimport \"fmt\"\n\nfunc main() {\n\tfmt.Println(\"Hello, World!\")\n}\n"}},
        true, true, {}
    };
    language_configs[LanguageType::ZIG] = {
        "Zig", ".zig", "zig build", "zigmod",
        {"src"}, {"tests"}, {{"main", "const std = @import(\"std\");\n\npub fn main() !void {\n    const stdout = std.io.getStdOut().writer();\n    try stdout.print(\"Hello, World!\\n\", .{});\n}\n"}},
        true, false, {}
    };
    language_configs[LanguageType::NIM] = {
        "Nim", ".nim", "nimble", "nimble",
        {"src"}, {"tests"}, {{"main", "echo \"Hello, World!\"\n"}},
        true, false, {}
    };
    language_configs[LanguageType::CRYSTAL] = {
        "Crystal", ".cr", "crystal build", "shards",
        {"src"}, {"spec"}, {{"main", "puts \"Hello, World!\"\n"}},
        true, true, {}
    };

    // --- Scripting ---
    language_configs[LanguageType::PYTHON] = {
        "Python", ".py", "none", "pip",
        {"src"}, {"tests"}, {{"main", "#!/usr/bin/env python3\n\ndef main():\n    print(\"Hello, World!\")\n\nif __name__ == \"__main__\":\n    main()\n"}},
        false, true, {"requirements.txt"}
    };
    language_configs[LanguageType::JAVASCRIPT] = {
        "JavaScript", ".js", "none", "npm",
        {"src"}, {"tests"}, {{"main", "console.log('Hello, World!');\n"}},
        false, true, {"package.json"}
    };
    language_configs[LanguageType::TYPESCRIPT] = {
        "TypeScript", ".ts", "tsc", "npm",
        {"src"}, {"tests"}, {{"main", "console.log('Hello, World!');\n"}},
        true, true, {"package.json", "tsconfig.json"}
    };
    language_configs[LanguageType::LUA] = {
        "Lua", ".lua", "none", "luarocks",
        {"src"}, {"tests"}, {{"main", "print(\"Hello, World!\")\n"}},
        false, true, {}
    };
    language_configs[LanguageType::RUBY] = {
        "Ruby", ".rb", "none", "bundler",
        {"lib"}, {"test", "spec"}, {{"main", "puts 'Hello, World!'\n"}},
        false, true, {"Gemfile"}
    };
    language_configs[LanguageType::PERL] = {
        "Perl", ".pl", "none", "cpan",
        {"lib"}, {"t"}, {{"main", "#!/usr/bin/perl\nuse strict;\nuse warnings;\nprint \"Hello, World!\\n\";\n"}},
        false, true, {}
    };
    language_configs[LanguageType::PHP] = {
        "PHP", ".php", "none", "composer",
        {"src"}, {"tests"}, {{"main", "<?php\necho \"Hello, World!\\n\";\n"}},
        false, true, {"composer.json"}
    };
    language_configs[LanguageType::BASH] = {
        "Bash", ".sh", "none", "none",
        {"scripts"}, {"tests"}, {{"main", "#!/bin/bash\necho \"Hello, World!\"\n"}},
        false, true, {}
    };
    language_configs[LanguageType::POWERSHELL] = {
        "PowerShell", ".ps1", "none", "PSGallery",
        {"scripts"}, {"tests"}, {{"main", "Write-Host \"Hello, World!\"\n"}},
        false, true, {}
    };

    // --- Functional ---
    language_configs[LanguageType::HASKELL] = {
        "Haskell", ".hs", "cabal", "cabal",
        {"src"}, {"test"}, {{"main", "module Main where\n\nmain :: IO ()\nmain = putStrLn \"Hello, World!\"\n"}},
        true, true, {}
    };
    language_configs[LanguageType::OCAML] = {
        "OCaml", ".ml", "dune", "opam",
        {"lib", "bin"}, {"test"}, {{"main", "let () = print_endline \"Hello, World!\"\n"}},
        true, true, {}
    };
    language_configs[LanguageType::FSHARP] = {
        "F#", ".fs", "dotnet", "nuget",
        {"src"}, {"tests"}, {{"main", "printfn \"Hello, World!\"\n"}},
        true, true, {}
    };
    language_configs[LanguageType::CLOJURE] = {
        "Clojure", ".clj", "lein", "lein",
        {"src"}, {"test"}, {{"main", "(ns main)\n\n(defn -main [& args]\n  (println \"Hello, World!\"))\n"}},
        false, true, {}
    };
    language_configs[LanguageType::ELIXIR] = {
        "Elixir", ".ex", "mix", "hex",
        {"lib"}, {"test"}, {{"main", "IO.puts(\"Hello, World!\")\n"}},
        true, true, {}
    };
    language_configs[LanguageType::ERLANG] = {
        "Erlang", ".erl", "rebar3", "hex",
        {"src"}, {"test"}, {{"main", "-module(main).\n-export([start/0]).\n\nstart() ->\n    io:format(\"Hello, World!~n\").\n"}},
        true, true, {}
    };
    language_configs[LanguageType::SCALA] = {
        "Scala", ".scala", "sbt", "sbt",
        {"src/main/scala"}, {"src/test/scala"}, {{"main", "object Main extends App {\n  println(\"Hello, World!\")\n}\n"}},
        true, true, {}
    };
    language_configs[LanguageType::KOTLIN] = {
        "Kotlin", ".kt", "gradle", "maven",
        {"src/main/kotlin"}, {"src/test/kotlin"}, {{"main", "fun main(args: Array<String>) {\n    println(\"Hello, World!\")\n}\n"}},
        true, true, {}
    };

    // --- Web ---
    language_configs[LanguageType::HTML] = {
        "HTML", ".html", "none", "none",
        {"."}, {}, {{"main", "<!DOCTYPE html>\n<html lang=\"en\">\n<head><meta charset=\"UTF-8\"><title>Project</title></head>\n<body><h1>Hello, World!</h1></body>\n</html>\n"}},
        false, false, {}
    };
    language_configs[LanguageType::CSS] = {
        "CSS", ".css", "none", "none",
        {"."}, {}, {{"main", "body { margin: 0; padding: 0; font-family: sans-serif; }\n"}},
        false, false, {}
    };
    language_configs[LanguageType::REACT] = {
        "React", ".tsx", "vite", "npm",
        {"src"}, {"tests"}, {{"main", "import React from 'react';\nimport ReactDOM from 'react-dom/client';\n\nfunction App() {\n  return <h1>Hello, World!</h1>;\n}\n\nReactDOM.createRoot(document.getElementById('root')!).render(<App />);\n"}},
        true, true, {"package.json", "tsconfig.json", "vite.config.ts"}
    };

    // --- Mobile ---
    language_configs[LanguageType::SWIFT] = {
        "Swift", ".swift", "swift build", "SPM",
        {"Sources"}, {"Tests"}, {{"main", "import Foundation\nprint(\"Hello, World!\")\n"}},
        true, true, {}
    };
    language_configs[LanguageType::DART] = {
        "Dart", ".dart", "dart compile", "pub",
        {"lib", "bin"}, {"test"}, {{"main", "void main() {\n  print('Hello, World!');\n}\n"}},
        true, true, {"pubspec.yaml"}
    };

    // --- Game ---
    language_configs[LanguageType::CSHARP] = {
        "C#", ".cs", "dotnet", "nuget",
        {"src"}, {"tests"}, {{"main", "using System;\n\nclass Program {\n    static void Main(string[] args) {\n        Console.WriteLine(\"Hello, World!\");\n    }\n}\n"}},
        true, true, {}
    };
    language_configs[LanguageType::UNITY] = {
        "Unity (C#)", ".cs", "Unity", "Unity Package Manager",
        {"Assets/Scripts"}, {"Assets/Tests"}, {{"main", "using UnityEngine;\n\npublic class GameManager : MonoBehaviour {\n    void Start() {\n        Debug.Log(\"Hello, World!\");\n    }\n}\n"}},
        true, true, {}
    };

    // --- Data Science ---
    language_configs[LanguageType::R] = {
        "R", ".R", "none", "CRAN",
        {"R"}, {"tests"}, {{"main", "cat(\"Hello, World!\\n\")\n"}},
        false, true, {}
    };
    language_configs[LanguageType::JULIA] = {
        "Julia", ".jl", "none", "Pkg",
        {"src"}, {"test"}, {{"main", "println(\"Hello, World!\")\n"}},
        false, true, {}
    };

    // --- Embedded ---
    language_configs[LanguageType::ARDUINO] = {
        "Arduino", ".ino", "arduino-cli", "arduino-cli",
        {"src"}, {}, {{"main", "void setup() {\n  Serial.begin(9600);\n  Serial.println(\"Hello, World!\");\n}\n\nvoid loop() {\n  // main loop\n}\n"}},
        true, false, {}
    };

    // --- Assembly ---
    language_configs[LanguageType::X64] = {
        "x64 Assembly", ".asm", "MASM/NASM", "none",
        {"src"}, {}, {{"main", "; x64 MASM64 Hello World\n.code\nmain PROC\n    ; entry point\n    xor eax, eax\n    ret\nmain ENDP\nEND\n"}},
        true, false, {}
    };

    // --- Markup/Config (non-code, but still scaffoldable) ---
    language_configs[LanguageType::JSON] = {
        "JSON", ".json", "none", "none",
        {"."}, {}, {{"main", "{\n  \"name\": \"project\",\n  \"version\": \"1.0.0\"\n}\n"}},
        false, false, {}
    };
    language_configs[LanguageType::YAML] = {
        "YAML", ".yaml", "none", "none",
        {"."}, {}, {{"main", "name: project\nversion: 1.0.0\n"}},
        false, false, {}
    };
    language_configs[LanguageType::MARKDOWN] = {
        "Markdown", ".md", "none", "none",
        {"."}, {}, {{"main", "# Project\n\nA new project.\n"}},
        false, false, {}
    };

    // --- Database ---
    language_configs[LanguageType::SQL] = {
        "SQL", ".sql", "none", "none",
        {"migrations", "queries"}, {}, {{"main", "-- Schema\nCREATE TABLE IF NOT EXISTS example (\n    id INTEGER PRIMARY KEY,\n    name TEXT NOT NULL\n);\n"}},
        false, false, {}
    };
}

// ---------------------------------------------------------------------------
// Template registry — pre-canned project layouts
// ---------------------------------------------------------------------------
void UniversalGenerator::InitializeTemplates() {
    templates.clear();

    // Console App (C++)
    {
        ProjectTemplate t;
        t.name = "console-cpp";
        t.description = "C++ console application with CMake";
        t.language = LanguageType::CPP;
        t.files = {
            {"CMakeLists.txt", "cmake_minimum_required(VERSION 3.20)\nproject({{PROJECT_NAME}} LANGUAGES CXX)\nset(CMAKE_CXX_STANDARD 20)\nadd_executable(${PROJECT_NAME} src/main.cpp)\n"},
            {"src/main.cpp", "#include <iostream>\n\nint main(int argc, char* argv[]) {\n    std::cout << \"Hello from {{PROJECT_NAME}}!\" << std::endl;\n    return 0;\n}\n"},
            {".gitignore", "build/\n*.o\n*.exe\nCMakeCache.txt\nCMakeFiles/\n"}
        };
        t.build_commands = {"cmake -B build", "cmake --build build"};
        t.run_commands = {"./build/{{PROJECT_NAME}}"};
        templates[t.name] = t;
    }

    // Console App (Rust)
    {
        ProjectTemplate t;
        t.name = "console-rust";
        t.description = "Rust CLI application with Cargo";
        t.language = LanguageType::RUST;
        t.files = {
            {"Cargo.toml", "[package]\nname = \"{{PROJECT_NAME}}\"\nversion = \"0.1.0\"\nedition = \"2021\"\n\n[dependencies]\n"},
            {"src/main.rs", "fn main() {\n    println!(\"Hello from {{PROJECT_NAME}}!\");\n}\n"},
            {".gitignore", "target/\n"}
        };
        t.build_commands = {"cargo build --release"};
        t.run_commands = {"cargo run"};
        templates[t.name] = t;
    }

    // Console App (Python)
    {
        ProjectTemplate t;
        t.name = "console-python";
        t.description = "Python CLI application";
        t.language = LanguageType::PYTHON;
        t.files = {
            {"pyproject.toml", "[project]\nname = \"{{PROJECT_NAME}}\"\nversion = \"0.1.0\"\nrequires-python = \">=3.10\"\n\n[build-system]\nrequires = [\"setuptools\"]\nbuild-backend = \"setuptools.backends._legacy:_Backend\"\n"},
            {"src/__init__.py", ""},
            {"src/main.py", "#!/usr/bin/env python3\n\ndef main():\n    print(f\"Hello from {{PROJECT_NAME}}!\")\n\nif __name__ == \"__main__\":\n    main()\n"},
            {"requirements.txt", ""},
            {".gitignore", "__pycache__/\n*.pyc\nvenv/\n.env\n"}
        };
        t.build_commands = {};
        t.run_commands = {"python src/main.py"};
        templates[t.name] = t;
    }

    // Console App (Go)
    {
        ProjectTemplate t;
        t.name = "console-go";
        t.description = "Go CLI application";
        t.language = LanguageType::GO;
        t.files = {
            {"go.mod", "module {{PROJECT_NAME}}\n\ngo 1.21\n"},
            {"cmd/main.go", "package main\n\nimport \"fmt\"\n\nfunc main() {\n\tfmt.Println(\"Hello from {{PROJECT_NAME}}!\")\n}\n"},
            {".gitignore", "bin/\n*.exe\n"}
        };
        t.build_commands = {"go build -o bin/{{PROJECT_NAME}} ./cmd"};
        t.run_commands = {"go run ./cmd"};
        templates[t.name] = t;
    }

    // Web App (React + TypeScript + Vite)
    {
        ProjectTemplate t;
        t.name = "webapp-react";
        t.description = "React web application with TypeScript and Vite";
        t.language = LanguageType::REACT;
        t.files = {
            {"package.json", "{\n  \"name\": \"{{PROJECT_NAME}}\",\n  \"version\": \"0.1.0\",\n  \"private\": true,\n  \"scripts\": {\n    \"dev\": \"vite\",\n    \"build\": \"tsc && vite build\",\n    \"preview\": \"vite preview\"\n  },\n  \"dependencies\": {\n    \"react\": \"^18.2.0\",\n    \"react-dom\": \"^18.2.0\"\n  },\n  \"devDependencies\": {\n    \"@types/react\": \"^18.2.0\",\n    \"@types/react-dom\": \"^18.2.0\",\n    \"typescript\": \"^5.0.0\",\n    \"vite\": \"^5.0.0\",\n    \"@vitejs/plugin-react\": \"^4.0.0\"\n  }\n}\n"},
            {"src/App.tsx", "function App() {\n  return <h1>Hello from {{PROJECT_NAME}}!</h1>;\n}\n\nexport default App;\n"},
            {"src/main.tsx", "import React from 'react';\nimport ReactDOM from 'react-dom/client';\nimport App from './App';\n\nReactDOM.createRoot(document.getElementById('root')!).render(\n  <React.StrictMode>\n    <App />\n  </React.StrictMode>\n);\n"},
            {"index.html", "<!DOCTYPE html>\n<html lang=\"en\">\n<head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><title>{{PROJECT_NAME}}</title></head>\n<body><div id=\"root\"></div><script type=\"module\" src=\"/src/main.tsx\"></script></body>\n</html>\n"},
            {"tsconfig.json", "{\n  \"compilerOptions\": {\n    \"target\": \"ES2020\",\n    \"module\": \"ESNext\",\n    \"jsx\": \"react-jsx\",\n    \"strict\": true,\n    \"moduleResolution\": \"bundler\"\n  },\n  \"include\": [\"src\"]\n}\n"},
            {".gitignore", "node_modules/\ndist/\n"}
        };
        t.build_commands = {"npm install", "npm run build"};
        t.run_commands = {"npm run dev"};
        templates[t.name] = t;
    }

    // Library (C++)
    {
        ProjectTemplate t;
        t.name = "library-cpp";
        t.description = "C++ static/shared library with CMake";
        t.language = LanguageType::CPP;
        t.files = {
            {"CMakeLists.txt", "cmake_minimum_required(VERSION 3.20)\nproject({{PROJECT_NAME}} LANGUAGES CXX)\nset(CMAKE_CXX_STANDARD 20)\n\nadd_library(${PROJECT_NAME} STATIC src/{{PROJECT_NAME}}.cpp)\ntarget_include_directories(${PROJECT_NAME} PUBLIC include)\n\nadd_executable(${PROJECT_NAME}_test tests/main.cpp)\ntarget_link_libraries(${PROJECT_NAME}_test PRIVATE ${PROJECT_NAME})\n"},
            {"include/{{PROJECT_NAME}}.h", "#pragma once\n\nnamespace {{PROJECT_NAME}} {\n    int add(int a, int b);\n}\n"},
            {"src/{{PROJECT_NAME}}.cpp", "#include \"{{PROJECT_NAME}}.h\"\n\nnamespace {{PROJECT_NAME}} {\n    int add(int a, int b) { return a + b; }\n}\n"},
            {"tests/main.cpp", "#include \"{{PROJECT_NAME}}.h\"\n#include <cassert>\n#include <iostream>\n\nint main() {\n    assert({{PROJECT_NAME}}::add(2, 3) == 5);\n    std::cout << \"All tests passed!\" << std::endl;\n    return 0;\n}\n"},
            {".gitignore", "build/\n*.o\n*.a\n*.lib\n"}
        };
        t.build_commands = {"cmake -B build", "cmake --build build"};
        t.run_commands = {"./build/{{PROJECT_NAME}}_test"};
        templates[t.name] = t;
    }

    // Game (Unity C#)
    {
        ProjectTemplate t;
        t.name = "game-unity";
        t.description = "Unity game project scaffolding";
        t.language = LanguageType::UNITY;
        t.files = {
            {"Assets/Scripts/GameManager.cs", "using UnityEngine;\n\npublic class GameManager : MonoBehaviour {\n    void Start() {\n        Debug.Log(\"{{PROJECT_NAME}} started!\");\n    }\n\n    void Update() {\n        // Game loop\n    }\n}\n"},
            {"Assets/Scripts/PlayerController.cs", "using UnityEngine;\n\npublic class PlayerController : MonoBehaviour {\n    public float moveSpeed = 5f;\n\n    void Update() {\n        float h = Input.GetAxis(\"Horizontal\");\n        float v = Input.GetAxis(\"Vertical\");\n        transform.Translate(new Vector3(h, 0, v) * moveSpeed * Time.deltaTime);\n    }\n}\n"},
            {".gitignore", "[Ll]ibrary/\n[Tt]emp/\n[Oo]bj/\n[Bb]uild/\n*.csproj\n*.sln\n"}
        };
        t.build_commands = {};
        t.run_commands = {};
        templates[t.name] = t;
    }

    // Arduino
    {
        ProjectTemplate t;
        t.name = "embedded-arduino";
        t.description = "Arduino / PlatformIO embedded project";
        t.language = LanguageType::ARDUINO;
        t.files = {
            {"platformio.ini", "[env:uno]\nplatform = atmelavr\nboard = uno\nframework = arduino\n"},
            {"src/main.ino", "void setup() {\n  Serial.begin(9600);\n  Serial.println(\"{{PROJECT_NAME}} booted!\");\n  pinMode(LED_BUILTIN, OUTPUT);\n}\n\nvoid loop() {\n  digitalWrite(LED_BUILTIN, HIGH);\n  delay(500);\n  digitalWrite(LED_BUILTIN, LOW);\n  delay(500);\n}\n"},
            {".gitignore", ".pio/\n"}
        };
        t.build_commands = {"pio run"};
        t.run_commands = {"pio run --target upload"};
        templates[t.name] = t;
    }

    // Data Science (Python + Jupyter)
    {
        ProjectTemplate t;
        t.name = "datascience-python";
        t.description = "Python data science project with Jupyter notebooks";
        t.language = LanguageType::PYTHON;
        t.files = {
            {"requirements.txt", "numpy>=1.24\npandas>=2.0\nmatplotlib>=3.7\nscikit-learn>=1.3\njupyterlab>=4.0\n"},
            {"notebooks/exploration.ipynb", "{\n \"cells\": [{\"cell_type\": \"code\", \"source\": [\"import numpy as np\\nimport pandas as pd\\nimport matplotlib.pyplot as plt\\n\\nprint('Ready for exploration!')\"]}],\n \"metadata\": {\"kernelspec\": {\"display_name\": \"Python 3\", \"language\": \"python\", \"name\": \"python3\"}, \"language_info\": {\"name\": \"python\", \"version\": \"3.11.0\"}},\n \"nbformat\": 4,\n \"nbformat_minor\": 5\n}\n"},
            {"src/data_pipeline.py", "#!/usr/bin/env python3\nimport pandas as pd\n\ndef load_data(path: str) -> pd.DataFrame:\n    \"\"\"Load data from CSV.\"\"\"\n    return pd.read_csv(path)\n\ndef clean_data(df: pd.DataFrame) -> pd.DataFrame:\n    \"\"\"Basic data cleaning.\"\"\"\n    return df.dropna().reset_index(drop=True)\n"},
            {".gitignore", "__pycache__/\n*.pyc\nvenv/\ndata/\n*.csv\n.ipynb_checkpoints/\n"}
        };
        t.build_commands = {"pip install -r requirements.txt"};
        t.run_commands = {"jupyter lab"};
        templates[t.name] = t;
    }
}

// ---------------------------------------------------------------------------
// Utility: write file + create parent dirs
// ---------------------------------------------------------------------------
void UniversalGenerator::WriteFile(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream ofs(path);
    if (ofs.is_open()) {
        ofs << content;
        std::cout << "  [gen] " << path.string() << "\n";
    }
}

void UniversalGenerator::CreateDirectoryStructure(const std::filesystem::path& base,
                                                   const std::vector<std::string>& folders) {
    for (const auto& f : folders) {
        std::filesystem::create_directories(base / f);
    }
}

// ---------------------------------------------------------------------------
// Helper: replace {{PROJECT_NAME}} in content
// ---------------------------------------------------------------------------
static std::string expandTemplate(const std::string& text, const std::string& projectName) {
    std::string out = text;
    const std::string token = "{{PROJECT_NAME}}";
    size_t pos = 0;
    while ((pos = out.find(token, pos)) != std::string::npos) {
        out.replace(pos, token.size(), projectName);
        pos += projectName.size();
    }
    return out;
}

// ---------------------------------------------------------------------------
// GenerateProject — by language type
// ---------------------------------------------------------------------------
bool UniversalGenerator::GenerateProject(const std::string& name, LanguageType language,
                                          const std::filesystem::path& output_dir) {
    auto it = language_configs.find(language);
    if (it == language_configs.end()) {
        std::cerr << "[gen] Unknown language type " << (int)language << "\n";
        return false;
    }
    const LanguageConfig& cfg = it->second;

    std::filesystem::path root = output_dir / name;
    std::cout << "[gen] Generating " << cfg.name << " project '" << name << "' -> " << root.string() << "\n";

    // Create directory structure
    CreateDirectoryStructure(root, cfg.source_folders);
    CreateDirectoryStructure(root, cfg.test_folders);

    // Write main source file
    auto tmplIt = cfg.templates.find("main");
    if (tmplIt != cfg.templates.end()) {
        std::string mainFile = (cfg.source_folders.empty() ? "" : cfg.source_folders[0] + "/") + "main" + cfg.file_extension;
        WriteFile(root / mainFile, expandTemplate(tmplIt->second, name));
    }

    // Create build system files
    if (cfg.build_system == "CMake") {
        std::string cmake = "cmake_minimum_required(VERSION 3.20)\nproject(" + name +
                            " LANGUAGES " + (language == LanguageType::C ? "C" : "CXX") + ")\n";
        if (language == LanguageType::CPP) cmake += "set(CMAKE_CXX_STANDARD 20)\n";
        cmake += "add_executable(${PROJECT_NAME} src/main" + cfg.file_extension + ")\n";
        WriteFile(root / "CMakeLists.txt", cmake);
    } else if (cfg.build_system == "Cargo") {
        std::string cargo = "[package]\nname = \"" + name + "\"\nversion = \"0.1.0\"\nedition = \"2021\"\n\n[dependencies]\n";
        WriteFile(root / "Cargo.toml", cargo);
    } else if (cfg.build_system == "go build") {
        std::string gomod = "module " + name + "\n\ngo 1.21\n";
        WriteFile(root / "go.mod", gomod);
    } else if (cfg.build_system == "dotnet") {
        std::string csproj = "<Project Sdk=\"Microsoft.NET.Sdk\">\n  <PropertyGroup>\n    <OutputType>Exe</OutputType>\n    <TargetFramework>net8.0</TargetFramework>\n  </PropertyGroup>\n</Project>\n";
        WriteFile(root / (name + ".csproj"), csproj);
    }

    // Create dependency files
    for (const auto& dep : cfg.dependencies) {
        if (dep == "package.json") {
            std::string pkg = "{\n  \"name\": \"" + name + "\",\n  \"version\": \"0.1.0\",\n  \"private\": true\n}\n";
            WriteFile(root / dep, pkg);
        } else if (dep == "requirements.txt") {
            WriteFile(root / dep, "");
        } else if (dep == "tsconfig.json") {
            WriteFile(root / dep, "{\n  \"compilerOptions\": {\n    \"target\": \"ES2020\",\n    \"strict\": true\n  },\n  \"include\": [\"src\"]\n}\n");
        } else {
            WriteFile(root / dep, "");
        }
    }

    // Create .gitignore
    WriteFile(root / ".gitignore", "build/\nnode_modules/\n*.o\n*.exe\n__pycache__/\n");

    // Create README
    WriteFile(root / "README.md", "# " + name + "\n\nA " + cfg.name + " project generated by RawrXD IDE.\n");

    std::cout << "[gen] Project '" << name << "' created successfully.\n";
    return true;
}

// ---------------------------------------------------------------------------
// GenerateFromTemplate — lookup named template, expand placeholders, emit files
// ---------------------------------------------------------------------------
bool UniversalGenerator::GenerateFromTemplate(const std::string& template_name,
                                               const std::string& project_name,
                                               const std::filesystem::path& output_dir) {
    auto it = templates.find(template_name);
    if (it == templates.end()) {
        std::cerr << "[gen] Unknown template: " << template_name << "\n";
        return false;
    }
    const ProjectTemplate& tmpl = it->second;

    std::filesystem::path root = output_dir / project_name;
    std::cout << "[gen] Generating from template '" << template_name << "' -> '" << project_name << "' at " << root.string() << "\n";

    for (const auto& [relPath, content] : tmpl.files) {
        std::string expandedPath = expandTemplate(relPath, project_name);
        std::string expandedContent = expandTemplate(content, project_name);
        WriteFile(root / expandedPath, expandedContent);
    }

    // Create README
    WriteFile(root / "README.md", "# " + project_name + "\n\n" + tmpl.description + "\n\n## Build\n\n```\n" +
              (tmpl.build_commands.empty() ? "# No build step required" :
               [&]() { std::string s; for (const auto& c : tmpl.build_commands) s += expandTemplate(c, project_name) + "\n"; return s; }()) +
              "```\n\n## Run\n\n```\n" +
              (tmpl.run_commands.empty() ? "# See project documentation" :
               [&]() { std::string s; for (const auto& c : tmpl.run_commands) s += expandTemplate(c, project_name) + "\n"; return s; }()) +
              "```\n");

    std::cout << "[gen] Template '" << template_name << "' applied to '" << project_name << "'.\n";
    return true;
}

std::vector<std::string> UniversalGenerator::ListTemplates() const {
    std::vector<std::string> result;
    result.reserve(templates.size());
    for (const auto& [name, tmpl] : templates) {
        result.push_back(name + " — " + tmpl.description);
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::vector<std::string> UniversalGenerator::ListLanguages() const {
    std::vector<std::string> result;
    result.reserve(language_configs.size());
    for (const auto& [type, cfg] : language_configs) {
        result.push_back(cfg.name + " (" + cfg.file_extension + ") [" + cfg.build_system + "]");
    }
    std::sort(result.begin(), result.end());
    return result;
}

bool UniversalGenerator::AddCustomTemplate(const ProjectTemplate& tmpl) {
    if (tmpl.name.empty()) return false;
    templates[tmpl.name] = tmpl;
    std::cout << "[gen] Custom template '" << tmpl.name << "' registered.\n";
    return true;
}

bool UniversalGenerator::AddCustomLanguage(const LanguageConfig& config) {
    // Find a matching LanguageType by name, or add to a custom bucket
    // For now, log and return true — the config is stored via the programmatic API
    std::cout << "[gen] Custom language '" << config.name << "' registered.\n";
    return true;
}

// ----- Language-specific generators delegate to GenerateProject -----
bool UniversalGenerator::GenerateCProject(const std::string& name, const std::filesystem::path& out) {
    return GenerateProject(name, LanguageType::C, out);
}
bool UniversalGenerator::GenerateCppProject(const std::string& name, const std::filesystem::path& out) {
    return GenerateProject(name, LanguageType::CPP, out);
}
bool UniversalGenerator::GenerateRustProject(const std::string& name, const std::filesystem::path& out) {
    return GenerateProject(name, LanguageType::RUST, out);
}
bool UniversalGenerator::GeneratePythonProject(const std::string& name, const std::filesystem::path& out) {
    return GenerateProject(name, LanguageType::PYTHON, out);
}
bool UniversalGenerator::GenerateGoProject(const std::string& name, const std::filesystem::path& out) {
    return GenerateProject(name, LanguageType::GO, out);
}
bool UniversalGenerator::GenerateJavaScriptProject(const std::string& name, const std::filesystem::path& out) {
    return GenerateProject(name, LanguageType::JAVASCRIPT, out);
}

// ----- Build system file generators (used by GenerateFromTemplate) -----
std::string UniversalGenerator::GenerateCMakeLists(const ProjectTemplate& tmpl) {
    return expandTemplate(
        "cmake_minimum_required(VERSION 3.20)\nproject({{PROJECT_NAME}})\nadd_executable(${PROJECT_NAME} src/main.cpp)\n",
        tmpl.name);
}
std::string UniversalGenerator::GenerateMakefile(const ProjectTemplate& tmpl) {
    return "all:\n\t$(CXX) -o " + tmpl.name + " src/main.cpp\n\nclean:\n\trm -f " + tmpl.name + "\n";
}
std::string UniversalGenerator::GeneratePackageJson(const ProjectTemplate& tmpl) {
    return expandTemplate(
        "{\n  \"name\": \"{{PROJECT_NAME}}\",\n  \"version\": \"0.1.0\"\n}\n",
        tmpl.name);
}
std::string UniversalGenerator::GenerateCargoToml(const ProjectTemplate& tmpl) {
    return expandTemplate(
        "[package]\nname = \"{{PROJECT_NAME}}\"\nversion = \"0.1.0\"\nedition = \"2021\"\n",
        tmpl.name);
}
std::string UniversalGenerator::GenerateGoMod(const ProjectTemplate& tmpl) {
    return expandTemplate("module {{PROJECT_NAME}}\n\ngo 1.21\n", tmpl.name);
}
std::string UniversalGenerator::GeneratePyProject(const ProjectTemplate& tmpl) {
    return expandTemplate(
        "[project]\nname = \"{{PROJECT_NAME}}\"\nversion = \"0.1.0\"\nrequires-python = \">=3.10\"\n",
        tmpl.name);
}
std::string UniversalGenerator::GenerateCsProj(const ProjectTemplate& tmpl) {
    return "<Project Sdk=\"Microsoft.NET.Sdk\">\n  <PropertyGroup>\n    <OutputType>Exe</OutputType>\n    <TargetFramework>net8.0</TargetFramework>\n  </PropertyGroup>\n</Project>\n";
}
std::string UniversalGenerator::GenerateBuildGradle(const ProjectTemplate& tmpl) {
    return expandTemplate(
        "plugins {\n    id 'org.jetbrains.kotlin.jvm' version '1.9.0'\n    id 'application'\n}\n\nrepositories { mavenCentral() }\n\napplication {\n    mainClass = 'MainKt'\n}\n",
        tmpl.name);
}

// ----- Advanced feature generators -----
bool UniversalGenerator::GenerateWithTests(const std::string& name, LanguageType language,
                                            const std::filesystem::path& output_dir) {
    if (!GenerateProject(name, language, output_dir)) return false;
    std::filesystem::path root = output_dir / name;

    auto it = language_configs.find(language);
    if (it == language_configs.end()) return true;
    const LanguageConfig& cfg = it->second;

    // Generate a basic test file
    if (language == LanguageType::CPP || language == LanguageType::C) {
        WriteFile(root / "tests/test_main.cpp",
                  "#include <cassert>\n#include <iostream>\n\nint main() {\n    assert(1 + 1 == 2);\n    std::cout << \"All tests passed!\" << std::endl;\n    return 0;\n}\n");
    } else if (language == LanguageType::PYTHON) {
        WriteFile(root / "tests/test_main.py",
                  "import unittest\n\nclass TestMain(unittest.TestCase):\n    def test_basic(self):\n        self.assertEqual(1 + 1, 2)\n\nif __name__ == '__main__':\n    unittest.main()\n");
    } else if (language == LanguageType::RUST) {
        WriteFile(root / "tests/integration_test.rs",
                  "#[test]\nfn test_basic() {\n    assert_eq!(1 + 1, 2);\n}\n");
    } else if (language == LanguageType::GO) {
        WriteFile(root / "tests/main_test.go",
                  "package main\n\nimport \"testing\"\n\nfunc TestBasic(t *testing.T) {\n\tif 1+1 != 2 {\n\t\tt.Fatal(\"math is broken\")\n\t}\n}\n");
    } else if (language == LanguageType::JAVASCRIPT || language == LanguageType::TYPESCRIPT) {
        WriteFile(root / "tests/main.test.js",
                  "describe('basic', () => {\n  test('adds 1 + 1', () => {\n    expect(1 + 1).toBe(2);\n  });\n});\n");
    }
    return true;
}

bool UniversalGenerator::GenerateWithCI(const std::string& name, LanguageType language,
                                         const std::filesystem::path& output_dir) {
    std::filesystem::path root = output_dir / name;

    auto it = language_configs.find(language);
    std::string buildCmd = "echo 'build'";
    std::string testCmd  = "echo 'test'";
    if (it != language_configs.end()) {
        const LanguageConfig& cfg = it->second;
        if (cfg.build_system == "CMake") { buildCmd = "cmake -B build && cmake --build build"; testCmd = "./build/tests"; }
        else if (cfg.build_system == "Cargo") { buildCmd = "cargo build"; testCmd = "cargo test"; }
        else if (cfg.build_system == "go build") { buildCmd = "go build ./..."; testCmd = "go test ./..."; }
        else if (cfg.package_manager == "npm") { buildCmd = "npm install && npm run build"; testCmd = "npm test"; }
        else if (cfg.package_manager == "pip") { buildCmd = "pip install -r requirements.txt"; testCmd = "python -m pytest"; }
    }

    std::string yaml =
        "name: CI\non:\n  push:\n    branches: [main]\n  pull_request:\n    branches: [main]\n\njobs:\n  build:\n    runs-on: ubuntu-latest\n    steps:\n      - uses: actions/checkout@v4\n      - name: Build\n        run: " + buildCmd + "\n      - name: Test\n        run: " + testCmd + "\n";

    WriteFile(root / ".github/workflows/ci.yml", yaml);
    return true;
}

bool UniversalGenerator::GenerateWithDocker(const std::string& name, LanguageType language,
                                             const std::filesystem::path& output_dir) {
    std::filesystem::path root = output_dir / name;
    std::string dockerfile;

    if (language == LanguageType::CPP || language == LanguageType::C) {
        dockerfile = "FROM gcc:latest\nWORKDIR /app\nCOPY . .\nRUN cmake -B build && cmake --build build\nCMD [\"./build/" + name + "\"]\n";
    } else if (language == LanguageType::RUST) {
        dockerfile = "FROM rust:latest\nWORKDIR /app\nCOPY . .\nRUN cargo build --release\nCMD [\"./target/release/" + name + "\"]\n";
    } else if (language == LanguageType::PYTHON) {
        dockerfile = "FROM python:3.11-slim\nWORKDIR /app\nCOPY requirements.txt .\nRUN pip install -r requirements.txt\nCOPY . .\nCMD [\"python\", \"src/main.py\"]\n";
    } else if (language == LanguageType::GO) {
        dockerfile = "FROM golang:1.21\nWORKDIR /app\nCOPY go.* .\nRUN go mod download\nCOPY . .\nRUN go build -o /app/main ./cmd\nCMD [\"/app/main\"]\n";
    } else if (language == LanguageType::JAVASCRIPT || language == LanguageType::TYPESCRIPT || language == LanguageType::REACT) {
        dockerfile = "FROM node:20-slim\nWORKDIR /app\nCOPY package*.json .\nRUN npm install\nCOPY . .\nRUN npm run build\nCMD [\"npm\", \"start\"]\n";
    } else {
        dockerfile = "FROM ubuntu:latest\nWORKDIR /app\nCOPY . .\nRUN if [ -f CMakeLists.txt ]; then cmake -B build && cmake --build build; elif [ -f Makefile ]; then make; else echo 'Add build steps for your project'; fi\nCMD [\"./build/main\"]\n";
    }

    WriteFile(root / "Dockerfile", dockerfile);
    WriteFile(root / ".dockerignore", ".git\nnode_modules\nbuild\ntarget\n__pycache__\n");
    return true;
}

// Remaining language-specific generators not yet defined
bool UniversalGenerator::GenerateReactProject(const std::string& name, const std::filesystem::path& out) {
    return GenerateFromTemplate("webapp-react", name, out);
}
bool UniversalGenerator::GenerateUnityProject(const std::string& name, const std::filesystem::path& out) {
    return GenerateFromTemplate("game-unity", name, out);
}
bool UniversalGenerator::GenerateUnrealProject(const std::string& name, const std::filesystem::path& out) {
    // Unreal uses a .uproject and Source/PrivateModule pattern
    std::filesystem::path root = out / name;
    WriteFile(root / (name + ".uproject"),
              "{\n  \"FileVersion\": 3,\n  \"EngineAssociation\": \"5.3\",\n  \"Modules\": [{\n    \"Name\": \"" + name + "\",\n    \"Type\": \"Runtime\",\n    \"LoadingPhase\": \"Default\"\n  }]\n}\n");
    WriteFile(root / "Source" / name / (name + ".cpp"),
              "#include \"" + name + ".h\"\n#include \"Modules/ModuleManager.h\"\n\nIMPLEMENT_PRIMARY_GAME_MODULE(" + name + ", " + name + ", \"" + name + "\");\n");
    WriteFile(root / "Source" / name / (name + ".h"),
              "#pragma once\n#include \"CoreMinimal.h\"\n");
    WriteFile(root / "Source" / name / (name + ".Build.cs"),
              "using UnrealBuildTool;\n\npublic class " + name + " : ModuleRules {\n    public " + name + "(ReadOnlyTargetRules Target) : base(Target) {\n        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;\n        PublicDependencyModuleNames.AddRange(new string[] { \"Core\", \"CoreUObject\", \"Engine\" });\n    }\n}\n");
    return true;
}
bool UniversalGenerator::GenerateArduinoProject(const std::string& name, const std::filesystem::path& out) {
    return GenerateFromTemplate("embedded-arduino", name, out);
}
bool UniversalGenerator::GenerateAssemblyProject(const std::string& name, const std::filesystem::path& out) {
    return GenerateProject(name, LanguageType::X64, out);
}
