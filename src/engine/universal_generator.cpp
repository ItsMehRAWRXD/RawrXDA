#include "universal_generator.h"
#include "react_ide_generator.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

UniversalGenerator::UniversalGenerator() {
    InitializeLanguages();
    InitializeTemplates();
}

void UniversalGenerator::InitializeLanguages() {
    // Systems Programming
    language_configs[LanguageType::C] = {
        "C", ".c", "Makefile", "",
        {"src", "include"}, {"tests"},
        {{"main.c", "#include <stdio.h>\n\nint main() {\n    printf(\"Hello, World!\\n\");\n    return 0;\n}"}},
        true, false, {}
    };
    
    language_configs[LanguageType::CPP] = {
        "C++", ".cpp", "CMake", "",
        {"src", "include"}, {"tests"},
        {{"main.cpp", "#include <iostream>\n\nint main() {\n    std::cout << \"Hello, World!\" << std::endl;\n    return 0;\n}"}},
        true, false, {}
    };
    
    language_configs[LanguageType::RUST] = {
        "Rust", ".rs", "Cargo", "cargo",
        {"src"}, {"tests"},
        {{"main.rs", "fn main() {\n    println!(\"Hello, World!\");\n}"}},
        true, false, {}
    };
    
    language_configs[LanguageType::GO] = {
        "Go", ".go", "Go Modules", "go",
        {""}, {""},
        {{"main.go", "package main\n\nimport \"fmt\"\n\nfunc main() {\n    fmt.Println(\"Hello, World!\")\n}"}},
        true, false, {}
    };
    
    language_configs[LanguageType::ZIG] = {
        "Zig", ".zig", "Zig Build", "",
        {"src"}, {"tests"},
        {{"main.zig", "const std = @import(\"std\");\n\npub fn main() !void {\n    const stdout = std.io.getStdOut().writer();\n    try stdout.print(\"Hello, World!\\n\", .{});\n}"}},
        true, false, {}
    };
    
    // Scripting Languages
    language_configs[LanguageType::PYTHON] = {
        "Python", ".py", "pip", "pip",
        {"src"}, {"tests"},
        {{"main.py", "def main():\n    print(\"Hello, World!\")\n\nif __name__ == \"__main__\":\n    main()"}},
        false, true, {}
    };
    
    language_configs[LanguageType::JAVASCRIPT] = {
        "JavaScript", ".js", "npm", "npm",
        {"src"}, {"tests"},
        {{"index.js", "console.log(\"Hello, World!\");"}},
        false, true, {}
    };
    
    language_configs[LanguageType::TYPESCRIPT] = {
        "TypeScript", ".ts", "npm", "npm",
        {"src"}, {"tests"},
        {{"index.ts", "console.log(\"Hello, World!\");"}},
        true, true, {}
    };
    
    language_configs[LanguageType::LUA] = {
        "Lua", ".lua", "", "",
        {"src"}, {"tests"},
        {{"main.lua", "print(\"Hello, World!\")"}},
        false, true, {}
    };
    
    language_configs[LanguageType::RUBY] = {
        "Ruby", ".rb", "gem", "gem",
        {"lib"}, {"test"},
        {{"main.rb", "puts \"Hello, World!\""}},
        false, true, {}
    };
    
    language_configs[LanguageType::PHP] = {
        "PHP", ".php", "composer", "composer",
        {"src"}, {"tests"},
        {{"index.php", "<?php\necho \"Hello, World!\";\n?>"}},
        false, true, {}
    };
    
    // Functional Languages
    language_configs[LanguageType::HASKELL] = {
        "Haskell", ".hs", "Cabal", "cabal",
        {"src"}, {"test"},
        {{"Main.hs", "main :: IO ()\nmain = putStrLn \"Hello, World!\""}},
        true, false, {}
    };
    
    language_configs[LanguageType::OCAML] = {
        "OCaml", ".ml", "dune", "opam",
        {"lib"}, {"test"},
        {{"main.ml", "print_endline \"Hello, World!\""}},
        true, false, {}
    };
    
    language_configs[LanguageType::CLOJURE] = {
        "Clojure", ".clj", "Leiningen", "lein",
        {"src"}, {"test"},
        {{"core.clj", "(ns hello-world.core)\n(defn -main [& args]\n  (println \"Hello, World!\"))"}},
        false, true, {}
    };
    
    // Web Frameworks
    language_configs[LanguageType::REACT] = {
        "React", ".jsx", "npm", "npm",
        {"src"}, {"tests"},
        {{"App.jsx", "import React from 'react';\n\nfunction App() {\n  return <h1>Hello, World!</h1>;\n}\n\nexport default App;"}},
        true, true, {}
    };
    
    language_configs[LanguageType::VUE] = {
        "Vue", ".vue", "npm", "npm",
        {"src"}, {"tests"},
        {{"App.vue", "<template>\n  <h1>Hello, World!</h1>\n</template>\n\n<script>\nexport default {\n  name: 'App'\n}\n</script>"}},
        true, true, {}
    };
    
    language_configs[LanguageType::SVELTE] = {
        "Svelte", ".svelte", "npm", "npm",
        {"src"}, {"tests"},
        {{"App.svelte", "<h1>Hello, World!</h1>"}},
        true, true, {}
    };
    
    // Mobile Development
    language_configs[LanguageType::SWIFT] = {
        "Swift", ".swift", "SwiftPM", "swift",
        {"Sources"}, {"Tests"},
        {{"main.swift", "print(\"Hello, World!\")"}},
        true, false, {}
    };
    
    language_configs[LanguageType::DART] = {
        "Dart", ".dart", "pub", "pub",
        {"lib"}, {"test"},
        {{"main.dart", "void main() {\n  print('Hello, World!');\n}"}},
        false, true, {}
    };
    
    // Game Development
    language_configs[LanguageType::CSHARP] = {
        "C#", ".cs", "MSBuild", "nuget",
        {"src"}, {"tests"},
        {{"Program.cs", "using System;\n\nclass Program {\n    static void Main() {\n        Console.WriteLine(\"Hello, World!\");\n    }\n}"}},
        true, false, {}
    };
    
    language_configs[LanguageType::UNITY] = {
        "Unity C#", ".cs", "Unity", "",
        {"Assets/Scripts"}, {"Assets/Tests"},
        {{"GameManager.cs", "using UnityEngine;\n\npublic class GameManager : MonoBehaviour {\n    void Start() {\n        Debug.Log(\"Hello, World!\");\n    }\n}"}},
        true, false, {}
    };
    
    // Data Science
    language_configs[LanguageType::R] = {
        "R", ".r", "", "",
        {"R"}, {"tests"},
        {{"main.r", "cat(\"Hello, World!\\n\")"}},
        false, true, {}
    };
    
    language_configs[LanguageType::JULIA] = {
        "Julia", ".jl", "", "",
        {"src"}, {"test"},
        {{"main.jl", "println(\"Hello, World!\")"}},
        false, true, {}
    };
    
    // Embedded Systems
    language_configs[LanguageType::ARDUINO] = {
        "Arduino", ".ino", "Arduino CLI", "",
        {""}, {""},
        {{"sketch.ino", "void setup() {\n  Serial.begin(9600);\n  Serial.println(\"Hello, World!\");\n}\n\nvoid loop() {\n}"}},
        true, false, {}
    };
    
    language_configs[LanguageType::ESP32] = {
        "ESP32", ".cpp", "PlatformIO", "pio",
        {"src"}, {"test"},
        {{"main.cpp", "#include <Arduino.h>\n\nvoid setup() {\n  Serial.begin(115200);\n  Serial.println(\"Hello, World!\");\n}\n\nvoid loop() {\n}"}},
        true, false, {}
    };
    
    // Assembly
    language_configs[LanguageType::X64] = {
        "x64 Assembly", ".asm", "MASM", "",
        {"src"}, {"tests"},
        {{"main.asm", ".code\nmain proc\n    mov rax, 1\n    ret\nmain endp\nend"}},
        true, false, {}
    };
    
    // Markup/Config
    language_configs[LanguageType::MARKDOWN] = {
        "Markdown", ".md", "", "",
        {"docs"}, {},
        {{"README.md", "# Project\n\nDescription here.\n"}},
        false, false, {}
    };
    
    // Database
    language_configs[LanguageType::SQL] = {
        "SQL", ".sql", "", "",
        {"sql"}, {},
        {{"schema.sql", "CREATE TABLE users (\n    id INTEGER PRIMARY KEY,\n    name TEXT NOT NULL\n);"}},
        false, false, {}
    };
    
    // Legacy Languages
    language_configs[LanguageType::COBOL] = {
        "COBOL", ".cob", "", "",
        {"src"}, {"tests"},
        {{"main.cob", "IDENTIFICATION DIVISION.\nPROGRAM-ID. HelloWorld.\nPROCEDURE DIVISION.\n    DISPLAY 'Hello, World!'.\n    STOP RUN."}},
        true, false, {}
    };
    
    language_configs[LanguageType::FORTRAN] = {
        "Fortran", ".f90", "", "",
        {"src"}, {"tests"},
        {{"main.f90", "program hello\n    print *, \"Hello, World!\"\nend program hello"}},
        true, false, {}
    };
}

