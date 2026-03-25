// Win32IDE_GameEnginePanel.cpp — Game Engine UI Integration (Phase 45)
// Wires the Unity + Unreal game engine integration into the Win32IDE
// command palette, menu bar, and output panel.
// Handles all IDM_GAME_ENGINE_*, IDM_UNITY_*, and IDM_UNREAL_* commands.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
#include "../core/shared_feature_dispatch.h"
#include "../core/unified_command_dispatch.hpp"
#include "Win32IDE.h"
#include <commdlg.h>
#include <filesystem>
#include <iomanip>
#include <sstream>

// SCAFFOLD_043: Game engine panel


namespace fs = std::filesystem;
using namespace RawrXD::GameEngine;

// ============================================================================
// Initialization
// ============================================================================

void Win32IDE::initGameEngines()
{
    if (m_gameEnginesInitialized)
        return;

    m_gameEngineManager = std::make_unique<GameEngineManager>();

    // Set up log callback to route engine messages to IDE output
    m_gameEngineManager->setLogCallback(
        [](const char* message, int severity, GameEngineType engine)
        {
            // This uses the static instance pattern — log goes to IDE output
            std::string prefix;
            switch (engine)
            {
                case GameEngineType::Unity:
                    prefix = "[Unity] ";
                    break;
                case GameEngineType::Unreal:
                    prefix = "[Unreal] ";
                    break;
                default:
                    prefix = "[Engine] ";
                    break;
            }
            // Severity: 0=info, 1=log, 2=warning, 3=error
            (void)severity;  // Could color-code in future
            OutputDebugStringA((prefix + message + "\n").c_str());
        });

    m_gameEngineManager->initialize();

    m_gameEnginesInitialized = true;
    appendToOutput("[GameEngine] Phase 45: Game Engine Integration initialized.\n");

    // Show discovered installations
    auto installations = m_gameEngineManager->getAllInstallations();
    if (installations.empty())
    {
        appendToOutput("[GameEngine] No Unity or Unreal Engine installations detected.\n");
    }
    else
    {
        appendToOutput("[GameEngine] Discovered " + std::to_string(installations.size()) +
                       " engine installation(s):\n");
        for (const auto& inst : installations)
        {
            std::string type = (inst.engine == GameEngineType::Unity) ? "Unity" : "Unreal";
            appendToOutput("  " + type + " " + inst.version + " → " + inst.path + "\n");
        }
    }
}

// ============================================================================
// Command Router
// ============================================================================

