#include "../../include/native_ui.h"
#include <vector>
#include <map>
#include <iostream>
#include <mutex>

namespace {
    struct ComboItem { std::string value; std::string label; int v = 0; };
    std::vector<ComboItem> g_agentCombo;
    std::vector<std::string> g_agentList;
    std::string g_inputText;
    std::mutex g_mu;
}

void* Native_CreateCombo() { return nullptr; }
void Native_SetComboMinWidth(void* /*combo*/, int /*width*/) {}
void Native_SetComboCallback(void* /*combo*/, std::function<void(int, const std::string&)> /*cb*/) {}
void Native_AddComboItem(void* /*combo*/, const std::string& label, int value) {
    std::lock_guard<std::mutex> lk(g_mu);
    g_agentCombo.push_back({label, label, value});
}

void Native_AddComboItemByValue(const std::string& value, const std::string& label) {
    std::lock_guard<std::mutex> lk(g_mu);
    g_agentCombo.push_back({value, label, 0});
}

void Native_RemoveComboItemByValue(const std::string& value) {
    std::lock_guard<std::mutex> lk(g_mu);
    g_agentCombo.erase(std::remove_if(g_agentCombo.begin(), g_agentCombo.end(), [&](const ComboItem& it){ return it.value == value; }), g_agentCombo.end());
}

std::string Native_GetComboValueAt(int index) {
    std::lock_guard<std::mutex> lk(g_mu);
    if (index >= 0 && index < static_cast<int>(g_agentCombo.size())) return g_agentCombo[index].value;
    return {};
}

void Native_SelectComboItemByValue(const std::string& value) {
    // no-op for in-memory stub
}

void* Native_CreateButton(const std::string& text) {
    std::cout << "[native-ui] CreateButton: " << text << std::endl;
    return nullptr;
}
void Native_SetButtonCallback(void* /*btn*/, std::function<void()> /*cb*/) {}
void Native_SetButtonToggle(void* /*btn*/, bool /*toggle*/) {}
void Native_SetButtonChecked(const std::string& /*id*/, bool /*checked*/) {}
void Native_SetButtonText(const std::string& /*id*/, const std::string& /*text*/) {}

void Native_AddAgentListItem(const std::string& text) {
    std::lock_guard<std::mutex> lk(g_mu);
    g_agentList.push_back(text);
}

void Native_RemoveAgentListItemByValue(const std::string& /*value*/) {
    // best-effort no-op
}

void Native_SelectAgentListRowByValue(const std::string& /*value*/) {}
void Native_SelectAgentListRowByIndex(int /*index*/) {}

void Native_ClearChat() {
    std::cout << "[native-ui] Chat cleared" << std::endl;
}

void Native_AppendChatMessage(const std::string& sender, const std::string& message, const std::string& timestamp, bool isAgent) {
    std::lock_guard<std::mutex> lk(g_mu);
    std::cout << "[chat] " << timestamp << " " << sender << (isAgent ? " (agent): " : ": ") << message << std::endl;
}

void Native_ScrollChatToBottom() {}

std::string Native_GetInputText() { std::lock_guard<std::mutex> lk(g_mu); return g_inputText; }
void Native_ClearInputText() { std::lock_guard<std::mutex> lk(g_mu); g_inputText.clear(); }
void Native_SetInputText(const std::string& text) { std::lock_guard<std::mutex> lk(g_mu); g_inputText = text; }