void UniversalGenerator::InitializeTemplates() {
    // Web Application Template
    templates["web-app"] = {
        "web-app", "Full-stack web application",
        LanguageType::JAVASCRIPT,
        {
            {"package.json", GeneratePackageJson},
            {"src/index.js", "const express = require('express');\nconst app = express();\n\napp.get('/', (req, res) => {\n  res.send('Hello, World!');\n});\n\napp.listen(3000, () => {\n  console.log('Server running on port 3000');\n});"},
            {"public/index.html", "<!DOCTYPE html><html><head><title>Web App</title></head><body><h1>Hello, World!</h1></body></html>"},
            {"Dockerfile", "FROM node:18\nWORKDIR /app\nCOPY package*.json ./\nRUN npm install\nCOPY . .\nEXPOSE 3000\nCMD [\"node\", \"src/index.js\"]"}
        },
        {"npm install"},
        {"npm start"}
    };
    
    // CLI Tool Template
    templates["cli-tool"] = {
        "cli-tool", "Command-line interface tool",
        LanguageType::RUST,
        {
            {"Cargo.toml", GenerateCargoToml},
            {"src/main.rs", "use std::env;\n\nfn main() {\n    let args: Vec<String> = env::args().collect();\n    println!(\"Hello, {}!\", args.get(1).map_or(\"World\", |s| s));\n}"},
            {"src/lib.rs", "pub fn greet(name: &str) -> String {\n    format!(\"Hello, {}!\", name)\n}"},
            {".gitignore", "/target\nCargo.lock"}
        },
        {"cargo build --release"},
        {"cargo run --release"}
    };
    
    // Library Template
    templates["library"] = {
        "library", "Reusable library package",
        LanguageType::CPP,
        {
            {"CMakeLists.txt", GenerateCMakeLists},
            {"include/library.h", "#pragma once\n\nnamespace mylib {\n    void hello();\n}"},
            {"src/library.cpp", "#include \"library.h\"\n#include <iostream>\n\nnamespace mylib {\n    void hello() {\n        std::cout << \"Hello from library!\" << std::endl;\n    }\n}"},
            {"tests/test_main.cpp", "#include \"library.h\"\n\nint main() {\n    mylib::hello();\n    return 0;\n}"}
        },
        {"cmake -B build", "cmake --build build"},
        {"./build/library_test"}
    };
    
    // Game Template
    templates["game"] = {
        "game", "2D game project",
        LanguageType::UNITY,
        {
            {"Assets/Scripts/GameManager.cs", "using UnityEngine;\n\npublic class GameManager : MonoBehaviour\n{\n    void Start()\n    {\n        Debug.Log(\"Game started!\");\n    }\n}"},
            {"Assets/Scripts/Player.cs", "using UnityEngine;\n\npublic class Player : MonoBehaviour\n{\n    public float speed = 5f;\n    \n    void Update()\n    {\n        float moveX = Input.GetAxis(\"Horizontal\");\n        float moveY = Input.GetAxis(\"Vertical\");\n        transform.position += new Vector3(moveX, moveY, 0) * speed * Time.deltaTime;\n    }\n}"},
            {"ProjectSettings/ProjectVersion.txt", "m_EditorVersion: 2021.3.0f1"}
        },
        {"Unity -batchmode -quit -buildWindows64Player Build/game.exe"},
        {"Build/game.exe"}
    };
    
    // Embedded Template
    templates["embedded"] = {
        "embedded", "Embedded systems project",
        LanguageType::ESP32,
        {
            {"platformio.ini", "[env:esp32dev]\nplatform = espressif32\nboard = esp32dev\nframework = arduino\n\n[env:native]\nplatform = native"},
            {"src/main.cpp", "#include <Arduino.h>\n\nvoid setup() {\n  Serial.begin(115200);\n  Serial.println(\"ESP32 Ready!\");\n}\n\nvoid loop() {\n  Serial.println(\"Hello from ESP32!\");\n  delay(1000);\n}"},
            {"include/sensors.h", "#pragma once\n\nclass SensorManager {\npublic:\n    void begin();\n    float readTemperature();\n};"},
            {"lib/README", "Place custom libraries here"}
        },
        {"pio run"},
        {"pio run --target upload"}
    };
    
    // Data Science Template
    templates["data-science"] = {
        "data-science", "Data analysis project",
        LanguageType::PYTHON,
        {
            {"requirements.txt", "numpy==1.24.0\npandas==1.5.3\nmatplotlib==3.6.0\nscikit-learn==1.2.0\njupyter==1.0.0"},
            {"src/data_loader.py", "import pandas as pd\n\ndef load_data(filepath):\n    return pd.read_csv(filepath)\n\ndef clean_data(df):\n    return df.dropna()"},
            {"src/analysis.py", "import numpy as np\nfrom sklearn.model_selection import train_test_split\n\ndef analyze_data(df):\n    X = df.drop('target', axis=1)\n    y = df['target']\n    return train_test_split(X, y, test_size=0.2)"},
            {"notebooks/exploration.ipynb", "{\n \"cells\": [\n  {\n   \"cell_type\": \"code\",\n   \"execution_count\": null,\n   \"metadata\": {},\n   \"outputs\": [],\n   \"source\": [\"import pandas as pd\\n\\n# Load data\\ndf = pd.read_csv('../data/sample.csv')\\nprint(df.head())\"]\n  }\n ],\n \"metadata\": {\n  \"kernelspec\": {\n   \"display_name\": \"Python 3\",\n   \"language\": \"python\",\n   \"name\": \"python3\"\n  }\n },\n \"nbformat\": 4,\n \"nbformat_minor\": 4\n}"}
        },
        {"pip install -r requirements.txt"},
        {"jupyter notebook"}
    };
}