void Win32IDE::handleGameEngineCommand(int commandId)
{
    if (!m_gameEnginesInitialized)
    {
        initGameEngines();
    }

    switch (commandId)
    {
        // Unified commands
        case IDM_GAME_ENGINE_DETECT:
            cmdGameEngineDetect();
            break;
        case IDM_GAME_ENGINE_OPEN:
            cmdGameEngineOpen();
            break;
        case IDM_GAME_ENGINE_CLOSE:
            cmdGameEngineClose();
            break;
        case IDM_GAME_ENGINE_STATUS:
            cmdGameEngineStatus();
            break;
        case IDM_GAME_ENGINE_BUILD:
            cmdGameEngineBuild();
            break;
        case IDM_GAME_ENGINE_PLAY:
            cmdGameEnginePlay();
            break;
        case IDM_GAME_ENGINE_STOP:
            cmdGameEngineStop();
            break;
        case IDM_GAME_ENGINE_PAUSE:
            cmdGameEnginePause();
            break;
        case IDM_GAME_ENGINE_COMPILE:
            cmdGameEngineCompile();
            break;
        case IDM_GAME_ENGINE_PROFILER_START:
            cmdGameEngineProfilerStart();
            break;
        case IDM_GAME_ENGINE_PROFILER_STOP:
            cmdGameEngineProfilerStop();
            break;
        case IDM_GAME_ENGINE_PROFILER_SNAP:
            cmdGameEngineProfilerSnapshot();
            break;
        case IDM_GAME_ENGINE_DEBUG_START:
            cmdGameEngineDebugStart();
            break;
        case IDM_GAME_ENGINE_DEBUG_STOP:
            cmdGameEngineDebugStop();
            break;
        case IDM_GAME_ENGINE_BREAKPOINT:
            cmdGameEngineBreakpoint();
            break;
        case IDM_GAME_ENGINE_AI_SUMMARY:
            cmdGameEngineAISummary();
            break;
        case IDM_GAME_ENGINE_AI_SCENE:
            cmdGameEngineAIScene();
            break;
        case IDM_GAME_ENGINE_INSTALLATIONS:
            cmdGameEngineInstallations();
            break;
        case IDM_GAME_ENGINE_HELP:
            cmdGameEngineHelp();
            break;

        // Unity-specific
        case IDM_UNITY_CREATE_SCRIPT:
            cmdUnityCreateScript();
            break;
        case IDM_UNITY_SCENE_LIST:
            cmdUnitySceneList();
            break;
        case IDM_UNITY_ASSET_BROWSER:
            cmdUnityAssetBrowser();
            break;
        case IDM_UNITY_PACKAGE_MANAGER:
            cmdUnityPackageManager();
            break;

        // Unreal-specific
        case IDM_UNREAL_CREATE_CLASS:
            cmdUnrealCreateClass();
            break;
        case IDM_UNREAL_CREATE_MODULE:
            cmdUnrealCreateModule();
            break;
        case IDM_UNREAL_CREATE_PLUGIN:
            cmdUnrealCreatePlugin();
            break;
        case IDM_UNREAL_GEN_PROJECT_FILES:
            cmdUnrealGenProjectFiles();
            break;
        case IDM_UNREAL_LIVE_CODING:
            cmdUnrealLiveCoding();
            break;
        case IDM_UNREAL_COOK_CONTENT:
            cmdUnrealCookContent();
            break;
        case IDM_UNREAL_PACKAGE_PROJECT:
            cmdUnrealPackageProject();
            break;
        case IDM_UNREAL_LEVEL_LIST:
            cmdUnrealLevelList();
            break;
        case IDM_UNREAL_BLUEPRINT_LIST:
            cmdUnrealBlueprintList();
            break;

        // COMMAND_TABLE SSOT IDs (10619–10622) — Unreal/Unity init/attach; palette + unified dispatch parity
        case 10619:
        case 10620:
        case 10621:
        case 10622:
        {
            CommandContext ctx{};
            ctx.rawInput = "";
            ctx.args = "";
            ctx.idePtr = this;
            ctx.cliStatePtr = nullptr;
            ctx.commandId = static_cast<uint32_t>(commandId);
            ctx.isGui = true;
            ctx.isHeadless = false;
            ctx.hwnd = m_hwndMain;
            ctx.emitEvent = nullptr;
            ctx.outputFn = [](const char* text, void* userData)
            {
                auto* ide = static_cast<Win32IDE*>(userData);
                if (ide && text)
                    ide->appendToOutput(std::string(text), "Output", OutputSeverity::Info);
            };
            ctx.outputUserData = this;
            (void)RawrXD::Dispatch::dispatchByGuiId(static_cast<uint32_t>(commandId), ctx);
            break;
        }

        default:
            appendToOutput("[GameEngine] Unknown command: " + std::to_string(commandId) + "\n");
            break;
    }
}

// ============================================================================
// Menu Creation
// ============================================================================

