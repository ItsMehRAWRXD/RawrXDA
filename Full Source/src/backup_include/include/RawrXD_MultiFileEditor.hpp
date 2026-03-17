/**
 * @file RawrXD_MultiFileEditor.hpp
 * @brief Multi-file editor widget (stub for patchable build).
 */
#pragma once

#include <string>
#include <vector>
#include <memory>

class RawrXD_MultiFileEditor {
public:
    RawrXD_MultiFileEditor(void* parent = nullptr) {}
    void openFile(const std::string& path) { (void)path; }
    void closeFile(const std::string& path) { (void)path; }
    std::string currentFilePath() const { return {}; }
    std::string currentText() const { return {}; }
    void setCurrentText(const std::string& text) { (void)text; }
};