std::string UniversalGenerator::GenerateCMakeLists(const ProjectTemplate& tmpl) {
    std::stringstream cmake;
    cmake << "cmake_minimum_required(VERSION 3.20)\n";
    cmake << "project(" << tmpl.name << ")\n\n";
    cmake << "set(CMAKE_CXX_STANDARD 20)\n\n";
    cmake << "# Find packages\n";
    cmake << "find_package(Threads REQUIRED)\n\n";
    cmake << "# Executable\n";
    cmake << "add_executable(${PROJECT_NAME} src/main.cpp)\n\n";
    cmake << "# Include directories\n";
    cmake << "target_include_directories(${PROJECT_NAME} PRIVATE include)\n\n";
    cmake << "# Link libraries\n";
    cmake << "target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)\n";
    return cmake.str();
}

std::string UniversalGenerator::GenerateMakefile(const ProjectTemplate& tmpl) {
    std::stringstream makefile;
    makefile << "CC = gcc\n";
    makefile << "CXX = g++\n";
    makefile << "CFLAGS = -Wall -Wextra -O2\n";
    makefile << "CXXFLAGS = -std=c++20 -Wall -Wextra -O2\n\n";
    makefile << "SRC_DIR = src\n";
    makefile << "INC_DIR = include\n";
    makefile << "BUILD_DIR = build\n\n";
    makefile << "SOURCES = $(wildcard $(SRC_DIR)/*.cpp)\n";
    makefile << "OBJECTS = $(SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)\n";
    makefile << "TARGET = " << tmpl.name << "\n\n";
    makefile << "$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)\n";
    makefile << "\t$(CXX) $(CXXFLAGS) -I$(INC_DIR) -c $< -o $@\n\n";
    makefile << "$(TARGET): $(OBJECTS)\n";
    makefile << "\t$(CXX) $(CXXFLAGS) -o $@ $^\n\n";
    makefile << "$(BUILD_DIR):\n";
    makefile << "\tmkdir -p $(BUILD_DIR)\n\n";
    makefile << ".PHONY: clean\n";
    makefile << "clean:\n";
    makefile << "\trm -rf $(BUILD_DIR) $(TARGET)\n";
    return makefile.str();
}

