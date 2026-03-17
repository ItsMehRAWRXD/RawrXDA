#pragma once
#include <string>
#include <thread>
#include <atomic>
#include "gui.h"

class APIServer {
public:
    explicit APIServer(AppState& state);
    ~APIServer();

    void Start(int port);
    void Stop();

private:
    void ServerLoop(int port);

    AppState& m_state;
    std::atomic<bool> m_running;
    std::thread m_thread;
};