void Win32IDE::createGameEngineMenu(HMENU parentMenu)
{
    HMENU hGameMenu = CreatePopupMenu();

    // Project sub-menu
    HMENU hProjectMenu = CreatePopupMenu();
    AppendMenuA(hProjectMenu, MF_STRING, IDM_GAME_ENGINE_DETECT, "Detect Engine...");
    AppendMenuA(hProjectMenu, MF_STRING, IDM_GAME_ENGINE_OPEN, "Open Project...");
    AppendMenuA(hProjectMenu, MF_STRING, IDM_GAME_ENGINE_CLOSE, "Close Project");
    AppendMenuA(hProjectMenu, MF_STRING, IDM_GAME_ENGINE_STATUS, "Project Status");
    AppendMenuA(hGameMenu, MF_POPUP, (UINT_PTR)hProjectMenu, "Project");

    // Build sub-menu
    HMENU hBuildMenu = CreatePopupMenu();
    AppendMenuA(hBuildMenu, MF_STRING, IDM_GAME_ENGINE_BUILD, "Build Project");
    AppendMenuA(hBuildMenu, MF_STRING, IDM_GAME_ENGINE_COMPILE, "Compile Scripts/C++");
    AppendMenuA(hBuildMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hBuildMenu, MF_STRING, IDM_UNREAL_GEN_PROJECT_FILES, "Generate Project Files (UE)");
    AppendMenuA(hBuildMenu, MF_STRING, IDM_UNREAL_LIVE_CODING, "Live Coding Toggle (UE)");
    AppendMenuA(hBuildMenu, MF_STRING, IDM_UNREAL_COOK_CONTENT, "Cook Content (UE)");
    AppendMenuA(hBuildMenu, MF_STRING, IDM_UNREAL_PACKAGE_PROJECT, "Package Project (UE)");
    AppendMenuA(hGameMenu, MF_POPUP, (UINT_PTR)hBuildMenu, "Build");

    // Play sub-menu
    HMENU hPlayMenu = CreatePopupMenu();
    AppendMenuA(hPlayMenu, MF_STRING, IDM_GAME_ENGINE_PLAY, "Play / Enter PIE");
    AppendMenuA(hPlayMenu, MF_STRING, IDM_GAME_ENGINE_STOP, "Stop Play Mode");
    AppendMenuA(hPlayMenu, MF_STRING, IDM_GAME_ENGINE_PAUSE, "Pause/Resume");
    AppendMenuA(hGameMenu, MF_POPUP, (UINT_PTR)hPlayMenu, "Play");

    // Debug sub-menu
    HMENU hDebugMenu = CreatePopupMenu();
    AppendMenuA(hDebugMenu, MF_STRING, IDM_GAME_ENGINE_DEBUG_START, "Start Debug");
    AppendMenuA(hDebugMenu, MF_STRING, IDM_GAME_ENGINE_DEBUG_STOP, "Stop Debug");
    AppendMenuA(hDebugMenu, MF_STRING, IDM_GAME_ENGINE_BREAKPOINT, "Set Breakpoint...");
    AppendMenuA(hGameMenu, MF_POPUP, (UINT_PTR)hDebugMenu, "Debug");

    // Profiler sub-menu
    HMENU hProfilerMenu = CreatePopupMenu();
    AppendMenuA(hProfilerMenu, MF_STRING, IDM_GAME_ENGINE_PROFILER_START, "Start Profiler");
    AppendMenuA(hProfilerMenu, MF_STRING, IDM_GAME_ENGINE_PROFILER_STOP, "Stop Profiler");
    AppendMenuA(hProfilerMenu, MF_STRING, IDM_GAME_ENGINE_PROFILER_SNAP, "Capture Snapshot");
    AppendMenuA(hGameMenu, MF_POPUP, (UINT_PTR)hProfilerMenu, "Profiler");

    // Unity sub-menu
    HMENU hUnityMenu = CreatePopupMenu();
    AppendMenuA(hUnityMenu, MF_STRING, IDM_UNITY_CREATE_SCRIPT, "New C# Script...");
    AppendMenuA(hUnityMenu, MF_STRING, IDM_UNITY_SCENE_LIST, "Scene List");
    AppendMenuA(hUnityMenu, MF_STRING, IDM_UNITY_ASSET_BROWSER, "Asset Browser");
    AppendMenuA(hUnityMenu, MF_STRING, IDM_UNITY_PACKAGE_MANAGER, "Package Manager");
    AppendMenuA(hGameMenu, MF_POPUP, (UINT_PTR)hUnityMenu, "Unity");

    // Unreal sub-menu
    HMENU hUnrealMenu = CreatePopupMenu();
    AppendMenuA(hUnrealMenu, MF_STRING, IDM_UNREAL_CREATE_CLASS, "New C++ Class...");
    AppendMenuA(hUnrealMenu, MF_STRING, IDM_UNREAL_CREATE_MODULE, "New Module...");
    AppendMenuA(hUnrealMenu, MF_STRING, IDM_UNREAL_CREATE_PLUGIN, "New Plugin...");
    AppendMenuA(hUnrealMenu, MF_STRING, IDM_UNREAL_LEVEL_LIST, "Level List");
    AppendMenuA(hUnrealMenu, MF_STRING, IDM_UNREAL_BLUEPRINT_LIST, "Blueprint List");
    AppendMenuA(hGameMenu, MF_POPUP, (UINT_PTR)hUnrealMenu, "Unreal");

    // AI Integration sub-menu
    HMENU hAIMenu = CreatePopupMenu();
    AppendMenuA(hAIMenu, MF_STRING, IDM_GAME_ENGINE_AI_SUMMARY, "AI Project Summary");
    AppendMenuA(hAIMenu, MF_STRING, IDM_GAME_ENGINE_AI_SCENE, "AI Scene Description");
    AppendMenuA(hGameMenu, MF_POPUP, (UINT_PTR)hAIMenu, "AI Integration");

    // Top-level items
    AppendMenuA(hGameMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hGameMenu, MF_STRING, IDM_GAME_ENGINE_INSTALLATIONS, "Engine Installations");
    AppendMenuA(hGameMenu, MF_STRING, IDM_GAME_ENGINE_HELP, "Help");

    AppendMenuA(parentMenu, MF_POPUP, (UINT_PTR)hGameMenu, "Game Engines");
}

// ============================================================================
// Status Dialog
// ============================================================================

