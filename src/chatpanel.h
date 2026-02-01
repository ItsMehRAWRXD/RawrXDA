#pragma once
#include "RawrXD_Window.h"
#include "ai_model_caller.h"
#include <vector>
#include <string>

namespace RawrXD {

class ChatPanel : public Window {
public:
    ChatPanel(Window* parent = nullptr);
    virtual ~ChatPanel();

    void setModelCaller(std::shared_ptr<ModelCaller> caller);
    void appendUserMessage(const std::string& msg);
    void appendAIMessage(const std::string& msg);
    
protected:
    void createControls();
    virtual LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) override;
    virtual void paintEvent(PAINTSTRUCT& ps) override;
    virtual void resizeEvent(int w, int h) override;

private:
    struct ChatMessage {
        bool isUser;
        std::string content;
        std::string timestamp;
    };

    HWND m_hInputEdit = nullptr;
    HWND m_hSendButton = nullptr;
    HWND m_hHistoryList = nullptr; // Using a ListBox or similar for simplicity, or custom draw
    
    std::vector<ChatMessage> m_messages;
    std::shared_ptr<ModelCaller> m_modelCaller;
    
    // UI Helpers
    void onSend();
    void layoutControls();
};

}