std::string UniversalGenerator::GeneratePackageJson(const ProjectTemplate& tmpl) {
    std::stringstream json;
    json << "{\n";
    json << "  \"name\": \"" << tmpl.name << "\",\n";
    json << "  \"version\": \"1.0.0\",\n";
    json << "  \"description\": \"" << tmpl.description << "\",\n";
    json << "  \"main\": \"src/index.js\",\n";
    json << "  \"scripts\": {\n";
    json << "    \"start\": \"node src/index.js\",\n";
    json << "    \"test\": \"jest\"\n";
    json << "  },\n";
    json << "  \"dependencies\": {},\n";
    json << "  \"devDependencies\": {\n";
    json << "    \"jest\": \"^29.0.0\"\n";
    json << "  }\n";
    json << "}\n";
    return json.str();
}

std::string UniversalGenerator::GenerateCargoToml(const ProjectTemplate& tmpl) {
    std::stringstream cargo;
    cargo << "[package]\n";
    cargo << "name = \"" << tmpl.name << "\"\n";
    cargo << "version = \"1.0.0\"\n";
    cargo << "edition = \"2021\"\n\n";
    cargo << "[dependencies]\n";
    return cargo.str();
}

std::string UniversalGenerator::GenerateGoMod(const ProjectTemplate& tmpl) {
    std::stringstream gomod;
    gomod << "module " << tmpl.name << "\n\n";
    gomod << "go 1.21\n\n";
    gomod << "require (\n";
    gomod << "\tgithub.com/gorilla/mux v1.8.0\n";
    gomod << ")\n";
    return gomod.str();
}

