#pragma once

/**
 * Win32IDE_AgenticIntegration.h
 * 
 * Drop-in integration header for adding Agentic capabilities to Win32IDE.
 * 
 * Add to Win32IDE class:
 *   #include "agentic/AgenticIDEIntegration.h"
 *   
 *   // Member:
 *   std::unique_ptr<RawrXD::Agentic::AgenticIDEIntegration> m_agenticIntegration;
 *   
 *   // In Create() after other initialization:
 *   m_agenticIntegration = std::make_unique<RawrXD::Agentic::AgenticIDEIntegration>();
 *   m_agenticIntegration->initialize(this, m_hInstance);
 *   
 *   // In status bar click handler:
 *   if (clickedOnAgenticStatus) {
 *       m_agenticIntegration->toggleAutopilotMode();
 *   }
 *   
 *   // In file open handler:
 *   m_agenticIntegration->analyzeCurrentFile(filePath);
 *   
 *   // In WM_COMMAND for suggestion panel:
 *   case IDM_SHOW_SUGGESTIONS:
 *       m_agenticIntegration->toggleSuggestionPanel();
 *       break;
 */

#include "agentic/AgenticIDEIntegration.h"
#include <memory>

// Menu IDs for agentic features
#define IDM_AGENTIC_TOGGLE      6001
#define IDM_AGENTIC_PASSIVE     6002
#define IDM_AGENTIC_SUGGESTIVE  6003
#define IDM_AGENTIC_AUTONOMOUS  6004
#define IDM_SHOW_SUGGESTIONS    6005
#define IDM_APPROVE_ACTION      6006
#define IDM_REJECT_ACTION       6007

// Status bar part IDs
#define SB_PART_AGENTIC         3  // Add to existing status bar parts

/**
 * Example menu structure to add to Win32IDE::CreateMainMenu():
 * 
 * HMENU hAgenticMenu = CreatePopupMenu();
 * AppendMenuW(hAgenticMenu, MF_STRING, IDM_AGENTIC_TOGGLE, L"Toggle Autopilot Mode");
 * AppendMenuW(hAgenticMenu, MF_SEPARATOR, 0, nullptr);
 * AppendMenuW(hAgenticMenu, MF_STRING | MF_CHECKED, IDM_AGENTIC_PASSIVE, L"Passive Mode");
 * AppendMenuW(hAgenticMenu, MF_STRING, IDM_AGENTIC_SUGGESTIVE, L"Suggestive Mode");
 * AppendMenuW(hAgenticMenu, MF_STRING, IDM_AGENTIC_AUTONOMOUS, L"Autonomous Mode");
 * AppendMenuW(hAgenticMenu, MF_SEPARATOR, 0, nullptr);
 * AppendMenuW(hAgenticMenu, MF_STRING, IDM_SHOW_SUGGESTIONS, L"Show Suggestion Panel");
 * AppendMenuW(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hAgenticMenu, L"Agentic");
 */

/**
 * Example WM_COMMAND handler additions:
 * 
 * case IDM_AGENTIC_TOGGLE:
 *     m_agenticIntegration->toggleAutopilotMode();
 *     updateAgenticMenuChecks();
 *     break;
 * 
 * case IDM_AGENTIC_PASSIVE:
 *     m_agenticIntegration->setAutopilotMode(RawrXD::Agentic::AgenticMode::Passive);
 *     updateAgenticMenuChecks();
 *     break;
 * 
 * case IDM_AGENTIC_SUGGESTIVE:
 *     m_agenticIntegration->setAutopilotMode(RawrXD::Agentic::AgenticMode::Suggestive);
 *     updateAgenticMenuChecks();
 *     break;
 * 
 * case IDM_AGENTIC_AUTONOMOUS:
 *     m_agenticIntegration->setAutopilotMode(RawrXD::Agentic::AgenticMode::Autonomous);
 *     updateAgenticMenuChecks();
 *     break;
 * 
 * case IDM_SHOW_SUGGESTIONS:
 *     m_agenticIntegration->toggleSuggestionPanel();
 *     break;
 */

/**
 * Example status bar update:
 * 
 * void Win32IDE::updateStatusBar() {
 *     // ... existing status ...
 *     
 *     // Add agentic status
 *     if (m_agenticIntegration && m_agenticIntegration->isInitialized()) {
 *         std::string agenticStatus = "Agentic: ";
 *         agenticStatus += m_agenticIntegration->getAutopilotModeString();
 *         
 *         int pending = m_agenticIntegration->getPendingSuggestionCount();
 *         if (pending > 0) {
 *             agenticStatus += " (" + std::to_string(pending) + " suggestions)";
 *         }
 *         
 *         SendMessageA(m_hwndStatusBar, SB_SETTEXTA, SB_PART_AGENTIC, (LPARAM)agenticStatus.c_str());
 *     }
 * }
 */

/**
 * Example agentic menu check update:
 * 
 * void Win32IDE::updateAgenticMenuChecks() {
 *     if (!m_hMenu) return;
 *     
 *     HMENU hAgenticMenu = GetSubMenu(m_hMenu, agenticMenuIndex);
 *     if (!hAgenticMenu) return;
 *     
 *     auto mode = m_agenticIntegration->getAutopilotMode();
 *     
 *     CheckMenuItem(hAgenticMenu, IDM_AGENTIC_PASSIVE, 
 *         MF_BYCOMMAND | (mode == RawrXD::Agentic::AgenticMode::Passive ? MF_CHECKED : MF_UNCHECKED));
 *     CheckMenuItem(hAgenticMenu, IDM_AGENTIC_SUGGESTIVE,
 *         MF_BYCOMMAND | (mode == RawrXD::Agentic::AgenticMode::Suggestive ? MF_CHECKED : MF_UNCHECKED));
 *     CheckMenuItem(hAgenticMenu, IDM_AGENTIC_AUTONOMOUS,
 *         MF_BYCOMMAND | (mode == RawrXD::Agentic::AgenticMode::Autonomous ? MF_CHECKED : MF_UNCHECKED));
 * }
 */
