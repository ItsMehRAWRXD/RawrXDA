#include "Win32IDE_AgenticIntegration.h"
#include "../win32ide/ExecModeToolbar.h"
#include "../win32ide/GhostOverlay.h"
#include "../agentic/ExecPipeline.h"

namespace
{
Win32IDE_AgenticIntegration* g_agenticIntegration = nullptr;
}

Win32IDE_AgenticIntegration* GetAgenticIntegration()
{
    return g_agenticIntegration;
}

void SetAgenticIntegration(Win32IDE_AgenticIntegration* integration)
{
    g_agenticIntegration = integration;
}

Win32IDE_AgenticIntegration::Win32IDE_AgenticIntegration(Win32IDE* ide)
    : m_ide(ide)
{
    SetAgenticIntegration(this);
}

Win32IDE_AgenticIntegration::~Win32IDE_AgenticIntegration()
{
    Shutdown();
    if (g_agenticIntegration == this)
    {
        g_agenticIntegration = nullptr;
    }
}

bool Win32IDE_AgenticIntegration::Initialize()
{
    if (m_initialized)
    {
        return true;
    }
    m_initialized = true;
    return true;
}

void Win32IDE_AgenticIntegration::Shutdown()
{
    m_initialized = false;
}

LRESULT Win32IDE_AgenticIntegration::HandleMessage(HWND, UINT, WPARAM, LPARAM)
{
    return 0;
}

bool Win32IDE_AgenticIntegration::HandleAccelerator(HWND, WPARAM)
{
    return false;
}

void Win32IDE_AgenticIntegration::ExecuteAgenticCommand(const std::wstring&, const std::wstring&)
{
}

void Win32IDE_AgenticIntegration::ShowGhostSuggestion(const std::wstring&, bool)
{
}

void Win32IDE_AgenticIntegration::ClearGhostSuggestion()
{
}

std::wstring Win32IDE_AgenticIntegration::GetExecutionModeLabel() const
{
    return L"Agentic: Off";
}

std::wstring Win32IDE_AgenticIntegration::GetExecutionModeDescription() const
{
    return L"Agentic integration stub active";
}

void Win32IDE_AgenticIntegration::UpdateStatusBar()
{
}

void Win32IDE_AgenticIntegration::DispatchAgenticCommand(const std::wstring&, const std::wstring&)
{
}

bool Win32IDE_AgenticIntegration::IsBackendReady() const
{
    return false;
}

void Win32IDE_AgenticIntegration::DrainDeferredQueue()
{
}
