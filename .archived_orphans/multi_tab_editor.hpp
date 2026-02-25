#pragma once

/**
 * @file multi_tab_editor.hpp
 * @brief Stub: multi-tab editor widget for IDE (used by bounded_autonomous_executor).
 */

#include <string>

class MultiTabEditor {
public:
    explicit MultiTabEditor(void* parent = nullptr);
    void openFile(const std::string& path);
    struct Size { int w = 0, h = 0; };
    Size size() const;
    void initialize();
private:
    void* m_parent = nullptr;
};