void Win32IDE::showGameEngineStatusDialog()
{
    if (!m_gameEngineManager)
        return;

    std::string status = m_gameEngineManager->getStatusString();

    MessageBoxA(m_hwndMain, status.c_str(), "Game Engine Status — RawrXD IDE", MB_OK | MB_ICONINFORMATION);
}

// ============================================================================
// Unified Command Handlers
// ============================================================================

void Win32IDE::cmdGameEngineDetect()
{
    // Use currently opened file's directory or ask for path
    char path[MAX_PATH] = {};
    BROWSEINFOA bi = {};
    bi.hwndOwner = m_hwndMain;
    bi.pszDisplayName = path;
    bi.lpszTitle = "Select Game Engine Project Folder";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderA(&bi);
    if (!pidl)
        return;
    SHGetPathFromIDListA(pidl, path);
    CoTaskMemFree(pidl);

    auto result = m_gameEngineManager->detectProject(path);
    if (result.isValid)
    {
        std::string msg =
            "Detected: " + result.detail + "\nVersion: " + result.version + "\nPath: " + result.projectPath;
        appendToOutput("[GameEngine] " + msg + "\n");

        // Ask if user wants to open
        int answer = MessageBoxA(m_hwndMain, (msg + "\n\nOpen this project?").c_str(), "Engine Detected",
                                 MB_YESNO | MB_ICONQUESTION);
        if (answer == IDYES)
        {
            if (m_gameEngineManager->openProject(path))
            {
                appendToOutput("[GameEngine] Project opened successfully.\n");
            }
            else
            {
                appendToOutput("[GameEngine] Failed to open project.\n");
            }
        }
    }
    else
    {
        appendToOutput("[GameEngine] No game engine project detected at: " + std::string(path) + "\n");
        MessageBoxA(m_hwndMain, "No Unity or Unreal Engine project found at this location.", "Not Found",
                    MB_OK | MB_ICONWARNING);
    }
}

void Win32IDE::cmdGameEngineOpen()
{
    char path[MAX_PATH] = {};
    BROWSEINFOA bi = {};
    bi.hwndOwner = m_hwndMain;
    bi.pszDisplayName = path;
    bi.lpszTitle = "Select Game Engine Project Folder";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderA(&bi);
    if (!pidl)
        return;
    SHGetPathFromIDListA(pidl, path);
    CoTaskMemFree(pidl);

    if (m_gameEngineManager->openProject(path))
    {
        std::string engineName;
        switch (m_gameEngineManager->getActiveEngine())
        {
            case GameEngineType::Unity:
                engineName = "Unity";
                break;
            case GameEngineType::Unreal:
                engineName = "Unreal";
                break;
            default:
                engineName = "Unknown";
                break;
        }
        appendToOutput("[GameEngine] Opened " + engineName +
                       " project: " + m_gameEngineManager->getActiveProjectName() + "\n");
    }
    else
    {
        appendToOutput("[GameEngine] Failed to open project at: " + std::string(path) + "\n");
    }
}

void Win32IDE::cmdGameEngineClose()
{
    if (!m_gameEngineManager->isProjectOpen())
    {
        appendToOutput("[GameEngine] No project is currently open.\n");
        return;
    }
    std::string name = m_gameEngineManager->getActiveProjectName();
    m_gameEngineManager->closeProject();
    appendToOutput("[GameEngine] Closed project: " + name + "\n");
}

void Win32IDE::cmdGameEngineStatus()
{
    showGameEngineStatusDialog();
}

void Win32IDE::cmdGameEngineBuild()
{
    if (!m_gameEngineManager->isProjectOpen())
    {
        appendToOutput("[GameEngine] No project open — nothing to build.\n");
        return;
    }

    appendToOutput("[GameEngine] Starting build...\n");

    GameBuildRequest request;
    request.platform = "Win64";
    request.configuration = "Development";

    // Run async to not block UI
    std::thread(
        [this, request]()
        {
            auto result = m_gameEngineManager->buildProject(request);
            // Post result back to UI thread
            std::string msg = result.success
                                  ? "[GameEngine] Build SUCCEEDED in " + std::to_string(result.buildTimeSec) + "s\n"
                                  : "[GameEngine] Build FAILED (" + std::to_string(result.errorCount) + " errors)\n";

            PostMessageA(m_hwndMain, WM_APP + 200, 0, 0);  // signal UI refresh
            appendToOutput(msg);

            if (result.success && !result.outputPath.empty())
            {
                // Integrate with Reverse Engineering: set built binary for codex analysis on main thread
                std::string* pathCopy = new std::string(result.outputPath);
                PostMessageA(m_hwndMain, WM_APP + 202, 0, reinterpret_cast<LPARAM>(pathCopy));
            }

            if (!result.errors.empty())
            {
                appendToOutput("[GameEngine] Errors:\n");
                for (const auto& err : result.errors)
                {
                    appendToOutput("  " + err + "\n");
                }
            }
        })
        .detach();
}

