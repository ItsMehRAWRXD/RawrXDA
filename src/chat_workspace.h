#pragma once


class ChatWorkspace : public void {

public:
    explicit ChatWorkspace(void* parent = nullptr);
    void initialize();


    void commandIssued(const std::string& command);
};