std::string UniversalGenerator::GeneratePyProject(const ProjectTemplate& tmpl) {
    std::stringstream pyproject;
    pyproject << "[build-system]\n";
    pyproject << "requires = [\"setuptools\", \"wheel\"]\n";
    pyproject << "build-backend = \"setuptools.build_meta\"\n\n";
    pyproject << "[project]\n";
    pyproject << "name = \"" << tmpl.name << "\"\n";
    pyproject << "version = \"1.0.0\"\n";
    pyproject << "description = \"" << tmpl.description << "\"\n";
    pyproject << "requires-python = \">=3.8\"\n";
    return pyproject.str();
}

std::string UniversalGenerator::GenerateCsProj(const ProjectTemplate& tmpl) {
    std::stringstream csproj;
    csproj << "<Project Sdk=\"Microsoft.NET.Sdk\">\n\n";
    csproj << "  <PropertyGroup>\n";
    csproj << "    <OutputType>Exe</OutputType>\n";
    csproj << "    <TargetFramework>net7.0</TargetFramework>\n";
    csproj << "    <Nullable>enable</Nullable>\n";
    csproj << "  </PropertyGroup>\n\n";
    csproj << "</Project>\n";
    return csproj.str();
}

std::string UniversalGenerator::GenerateBuildGradle(const ProjectTemplate& tmpl) {
    std::stringstream gradle;
    gradle << "plugins {\n";
    gradle << "    id 'java'\n";
    gradle << "    id 'application'\n";
    gradle << "}\n\n";
    gradle << "repositories {\n";
    gradle << "    mavenCentral()\n";
    gradle << "}\n\n";
    gradle << "dependencies {\n";
    gradle << "    testImplementation 'junit:junit:4.13.2'\n";
    gradle << "}\n\n";
    gradle << "application {\n";
    gradle << "    mainClass = 'com.example.Main'\n";
    gradle << "}\n";
    return gradle.str();
}

void UniversalGenerator::WriteFile(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path);
    if (file.is_open()) {
        file << content;
        file.close();
    }
}

void UniversalGenerator::CreateDirectoryStructure(const std::filesystem::path& base, const std::vector<std::string>& folders) {
    for (const auto& folder : folders) {
        std::filesystem::create_directories(base / folder);
    }
}