void Win32IDE::cmdGameEnginePlay()
{
    if (!m_gameEngineManager->isProjectOpen())
    {
        appendToOutput("[GameEngine] No project open.\n");
        return;
    }
    if (m_gameEngineManager->enterPlayMode())
    {
        appendToOutput("[GameEngine] Entered play mode.\n");
    }
    else
    {
        appendToOutput("[GameEngine] Failed to enter play mode.\n");
    }
}

void Win32IDE::cmdGameEngineStop()
{
    if (!m_gameEngineManager->isInPlayMode())
    {
        appendToOutput("[GameEngine] Not in play mode.\n");
        return;
    }
    m_gameEngineManager->exitPlayMode();
    appendToOutput("[GameEngine] Exited play mode.\n");
}

void Win32IDE::cmdGameEnginePause()
{
    if (!m_gameEngineManager->isInPlayMode())
    {
        appendToOutput("[GameEngine] Not in play mode.\n");
        return;
    }
    // Toggle pause
    if (m_gameEngineManager->getActiveEngine() == GameEngineType::Unity)
    {
        auto* unity = m_gameEngineManager->getUnity();
        if (unity && unity->isPaused())
        {
            m_gameEngineManager->resumePlayMode();
            appendToOutput("[GameEngine] Resumed play mode.\n");
        }
        else
        {
            m_gameEngineManager->pausePlayMode();
            appendToOutput("[GameEngine] Paused play mode.\n");
        }
    }
    else if (m_gameEngineManager->getActiveEngine() == GameEngineType::Unreal)
    {
        auto* unreal = m_gameEngineManager->getUnreal();
        if (unreal && unreal->isPIEPaused())
        {
            m_gameEngineManager->resumePlayMode();
            appendToOutput("[GameEngine] Resumed PIE.\n");
        }
        else
        {
            m_gameEngineManager->pausePlayMode();
            appendToOutput("[GameEngine] Paused PIE.\n");
        }
    }
}

void Win32IDE::cmdGameEngineCompile()
{
    if (!m_gameEngineManager->isProjectOpen())
    {
        appendToOutput("[GameEngine] No project open.\n");
        return;
    }

    appendToOutput("[GameEngine] Compiling...\n");

    std::thread(
        [this]()
        {
            bool ok = m_gameEngineManager->compile();
            if (ok)
            {
                appendToOutput("[GameEngine] Compilation succeeded.\n");
            }
            else
            {
                appendToOutput("[GameEngine] Compilation failed.\n");
                auto errors = m_gameEngineManager->getCompileErrorStrings();
                for (const auto& err : errors)
                {
                    appendToOutput("  " + err + "\n");
                }
            }
        })
        .detach();
}

void Win32IDE::cmdGameEngineProfilerStart()
{
    if (m_gameEngineManager->startProfiler())
    {
        appendToOutput("[GameEngine] Profiler started.\n");
    }
    else
    {
        appendToOutput("[GameEngine] Failed to start profiler.\n");
    }
}

void Win32IDE::cmdGameEngineProfilerStop()
{
    if (m_gameEngineManager->stopProfiler())
    {
        appendToOutput("[GameEngine] Profiler stopped.\n");
    }
}

void Win32IDE::cmdGameEngineProfilerSnapshot()
{
    if (!m_gameEngineManager->isProfilerRunning())
    {
        appendToOutput("[GameEngine] Profiler is not running.\n");
        return;
    }

    auto snap = m_gameEngineManager->getProfilerSnapshot();
    std::ostringstream ss;
    ss << "[GameEngine] Profiler Snapshot (Frame " << snap.frameNumber << "):\n";
    ss << std::fixed << std::setprecision(2);
    ss << "  FPS:         " << snap.fps << "\n";
    ss << "  Frame Time:  " << snap.frameTimeMs << " ms\n";
    ss << "  CPU Time:    " << snap.cpuTimeMs << " ms\n";
    ss << "  GPU Time:    " << snap.gpuTimeMs << " ms\n";
    ss << "  Script:      " << snap.scriptTimeMs << " ms\n";
    ss << "  Physics:     " << snap.physicsTimeMs << " ms\n";
    ss << "  Render:      " << snap.renderTimeMs << " ms\n";
    ss << "  Draw Calls:  " << snap.drawCalls << "\n";
    ss << "  Triangles:   " << snap.triangles << "\n";
    ss << "  Memory:      " << snap.totalMemoryMB << " MB\n";
    ss << "  Textures:    " << snap.textureMemoryMB << " MB\n";
    ss << "  Meshes:      " << snap.meshMemoryMB << " MB\n";
    appendToOutput(ss.str());
}

