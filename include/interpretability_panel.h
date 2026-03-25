#pragma once

<<<<<<< HEAD
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
using HWND = void*;
using BOOL = int;
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#endif

extern "C" {
HWND InterpretabilityPanel_Create(HWND parent);
void InterpretabilityPanel_SetVisualizationType(int type);
void InterpretabilityPanel_SetLayerRange(int minLayer, int maxLayer);
void InterpretabilityPanel_UpdateStats(float tps, float latency, int total, int prompt, int gen, float memMB, int activeLayer, float entropy);
void InterpretabilityPanel_Clear();
void InterpretabilityPanel_Show(BOOL show);
void InterpretabilityPanel_Resize(int x, int y, int w, int h);
}

class InterpretabilityPanel {
=======
#include <string>
#include <vector>
#include <map>
#include <cstdint>

/**
 * @class InterpretabilityPanel
 * @brief Visualize and interpret model behavior (Win32/Qt-free)
 *
 * Win32 build: parent is HWND (passed as void*). Panel is created on show().
 */
class InterpretabilityPanel
{
>>>>>>> origin/main
public:
    InterpretabilityPanel();
    ~InterpretabilityPanel();

    void setParent(void* parent);
    void initialize();
    void show();
    void hide();

    void setVisualizationType(int type) { InterpretabilityPanel_SetVisualizationType(type); }
    void setLayerRange(int minLayer, int maxLayer) { InterpretabilityPanel_SetLayerRange(minLayer, maxLayer); }
    void updateStats(float tps, float latency, int total, int prompt, int gen, float memMB, int activeLayer, float entropy) {
        InterpretabilityPanel_UpdateStats(tps, latency, total, prompt, gen, memMB, activeLayer, entropy);
    }
    void clear() { InterpretabilityPanel_Clear(); }
    void resize(int x, int y, int w, int h) { InterpretabilityPanel_Resize(x, y, w, h); }

private:
    void* m_parent = nullptr;
<<<<<<< HEAD
    HWND m_hwnd = nullptr;
=======
    void* m_hwnd   = nullptr;
>>>>>>> origin/main
};