bool UniversalGenerator::GenerateProject(const std::string& name, LanguageType language, 
                                        const std::filesystem::path& output_dir) {
    // Intercept React generation to use the specialized ReactIDEGenerator
    if (language == LanguageType::REACT) {
        ReactIDEGenerator react_gen;
        // Check if name implies full IDE or just a project
        if (name.find("ide") != std::string::npos || name.find("IDE") != std::string::npos) {
             return react_gen.GenerateFullIDE(name, output_dir);
        }
        return react_gen.GenerateMinimalIDE(name, output_dir);
    }

    auto it = language_configs.find(language);
    if (it == language_configs.end()) {
        return false;
    }
    
    const auto& config = it->second;
    std::filesystem::path project_dir = output_dir / name;
    
    // Create directory structure
    CreateDirectoryStructure(project_dir, config.source_folders);
    CreateDirectoryStructure(project_dir, config.test_folders);
    
    // Generate main source file
    std::filesystem::path main_file = project_dir / config.source_folders[0] / ("main" + config.file_extension);
    WriteFile(main_file, config.templates.at("main" + config.file_extension));
    
    // Generate build files based on language
    if (config.build_system == "CMake") {
        WriteFile(project_dir / "CMakeLists.txt", GenerateCMakeLists({name, "", language, {}, {}, {}}));
    } else if (config.build_system == "Cargo") {
        WriteFile(project_dir / "Cargo.toml", GenerateCargoToml({name, "", language, {}, {}, {}}));
    } else if (config.build_system == "Go Modules") {
        WriteFile(project_dir / "go.mod", GenerateGoMod({name, "", language, {}, {}, {}}));
    } else if (config.build_system == "npm") {
        WriteFile(project_dir / "package.json", GeneratePackageJson({name, "", language, {}, {}, {}}));
    } else if (config.build_system == "MSBuild") {
        WriteFile(project_dir / (name + ".csproj"), GenerateCsProj({name, "", language, {}, {}, {}}));
    }
    
    // Generate README
    std::stringstream readme;
    readme << "# " << name << "\n\n";
    readme << "Generated by RawrXD Universal Generator\n\n";
    readme << "## Build\n\n";
    readme << "```bash\n";
    for (const auto& cmd : config.build_system == "Cargo" ? std::vector<std::string>{"cargo build"} :
                              config.build_system == "CMake" ? std::vector<std::string>{"cmake -B build", "cmake --build build"} :
                              config.build_system == "npm" ? std::vector<std::string>{"npm install"} :
                              config.build_system == "Go Modules" ? std::vector<std::string>{"go build"} :
                              std::vector<std::string>{"make"}) {
        readme << cmd << "\n";
    }
    readme << "```\n\n";
    readme << "## Run\n\n";
    readme << "```bash\n";
    readme << config.run_commands[0] << "\n";
    readme << "```\n";
    
    WriteFile(project_dir / "README.md", readme.str());
    
    return true;
}

bool UniversalGenerator::GenerateFromTemplate(const std::string& template_name,
                                             const std::string& project_name,
                                             const std::filesystem::path& output_dir) {
    auto it = templates.find(template_name);
    if (it == templates.end()) {
        return false;
    }
    
    const auto& tmpl = it->second;
    std::filesystem::path project_dir = output_dir / project_name;
    
    // Create all files from template
    for (const auto& [file_path, content] : tmpl.files) {
        WriteFile(project_dir / file_path, content);
    }
    
    // Generate build files if not in template
    if (tmpl.language == LanguageType::CPP && !std::filesystem::exists(project_dir / "CMakeLists.txt")) {
        WriteFile(project_dir / "CMakeLists.txt", GenerateCMakeLists(tmpl));
    } else if (tmpl.language == LanguageType::RUST && !std::filesystem::exists(project_dir / "Cargo.toml")) {
        WriteFile(project_dir / "Cargo.toml", GenerateCargoToml(tmpl));
    }
    
    // Generate README
    std::stringstream readme;
    readme << "# " << project_name << "\n\n";
    readme << tmpl.description << "\n\n";
    readme << "## Build\n\n";
    readme << "```bash\n";
    for (const auto& cmd : tmpl.build_commands) {
        readme << cmd << "\n";
    }
    readme << "```\n\n";
    readme << "## Run\n\n";
    readme << "```bash\n";
    for (const auto& cmd : tmpl.run_commands) {
        readme << cmd << "\n";
    }
    readme << "```\n";
    
    WriteFile(project_dir / "README.md", readme.str());
    
    return true;
}

std::vector<std::string> UniversalGenerator::ListTemplates() const {
    std::vector<std::string> result;
    for (const auto& [name, tmpl] : templates) {
        result.push_back(name + " - " + tmpl.description);
    }
    return result;
}

std::vector<std::string> UniversalGenerator::ListLanguages() const {
    std::vector<std::string> result;
    for (const auto& [type, config] : language_configs) {
        result.push_back(config.name);
    }
    return result;
}

bool UniversalGenerator::AddCustomTemplate(const ProjectTemplate& tmpl) {
    templates[tmpl.name] = tmpl;
    return true;
}

bool UniversalGenerator::AddCustomLanguage(const LanguageConfig& config) {
    // Find next available LanguageType enum value
    LanguageType new_type = static_cast<LanguageType>(static_cast<int>(LanguageType::VALA) + 1);
    language_configs[new_type] = config;
    return true;
}

