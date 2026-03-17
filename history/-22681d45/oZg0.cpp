#include "auto_bootstrap.hpp"
#include "planner.hpp"
#include "self_patch.hpp"
#include "release_agent.hpp"
#include "meta_learn.hpp"
#include "zero_touch.hpp"
#include <windows.h>
#include <algorithm>

AutoBootstrap* AutoBootstrap::s_instance = nullptr;

AutoBootstrap* AutoBootstrap::instance() {
    if (!s_instance) {
        s_instance = new AutoBootstrap();
    }
    return s_instance;
}

AutoBootstrap::AutoBootstrap()  {}

void AutoBootstrap::installZeroTouch() {
    static ZeroTouch* zero = nullptr;
    if (zero) {
        return;
    }
    zero = new ZeroTouch(instance());
    zero->installAll();
}

void AutoBootstrap::startWithWish(const std::string& wish) {
    instance()->startWithWishInternal(wish);
}

std::string AutoBootstrap::grabWish() {
    // 1. Environment variable (CI / voice assistant / automation)
    const char* env_ptr = std::getenv("RAWRXD_WISH");
    if (env_ptr && env_ptr[0] != '\0') {
        return std::string(env_ptr);
    }
    
    // 2. Clipboard (Windows speech recognition leaves it here)
    std::string spoken;
    if (OpenClipboard(nullptr)) {
        HANDLE hData = GetClipboardData(CF_TEXT);
        if (hData) {
            char* pszText = static_cast<char*>(GlobalLock(hData));
            if (pszText) spoken = pszText;
            GlobalUnlock(hData);
        }
        CloseClipboard();
    }
    
    if (!spoken.empty() && spoken.length() < 200 && spoken.find('\n') == std::string::npos) {
        return spoken;
    }
    
    // 3. Simple console fallback
    printf("What should I build / fix / ship? ");
    char buffer[256];
    if (fgets(buffer, sizeof(buffer), stdin)) {
        std::string typed = buffer;
        typed.erase(std::remove(typed.begin(), typed.end(), '\n'), typed.end());
        typed.erase(std::remove(typed.begin(), typed.end(), '\r'), typed.end());
        return typed;
    }
    
    return "";
}

void AutoBootstrap::startWithWishInternal(const std::string& wish) {
    if (wish.empty()) {
        return;
    }

    wishReceived(wish);

    if (!safetyGate(wish)) {
        return;
    }

    Planner planner;
    auto plan = planner.plan(wish);

    if (plan.empty()) {
        printf("Agent: I don't know how to do that yet.\n");
        executionCompleted(false);
        return;
    }

    executePlan(wish, plan);
}

bool AutoBootstrap::safetyGate(const std::string& wish) {
    // Blacklist dangerous operations
    std::vector<std::string> blacklist = {
        "rm -rf", "format", "del /", "shutdown", 
        "powershell -c \"rm", "remove-item -recurse",
        "dd if=/dev/zero", "mkfs"
    };
    
    for (const auto& item : blacklist) {
        if (wish.find(item) != std::string::npos) {
            return false;
        }
    }
    return true;
}

void AutoBootstrap::wishReceived(const std::string&) {}
void AutoBootstrap::planGenerated(const std::string&) {}
void AutoBootstrap::executionStarted() {}
void AutoBootstrap::executionCompleted(bool) {}
void AutoBootstrap::executePlan(const std::string&, const std::vector<std::string>&) {}

void AutoBootstrap::start() {
    std::string wish = grabWish();
    startWithWishInternal(wish);
}






