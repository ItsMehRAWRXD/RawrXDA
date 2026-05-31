#pragma once
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <functional>

namespace vscode {

class TextDocument {
public:
    std::string uri;
    std::string getText() const;
};

class Window {
public:
    void showInformationMessage(const std::string& message);
    void showErrorMessage(const std::string& message);
};

class Workspace {
public:
    std::vector<std::shared_ptr<TextDocument>> textDocuments;
};

class Commands {
public:
    void registerCommand(const std::string& command, std::function<void()> callback);
    void executeCommand(const std::string& command);
};

struct ExtensionContext {
    std::string extensionPath;
    void subscribe(std::function<void()> disposeFunc);
};

// Global bridge access
extern Window window;
extern Workspace workspace;
extern Commands commands;

}
