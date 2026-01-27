// RawrXD Chromatic Wave Demo
// Chameleon/Neon/Chromatic text with wave background effects
// Target: 540Hz @ 3840x2160 (4K UHD)

#include "ui/chromatic_window.h"
#include <windows.h>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
    using namespace RawrXD::UI;
    
    ChromaticWindow chromatic;
    
    // Create at 1920x1080 for testing, can be changed to 3840x2160 for full 4K
    if (!chromatic.create(hInstance, 1920, 1080)) {
        MessageBoxW(nullptr, L"Failed to create chromatic window.\nCheck D3D11 support.", 
            L"RawrXD Chromatic", MB_OK | MB_ICONERROR);
        return 1;
    }
    
    // Set effect parameters
    chromatic.setWaveAmplitude(15.0f);
    chromatic.setWaveFrequency(0.02f);
    chromatic.setWaveSpeed(2.0f);
    chromatic.setChromaticSpeed(1.5f);
    chromatic.setChromaticSaturation(1.0f);
    chromatic.setChromaticBrightness(1.0f);
    chromatic.setText(L"RawrXD IDE - Chromatic Mode");
    
    // Main message loop with high-frequency rendering
    MSG msg = {};
    while (true) {
        // Process all pending messages
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                goto done;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        
        // Render and present as fast as possible for 540Hz target
        chromatic.render();
        chromatic.present();
    }
    
done:
    chromatic.destroy();
    return (int)msg.wParam;
}