void Win32IDE::cmdGameEngineDebugStart()
{
    if (m_gameEngineManager->startDebugSession())
    {
        appendToOutput("[GameEngine] Debug session started.\n");
    }
    else
    {
        appendToOutput("[GameEngine] Failed to start debug session.\n");
    }
}

void Win32IDE::cmdGameEngineDebugStop()
{
    if (m_gameEngineManager->stopDebugSession())
    {
        appendToOutput("[GameEngine] Debug session stopped.\n");
    }
}

void Win32IDE::cmdGameEngineBreakpoint()
{
    // Simple dialog to set breakpoint
    char buffer[512] = {};
    // For now, use a simple input via output prompt
    appendToOutput("[GameEngine] Set breakpoint: Use !engine bp <file> <line>\n");
}

void Win32IDE::cmdGameEngineAISummary()
{
    std::string summary = m_gameEngineManager->generateAIProjectSummary();
    appendToOutput("[GameEngine] AI Project Summary:\n" + summary + "\n");
}

void Win32IDE::cmdGameEngineAIScene()
{
    std::string desc = m_gameEngineManager->generateAISceneDescription();
    appendToOutput("[GameEngine] AI Scene Description:\n" + desc + "\n");
}

void Win32IDE::cmdGameEngineInstallations()
{
    auto installations = m_gameEngineManager->getAllInstallations();
    if (installations.empty())
    {
        appendToOutput("[GameEngine] No engine installations found.\n");
        return;
    }
    appendToOutput("[GameEngine] Engine Installations:\n");
    for (const auto& inst : installations)
    {
        std::string type = (inst.engine == GameEngineType::Unity) ? "Unity" : "Unreal";
        std::string def = inst.isDefault ? " (default)" : "";
        appendToOutput("  [" + type + "] " + inst.version + def + " → " + inst.path + "\n");
    }
}

void Win32IDE::cmdGameEngineHelp()
{
    std::string help = m_gameEngineManager->getHelpText();
    appendToOutput(help);
}

// ============================================================================
// Unity-Specific Command Handlers
// ============================================================================

void Win32IDE::cmdUnityCreateScript()
{
    if (m_gameEngineManager->getActiveEngine() != GameEngineType::Unity)
    {
        appendToOutput("[Unity] No Unity project is open.\n");
        return;
    }

    auto* unity = m_gameEngineManager->getUnity();
    if (!unity)
        return;

    // Prompt for script name
    char scriptName[256] = "NewScript";
    // Simple input dialog — in production would use a proper dialog
    appendToOutput("[Unity] Creating C# script. Enter name in command input.\n");

    // Generate a default MonoBehaviour
    std::string code = unity->generateScriptTemplate("MonoBehaviour", scriptName);
    std::string path = unity->getProjectInfo().assetsPath + "/Scripts/" + scriptName + ".cs";

    fs::create_directories(fs::path(path).parent_path());
    std::ofstream file(path);
    if (file.is_open())
    {
        file << code;
        file.close();
        appendToOutput("[Unity] Created script: " + path + "\n");
    }
    else
    {
        appendToOutput("[Unity] Failed to create script at: " + path + "\n");
    }
}

void Win32IDE::cmdUnitySceneList()
{
    if (m_gameEngineManager->getActiveEngine() != GameEngineType::Unity)
    {
        appendToOutput("[Unity] No Unity project is open.\n");
        return;
    }

    auto* unity = m_gameEngineManager->getUnity();
    if (!unity)
        return;

    auto scenes = unity->getScenes();
    if (scenes.empty())
    {
        appendToOutput("[Unity] No scenes found.\n");
        return;
    }

    appendToOutput("[Unity] Scenes (" + std::to_string(scenes.size()) + "):\n");
    for (const auto& scene : scenes)
    {
        std::string status;
        if (scene.isLoaded)
            status = " [loaded]";
        if (scene.isDirty)
            status += " [modified]";
        appendToOutput("  " + scene.sceneName + " — " + scene.scenePath + status + "\n");
    }
}