bool UniversalGenerator::GenerateCProject(const std::string& name, const std::filesystem::path& output_dir) {
    return GenerateProject(name, LanguageType::C, output_dir);
}

bool UniversalGenerator::GenerateCppProject(const std::string& name, const std::filesystem::path& output_dir) {
    return GenerateProject(name, LanguageType::CPP, output_dir);
}

bool UniversalGenerator::GenerateRustProject(const std::string& name, const std::filesystem::path& output_dir) {
    return GenerateProject(name, LanguageType::RUST, output_dir);
}

bool UniversalGenerator::GeneratePythonProject(const std::string& name, const std::filesystem::path& output_dir) {
    return GenerateProject(name, LanguageType::PYTHON, output_dir);
}

bool UniversalGenerator::GenerateGoProject(const std::string& name, const std::filesystem::path& output_dir) {
    return GenerateProject(name, LanguageType::GO, output_dir);
}

bool UniversalGenerator::GenerateJavaScriptProject(const std::string& name, const std::filesystem::path& output_dir) {
    return GenerateProject(name, LanguageType::JAVASCRIPT, output_dir);
}

bool UniversalGenerator::GenerateReactProject(const std::string& name, const std::filesystem::path& output_dir) {
    return GenerateProject(name, LanguageType::REACT, output_dir);
}

bool UniversalGenerator::GenerateUnityProject(const std::string& name, const std::filesystem::path& output_dir) {
    return GenerateProject(name, LanguageType::UNITY, output_dir);
}

bool UniversalGenerator::GenerateUnrealProject(const std::string& name, const std::filesystem::path& output_dir) {
    return GenerateProject(name, LanguageType::UNREAL, output_dir);
}

bool UniversalGenerator::GenerateArduinoProject(const std::string& name, const std::filesystem::path& output_dir) {
    return GenerateProject(name, LanguageType::ARDUINO, output_dir);
}

bool UniversalGenerator::GenerateAssemblyProject(const std::string& name, const std::filesystem::path& output_dir) {
    return GenerateProject(name, LanguageType::X64, output_dir);
}

bool UniversalGenerator::GenerateWithTests(const std::string& name, LanguageType language, 
                                          const std::filesystem::path& output_dir) {
    if (!GenerateProject(name, language, output_dir)) {
        return false;
    }
    
    auto it = language_configs.find(language);
    if (it == language_configs.end()) {
        return false;
    }
    
    const auto& config = it->second;
    std::filesystem::path project_dir = output_dir / name;
    
    // Generate test files based on language
    if (language == LanguageType::CPP) {
        WriteFile(project_dir / "tests" / "test_main.cpp",
            "#include <gtest/gtest.h>\n\nTEST(HelloTest, BasicAssertion) {\n    EXPECT_STRNE(\"hello\", \"world\");\n    EXPECT_EQ(7 * 6, 42);\n}\n\nint main(int argc, char **argv) {\n    ::testing::InitGoogleTest(&argc, argv);\n    return RUN_ALL_TESTS();\n}");
    } else if (language == LanguageType::PYTHON) {
        WriteFile(project_dir / "tests" / "test_main.py",
            "import unittest\n\nclass TestMain(unittest.TestCase):\n    def test_hello(self):\n        self.assertEqual(1 + 1, 2)\n\nif __name__ == '__main__':\n    unittest.main()");
    } else if (language == LanguageType::RUST) {
        WriteFile(project_dir / "tests" / "integration_test.rs",
            "#[cfg(test)]\nmod tests {\n    #[test]\n    fn it_works() {\n        assert_eq!(2 + 2, 4);\n    }\n}");
    }
    
    return true;
}

