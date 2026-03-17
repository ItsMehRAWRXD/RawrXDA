#pragma once

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
public:
    InterpretabilityPanel();
    ~InterpretabilityPanel();

    /** Win32: pass HWND as void* for parent window */
    void setParent(void* parent);
    void initialize();
    void show();
    void hide();

private:
    void* m_parent = nullptr;
    void* m_hwnd   = nullptr;
};
