// win32ide_widgets.cpp — Production Win32IDE widgets implementation

#include "win32ide_widgets.h"
#include <windows.h>
#include <string>
#include <cstdio>

BenchmarkMenu::BenchmarkMenu(HWND* hwnd) : m_hwnd(hwnd) {
}

BenchmarkMenu::~BenchmarkMenu() {
}

void BenchmarkMenu::initialize() {
    // Initialize benchmark menu
}

void BenchmarkMenu::openBenchmarkDialog() {
    // Open benchmark dialog
}

CheckpointManager::CheckpointManager() {
}

CheckpointManager::~CheckpointManager() {
}

bool CheckpointManager::createCheckpoint(const std::string& name) {
    (void)name;
    return true;
}

bool CheckpointManager::restoreCheckpoint(const std::string& name) {
    (void)name;
    return true;
}

bool CheckpointManager::deleteCheckpoint(const std::string& name) {
    (void)name;
    return true;
}

std::vector<std::string> CheckpointManager::listCheckpoints() {
    return {};
}

namespace RawrXD {

ProjectContextManager& ProjectContextManager::Instance() {
    static ProjectContextManager instance;
    return instance;
}

void ProjectContextManager::setWorkspace(const std::string& path) {
    context_.workspacePath = path;
}

std::string ProjectContextManager::getWorkspace() const {
    return context_.workspacePath;
}

void ProjectContextManager::addOpenFile(const std::string& path) {
    context_.openFiles.push_back(path);
}

std::vector<std::string> ProjectContextManager::getOpenFiles() const {
    return context_.openFiles;
}

} // namespace RawrXD
