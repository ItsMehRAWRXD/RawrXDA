#pragma once

#include "RawrXD_Window.h"

class ChatWorkspace : public RawrXD::Window {

public:
    explicit ChatWorkspace(void* parent = nullptr);
    void initialize();


    void commandIssued(const std::string& command);
};

