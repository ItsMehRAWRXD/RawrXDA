# RawrXD Win32 IDE — Feature Matrix

Feature, engine, agentic tool, and autonomy entry points in the Win32 GUI.

## Launch
- Run `Launch_RawrXD_IDE.bat` from repo root (or exe with CWD = repo root).
- Window at (50,50), 1600x1000; Camellia encryptWorkspace runs in background thread.

## Agent menu (4100+)
- Start Agent Loop 4100, Bounded Agent 4120, Execute Command 4101, Configure Model 4102, View Tools/Status 4103/4104, Stop 4105, Set Cycle Count 4122.
- AI Options (Max, Deep Think, No Refusal), Context Window 4K-1M, Titan 4224, 800B Status 4225, Multi-Agent 4218/4219/4223.
- ENGINE_UNLOCK_800B 4220, ENGINE_LOAD_800B 4221, IDM_TITAN_TOGGLE 4230 handled in handleAgentCommand.

## Autonomy (Agent > Autonomy)
- Toggle 4150, Start 4151, Stop 4152, Set Goal 4153, Status 4154, Memory 4155.

## SubAgent (Agent > Sub-Agent)
- Chain 4111, Swarm 4112, Todo List 4113, Todo Clear 4114, Status 4115.

## Agent Memory (Agent > Agent Memory)
- View 4107, Clear 4108, Export 4109.

## Model / streaming
- File > Load Model (GGUF) 1030 -> openModel.
- View > Use Streaming Loader 2026 -> m_useStreamingLoader toggle.
- View > Enable Vulkan Renderer 2027 -> m_useVulkanRenderer toggle.

## View 2020-2029
- 2020 Minimap, 2021 Output Tabs, 2022 Module Browser, 2023 Theme Editor, 2024 Floating Panel, 2025 Output Panel, 2026 Streaming Loader, 2027 Vulkan, 2028 Sidebar, 2029 Terminal. Routed to handleViewCommand.

## Routing
- 1000-1999 handleFileCommand, 2000-2019 handleEditCommand, 2020-2999+3000-3999 handleViewCommand, 4100-4399 handleAgentCommand.

All features above are directly useable via Win32 GUI menus.
