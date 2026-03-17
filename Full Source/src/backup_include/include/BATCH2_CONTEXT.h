// BATCH2_CONTEXT.h — Batch 2 Bridge Files Context
#pragma once
#include <windows.h>
#include <memoryapi.h>  // MMF support

// Bridge to your existing Batch 1
#include "../src/win32app/Win32IDE_BeaconWiring.h"
#include "../src/win32app/Win32IDE_HotpatchWiring.h"

// MASM64 Cathedral Bridge (extern from your .asm files)
extern "C" {
    __int64 RawrXD_BeaconStage(int stageId, void* context);
    __int64 RawrXD_CircularArchHandshake(void* mmfBase);
    __int64 RawrXD_SidebarWndProc(HWND hWnd, UINT msg, WPARAM w, LPARAM l);
}

// CommandResult from Batch 1
struct CommandResult {
    bool success;
    char message[256];
    int code;
};

// Beacon stages (match your MASM enum)
enum BeaconStage {
    STAGE_INIT = 0,
    STAGE_MASM_LOAD = 1,
    STAGE_HOTPATCH = 2,
    STAGE_MEMORY_MAP = 3,
    STAGE_WINDOW_CREATE = 4,
    STAGE_SIDEBAR_INIT = 5,
    STAGE_BEACON_SYNC = 6,
    STAGE_COMMAND_WIRE = 7,
    STAGE_UI_RENDER = 8,
    STAGE_FINAL_HANDSHAKE = 9,
    STAGE_COMPLETE = 10,
    STAGE_ERROR_RECOVERY = 11
};

// MMF Beacon State Structure
struct BeaconState {
    BeaconStage currentStage;
    DWORD processId;
    HANDLE hMmf;
    void* mmfBase;
    DWORD lastHeartbeat;
    char statusMessage[256];
    bool isAlive;
};

// Timeout constants
const DWORD BEACON_TIMEOUT_MS = 30000; // 30 seconds
const DWORD HEARTBEAT_INTERVAL_MS = 1000; // 1 second