void Win32IDE::cmdUnityAssetBrowser()
{
    if (m_gameEngineManager->getActiveEngine() != GameEngineType::Unity)
    {
        appendToOutput("[Unity] No Unity project is open.\n");
        return;
    }

    auto* unity = m_gameEngineManager->getUnity();
    if (!unity)
        return;

    auto assets = unity->getAssets();
    appendToOutput("[Unity] Assets (" + std::to_string(assets.size()) + "):\n");

    // Group by type
    std::map<int, int> typeCounts;
    for (const auto& asset : assets)
    {
        typeCounts[static_cast<int>(asset.type)]++;
    }

    for (const auto& [type, count] : typeCounts)
    {
        std::string typeName;
        switch (static_cast<UnityAssetType>(type))
        {
            case UnityAssetType::Scene:
                typeName = "Scenes";
                break;
            case UnityAssetType::Prefab:
                typeName = "Prefabs";
                break;
            case UnityAssetType::Script:
                typeName = "Scripts";
                break;
            case UnityAssetType::Shader:
                typeName = "Shaders";
                break;
            case UnityAssetType::Material:
                typeName = "Materials";
                break;
            case UnityAssetType::Texture:
                typeName = "Textures";
                break;
            case UnityAssetType::Model:
                typeName = "Models";
                break;
            case UnityAssetType::Audio:
                typeName = "Audio";
                break;
            case UnityAssetType::Animation:
                typeName = "Animations";
                break;
            case UnityAssetType::AnimController:
                typeName = "Animators";
                break;
            case UnityAssetType::Font:
                typeName = "Fonts";
                break;
            case UnityAssetType::ScriptableObject:
                typeName = "ScriptableObjects";
                break;
            default:
                typeName = "Other";
                break;
        }
        appendToOutput("  " + typeName + ": " + std::to_string(count) + "\n");
    }
}

void Win32IDE::cmdUnityPackageManager()
{
    if (m_gameEngineManager->getActiveEngine() != GameEngineType::Unity)
    {
        appendToOutput("[Unity] No Unity project is open.\n");
        return;
    }

    auto* unity = m_gameEngineManager->getUnity();
    if (!unity)
        return;

    auto packages = unity->getInstalledPackages();
    if (packages.empty())
    {
        appendToOutput("[Unity] No packages found (or manifest.json not readable).\n");
        return;
    }

    appendToOutput("[Unity] Installed Packages (" + std::to_string(packages.size()) + "):\n");
    for (const auto& pkg : packages)
    {
        appendToOutput("  " + pkg + "\n");
    }
}

// ============================================================================
// Unreal-Specific Command Handlers
// ============================================================================

void Win32IDE::cmdUnrealCreateClass()
{
    if (m_gameEngineManager->getActiveEngine() != GameEngineType::Unreal)
    {
        appendToOutput("[Unreal] No Unreal project is open.\n");
        return;
    }

    auto* unreal = m_gameEngineManager->getUnreal();
    if (!unreal)
        return;

    // Default: create an AActor subclass
    std::string className = "MyNewActor";
    std::string parentClass = "AActor";

    if (unreal->createCppClass(className, parentClass))
    {
        appendToOutput("[Unreal] Created class: " + className + " : " + parentClass + "\n");
        appendToOutput("[Unreal] Run 'Generate Project Files' to update the .sln\n");
    }
    else
    {
        appendToOutput("[Unreal] Failed to create class.\n");
    }
}

void Win32IDE::cmdUnrealCreateModule()
{
    if (m_gameEngineManager->getActiveEngine() != GameEngineType::Unreal)
    {
        appendToOutput("[Unreal] No Unreal project is open.\n");
        return;
    }

    auto* unreal = m_gameEngineManager->getUnreal();
    if (!unreal)
        return;

    std::string moduleName = "MyNewModule";
    if (unreal->createModule(moduleName))
    {
        appendToOutput("[Unreal] Created module: " + moduleName + "\n");
    }
    else
    {
        appendToOutput("[Unreal] Failed to create module.\n");
    }
}

void Win32IDE::cmdUnrealCreatePlugin()
{
    if (m_gameEngineManager->getActiveEngine() != GameEngineType::Unreal)
    {
        appendToOutput("[Unreal] No Unreal project is open.\n");
        return;
    }

    auto* unreal = m_gameEngineManager->getUnreal();
    if (!unreal)
        return;

    std::string pluginName = "MyPlugin";
    if (unreal->createPlugin(pluginName))
    {
        appendToOutput("[Unreal] Created plugin: " + pluginName + "\n");
    }
    else
    {
        appendToOutput("[Unreal] Failed to create plugin.\n");
    }
}

