#include "status_bar.h"

namespace RawrXD::UI {
    StatusBar& StatusBar::getInstance() {
        static StatusBar inst;
        return inst;
    }

    void StatusBar::setText(StatusField field, const std::string& text) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_fields[field] = text;
        if (m_updateCallback) m_updateCallback(field, text);
    }

    std::string StatusBar::getText(StatusField field) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_fields.find(field);
        return (it != m_fields.end()) ? it->second : "";
    }

    void StatusBar::setProgress(int percent) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_progress = std::clamp(percent, 0, 100);
        if (m_progressCallback) m_progressCallback(m_progress);
    }

    void StatusBar::showMessage(const std::string& msg, int timeoutMs) {
        setText(StatusField::GENERAL, msg);
        if (timeoutMs > 0 && m_clearCallback) {
            // In production, spawn a timer thread
            m_clearCallback(timeoutMs);
        }
    }
}