bool UniversalGenerator::GenerateWithCI(const std::string& name, LanguageType language,
                                       const std::filesystem::path& output_dir) {
    if (!GenerateProject(name, language, output_dir)) {
        return false;
    }
    
    std::filesystem::path project_dir = output_dir / name;
    
    // Generate GitHub Actions workflow
    std::stringstream workflow;
    workflow << "name: CI\n\n";
    workflow << "on: [push, pull_request]\n\n";
    workflow << "jobs:\n";
    workflow << "  build:\n";
    workflow << "    runs-on: ubuntu-latest\n\n";
    workflow << "    steps:\n";
    workflow << "    - uses: actions/checkout@v3\n\n";
    
    if (language == LanguageType::CPP) {
        workflow << "    - name: Install dependencies\n";
        workflow << "      run: |\n";
        workflow << "        sudo apt-get update\n";
        workflow << "        sudo apt-get install -y cmake g++\n\n";
        workflow << "    - name: Configure CMake\n";
        workflow << "      run: cmake -B build\n\n";
        workflow << "    - name: Build\n";
        workflow << "      run: cmake --build build\n\n";
        workflow << "    - name: Test\n";
        workflow << "      run: cd build && ctest\n";
    } else if (language == LanguageType::PYTHON) {
        workflow << "    - name: Set up Python\n";
        workflow << "      uses: actions/setup-python@v4\n";
        workflow << "      with:\n";
        workflow << "        python-version: '3.10'\n\n";
        workflow << "    - name: Install dependencies\n";
        workflow << "      run: |\n";
        workflow << "        python -m pip install --upgrade pip\n";
        workflow << "        pip install -r requirements.txt\n\n";
        workflow << "    - name: Run tests\n";
        workflow << "      run: python -m pytest tests/\n";
    } else if (language == LanguageType::RUST) {
        workflow << "    - name: Install Rust\n";
        workflow << "      uses: actions-rs/toolchain@v1\n";
        workflow << "      with:\n";
        workflow << "        toolchain: stable\n";
        workflow << "        override: true\n\n";
        workflow << "    - name: Build\n";
        workflow << "      run: cargo build --verbose\n\n";
        workflow << "    - name: Run tests\n";
        workflow << "      run: cargo test --verbose\n";
    }
    
    WriteFile(project_dir / ".github" / "workflows" / "ci.yml", workflow.str());
    return true;
}

bool UniversalGenerator::GenerateWithDocker(const std::string& name, LanguageType language,
                                           const std::filesystem::path& output_dir) {
    if (!GenerateProject(name, language, output_dir)) {
        return false;
    }
    
    std::filesystem::path project_dir = output_dir / name;
    
    // Generate Dockerfile based on language
    std::stringstream dockerfile;
    dockerfile << "# Multi-stage build\n";
    
    if (language == LanguageType::CPP) {
        dockerfile << "FROM gcc:12 as builder\n";
        dockerfile << "WORKDIR /app\n";
        dockerfile << "COPY . .\n";
        dockerfile << "RUN g++ -std=c++20 -O2 -o app src/main.cpp\n\n";
        dockerfile << "FROM alpine:latest\n";
        dockerfile << "WORKDIR /app\n";
        dockerfile << "COPY --from=builder /app/app .\n";
        dockerfile << "CMD [\"./app\"]\n";
    } else if (language == LanguageType::PYTHON) {
        dockerfile << "FROM python:3.10-slim\n";
        dockerfile << "WORKDIR /app\n";
        dockerfile << "COPY requirements.txt .\n";
        dockerfile << "RUN pip install --no-cache-dir -r requirements.txt\n";
        dockerfile << "COPY . .\n";
        dockerfile << "CMD [\"python\", \"src/main.py\"]\n";
    } else if (language == LanguageType::RUST) {
        dockerfile << "FROM rust:1.70 as builder\n";
        dockerfile << "WORKDIR /app\n";
        dockerfile << "COPY Cargo.toml Cargo.lock ./\n";
        dockerfile << "COPY src ./src\n";
        dockerfile << "RUN cargo build --release\n\n";
        dockerfile << "FROM debian:bullseye-slim\n";
        dockerfile << "RUN apt-get update && apt-get install -y ca-certificates && rm -rf /var/lib/apt/lists/*\n";
        dockerfile << "WORKDIR /app\n";
        dockerfile << "COPY --from=builder /app/target/release/" << name << " .\n";
        dockerfile << "CMD [\"./" << name << "\"]\n";
    }
    
    WriteFile(project_dir / "Dockerfile", dockerfile.str());
    
    // Generate docker-compose.yml
    std::stringstream compose;
    compose << "version: '3.8'\n\n";
    compose << "services:\n";
    compose << "  " << name << ":\n";
    compose << "    build: .\n";
    compose << "    container_name: " << name << "\n";
    compose << "    restart: unless-stopped\n";
    
    if (language == LanguageType::JAVASCRIPT || language == LanguageType::REACT) {
        compose << "    ports:\n";
        compose << "      - \"3000:3000\"\n";
        compose << "    environment:\n";
        compose << "      - NODE_ENV=production\n";
    }
    
    WriteFile(project_dir / "docker-compose.yml", compose.str());
    return true;
}