void Win32IDE::cmdUnrealGenProjectFiles()
{
    if (m_gameEngineManager->getActiveEngine() != GameEngineType::Unreal)
    {
        appendToOutput("[Unreal] No Unreal project is open.\n");
        return;
    }

    auto* unreal = m_gameEngineManager->getUnreal();
    if (!unreal)
        return;

    appendToOutput("[Unreal] Generating project files (.sln)...\n");

    std::thread(
        [this, unreal]()
        {
            if (unreal->generateProjectFiles())
            {
                appendToOutput("[Unreal] Project files generated successfully.\n");
            }
            else
            {
                appendToOutput("[Unreal] Failed to generate project files.\n");
            }
        })
        .detach();
}

void Win32IDE::cmdUnrealLiveCoding()
{
    if (m_gameEngineManager->getActiveEngine() != GameEngineType::Unreal)
    {
        appendToOutput("[Unreal] No Unreal project is open.\n");
        return;
    }

    auto* unreal = m_gameEngineManager->getUnreal();
    if (!unreal)
        return;

    if (unreal->isLiveCodingEnabled())
    {
        unreal->disableLiveCoding();
        appendToOutput("[Unreal] Live Coding DISABLED.\n");
    }
    else
    {
        unreal->enableLiveCoding();
        appendToOutput("[Unreal] Live Coding ENABLED.\n");
    }
}

void Win32IDE::cmdUnrealCookContent()
{
    if (m_gameEngineManager->getActiveEngine() != GameEngineType::Unreal)
    {
        appendToOutput("[Unreal] No Unreal project is open.\n");
        return;
    }

    auto* unreal = m_gameEngineManager->getUnreal();
    if (!unreal)
        return;

    appendToOutput("[Unreal] Cooking content for Win64...\n");

    std::thread(
        [this, unreal]()
        {
            if (unreal->cookContent(UnrealBuildTarget::Win64))
            {
                appendToOutput("[Unreal] Content cooking completed.\n");
            }
            else
            {
                appendToOutput("[Unreal] Content cooking failed.\n");
            }
        })
        .detach();
}

void Win32IDE::cmdUnrealPackageProject()
{
    if (m_gameEngineManager->getActiveEngine() != GameEngineType::Unreal)
    {
        appendToOutput("[Unreal] No Unreal project is open.\n");
        return;
    }

    auto* unreal = m_gameEngineManager->getUnreal();
    if (!unreal)
        return;

    // Ask for output directory
    char path[MAX_PATH] = {};
    BROWSEINFOA bi = {};
    bi.hwndOwner = m_hwndMain;
    bi.pszDisplayName = path;
    bi.lpszTitle = "Select Package Output Directory";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderA(&bi);
    if (!pidl)
        return;
    SHGetPathFromIDListA(pidl, path);
    CoTaskMemFree(pidl);

    appendToOutput("[Unreal] Packaging project to: " + std::string(path) + "\n");

    std::string outDir = path;
    std::thread(
        [this, unreal, outDir]()
        {
            if (unreal->packageProject(UnrealBuildTarget::Win64, outDir))
            {
                appendToOutput("[Unreal] Packaging completed: " + outDir + "\n");
            }
            else
            {
                appendToOutput("[Unreal] Packaging failed.\n");
            }
        })
        .detach();
}

void Win32IDE::cmdUnrealLevelList()
{
    if (m_gameEngineManager->getActiveEngine() != GameEngineType::Unreal)
    {
        appendToOutput("[Unreal] No Unreal project is open.\n");
        return;
    }

    auto* unreal = m_gameEngineManager->getUnreal();
    if (!unreal)
        return;

    auto levels = unreal->getLevels();
    if (levels.empty())
    {
        appendToOutput("[Unreal] No levels found in project.\n");
        return;
    }

    appendToOutput("[Unreal] Levels (" + std::to_string(levels.size()) + "):\n");
    for (const auto& level : levels)
    {
        std::string status = level.isLoaded ? " [loaded]" : "";
        appendToOutput("  " + level.levelName + " — " + level.levelPath + status + " (" +
                       std::to_string(level.actorCount) + " actors)\n");
    }
}

void Win32IDE::cmdUnrealBlueprintList()
{
    if (m_gameEngineManager->getActiveEngine() != GameEngineType::Unreal)
    {
        appendToOutput("[Unreal] No Unreal project is open.\n");
        return;
    }

    auto* unreal = m_gameEngineManager->getUnreal();
    if (!unreal)
        return;

    auto blueprints = unreal->getBlueprints();
    if (blueprints.empty())
    {
        appendToOutput("[Unreal] No blueprints found in project.\n");
        return;
    }

    appendToOutput("[Unreal] Blueprints (" + std::to_string(blueprints.size()) + "):\n");
    for (const auto& bp : blueprints)
    {
        appendToOutput("  " + bp + "\n");
    }
}
