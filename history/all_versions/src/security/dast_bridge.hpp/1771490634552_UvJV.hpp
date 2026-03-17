/**
 * @file dast_bridge.hpp
 * @brief DAST report bridge (ZAP/Burp-style) into Problems (stub for patchable build).
 */
#pragma once

#include <string>
#include <vector>

#include <fstream>

struct DASTFinding {
    std::string id;
    std::string severity;
    std::string description;
    std::string url;
    int line = 0;
};

class DASTBridge {
public:
    bool loadReport(const std::string& path) {
        m_findings.clear();
        std::ifstream in(path);
        if (!in.is_open()) return false;
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (content.empty()) return false;
        // Parse JSON-like array of findings: [{"id":..., "severity":..., "description":..., "url":...}]
        size_t pos = 0;
        while ((pos = content.find("\"id\"", pos)) != std::string::npos) {
            DASTFinding f;
            auto extract = [&](const std::string& key) -> std::string {
                auto kp = content.find("\"" + key + "\"", pos);
                if (kp == std::string::npos || kp > pos + 500) return "";
                auto c = content.find(':', kp);
                auto q1 = content.find('"', c + 1);
                auto q2 = content.find('"', q1 + 1);
                if (q1 == std::string::npos || q2 == std::string::npos) return "";
                return content.substr(q1 + 1, q2 - q1 - 1);
            };
            f.id = extract("id");
            f.severity = extract("severity");
            f.description = extract("description");
            f.url = extract("url");
            if (!f.id.empty()) m_findings.push_back(f);
            pos += 4;
        }
        return !m_findings.empty();
    }
    std::vector<DASTFinding> getFindings() const { return m_findings; }
    void clear() { m_findings.clear(); }
private:
    std::vector<DASTFinding> m_findings;
};
