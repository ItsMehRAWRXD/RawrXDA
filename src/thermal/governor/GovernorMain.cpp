#include <windows.h>
#include <csignal>
#include "ThermalGovernor.h"

extern "C" void AgentBridgeEntry();

volatile bool keepRunning = true;

void SignalHandler(int) {
    keepRunning = false;
}

int main(int argc, char* argv[]) {
    
    std::signal(SIGINT, SignalHandler);
    
    ThermalGovernor governor;
    
    // Parse args? For now defaults.
    // governor.SetGlobalConfig(75, 65, 85);

    if (!governor.Initialize()) {
        
    } else {
        
    }

    // Spawn the MASM Agent Bridge Thread
    HANDLE hBridge = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)AgentBridgeEntry, NULL, 0, NULL);
    if (hBridge) {
        
        CloseHandle(hBridge); // We don't join, it runs forever
    } else {
        
    }

    while (keepRunning) {
        governor.Update();
        Sleep(1000); // 1Hz Governor Loop
    }


    governor.Shutdown();
    return 0;
}
