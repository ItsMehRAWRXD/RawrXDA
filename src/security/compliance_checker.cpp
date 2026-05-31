// ============================================================================
// Compliance Checker — Security Compliance and Standards Validation
// Validates code against security standards and compliance requirements
// ============================================================================
#pragma once
#include "../inference/SovereignInferenceClient.h"
#include "../security/vulnerability_scanner.h"
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <set>

namespace RawrXD::Security {

enum class ComplianceStandard {
    OWASP_TOP_10,
    CWE_TOP_25,
    PCI_DSS,
    HIPAA,
    GDPR,
    SOC2,
    ISO27001,
    NIST_CSF
};

enum class ComplianceStatus {
    COMPLIANT,
    NON_COMPLIANT,
    PARTIAL,
    NOT_APPLICABLE
};

struct ComplianceRule {
    std::string id;
    std::string description;
    ComplianceStandard standard;
    std::vector<std::string> applicableCategories;
    std::function<bool(const std::string&)> check;
    std::string remediation;
};

struct ComplianceFinding {
    std::string ruleId;
    std::string filePath;
    int lineNumber;
    ComplianceStatus status;
    std::string details;
    std::string evidence;
    std::vector<std::string> references;
};

struct ComplianceReport {
    ComplianceStandard standard;
    std::chrono::system_clock::time_point generatedAt;
    std::vector<ComplianceFinding> findings;
    std::map<ComplianceStatus, int> summary;
    double complianceScore;
    std::vector<std::string> recommendations;
};

struct RegulatoryUpdate {
    ComplianceStandard standard;
    std::string version;
    std::vector<std::string> newRules;
    std::vector<std::string> modifiedRules;
    std::chrono::system_clock::time_point effectiveDate;
};

class ComplianceChecker {
public:
    explicit ComplianceChecker(std::shared_ptr<SovereignInferenceClient> aiClient)
        : m_aiClient(aiClient) {
        InitializeRules();
    }

    ComplianceReport CheckCompliance(const std::string& code, ComplianceStandard standard) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        ComplianceReport report;
        report.standard = standard;
        report.generatedAt = std::chrono::system_clock::now();
        
        // Get applicable rules
        auto rules = GetRulesForStandard(standard);
        
        // Check each rule
        for (const auto& rule : rules) {
            ComplianceFinding finding;
            finding.ruleId = rule.id;
            finding.status = rule.check(code) ? ComplianceStatus::COMPLIANT : ComplianceStatus::NON_COMPLIANT;
            finding.details = rule.description;
            
            if (finding.status == ComplianceStatus::NON_COMPLIANT) {
                finding.evidence = "Rule violation detected";
                finding.references.push_back(rule.remediation);
            }
            
            report.findings.push_back(finding);
            report.summary[finding.status]++;
        }
        
        // Calculate compliance score
        int total = report.findings.size();
        int compliant = report.summary[ComplianceStatus::COMPLIANT];
        report.complianceScore = total > 0 ? (compliant * 100.0 / total) : 100.0;
        
        // Generate recommendations
        report.recommendations = GenerateRecommendations(report);
        
        // AI-enhanced analysis
        if (m_aiClient && m_aiClient->IsLoaded()) {
            auto aiFindings = PerformAIComplianceCheck(code, standard);
            report.findings.insert(report.findings.end(), aiFindings.begin(), aiFindings.end());
        }
        
        return report;
    }

    void GenerateComplianceDocumentation(const ComplianceReport& report) {
        std::ostringstream doc;
        doc << "# Compliance Report\n\n";
        doc << "**Standard:** " << StandardToString(report.standard) << "\n";
        doc << "**Generated:** " << FormatTime(report.generatedAt) << "\n";
        doc << "**Compliance Score:** " << std::fixed << std::setprecision(1) << report.complianceScore << "%\n\n";
        
        doc << "## Summary\n";
        for (const auto& [status, count] : report.summary) {
            doc << "- " << StatusToString(status) << ": " << count << "\n";
        }
        
        doc << "\n## Findings\n";
        for (const auto& finding : report.findings) {
            doc << "### " << finding.ruleId << "\n";
            doc << "**Status:** " << StatusToString(finding.status) << "\n";
            doc << "**Details:** " << finding.details << "\n";
            if (!finding.evidence.empty()) {
                doc << "**Evidence:** " << finding.evidence << "\n";
            }
            doc << "\n";
        }
        
        if (!report.recommendations.empty()) {
            doc << "## Recommendations\n";
            for (const auto& rec : report.recommendations) {
                doc << "- " << rec << "\n";
            }
        }
        
        // Store documentation
        m_documentation[report.standard] = doc.str();
    }

    void MonitorRegulatoryChanges(const RegulatoryUpdate& update) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Update rules based on regulatory changes
        for (const auto& newRule : update.newRules) {
            // Add new rule
        }
        
        for (const auto& modifiedRule : update.modifiedRules) {
            // Update existing rule
        }
        
        // Store update
        m_regulatoryUpdates.push_back(update);
    }

    std::vector<ComplianceStandard> GetSupportedStandards() const {
        return {
            ComplianceStandard::OWASP_TOP_10,
            ComplianceStandard::CWE_TOP_25,
            ComplianceStandard::PCI_DSS,
            ComplianceStandard::HIPAA,
            ComplianceStandard::GDPR,
            ComplianceStandard::SOC2,
            ComplianceStandard::ISO27001,
            ComplianceStandard::NIST_CSF
        };
    }

    std::string GetComplianceDocumentation(ComplianceStandard standard) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_documentation.find(standard);
        if (it != m_documentation.end()) {
            return it->second;
        }
        return "";
    }

private:
    std::shared_ptr<SovereignInferenceClient> m_aiClient;
    mutable std::mutex m_mutex;
    std::vector<ComplianceRule> m_rules;
    std::map<ComplianceStandard, std::string> m_documentation;
    std::vector<RegulatoryUpdate> m_regulatoryUpdates;

    void InitializeRules() {
        // OWASP Top 10 rules
        m_rules.push_back({
            "OWASP-01",
            "Injection vulnerabilities check",
            ComplianceStandard::OWASP_TOP_10,
            {"security"},
            [](const std::string& code) {
                return code.find("sql") == std::string::npos || 
                       code.find("prepare") != std::string::npos;
            },
            "Use parameterized queries"
        });
        
        m_rules.push_back({
            "OWASP-02",
            "Broken authentication check",
            ComplianceStandard::OWASP_TOP_10,
            {"security"},
            [](const std::string& code) {
                return code.find("password") == std::string::npos || 
                       code.find("hash") != std::string::npos;
            },
            "Implement proper password hashing"
        });
        
        // CWE Top 25 rules
        m_rules.push_back({
            "CWE-78",
            "OS Command Injection",
            ComplianceStandard::CWE_TOP_25,
            {"security"},
            [](const std::string& code) {
                return code.find("system(") == std::string::npos || 
                       code.find("validate") != std::string::npos;
            },
            "Validate all input before using in system calls"
        });
        
        m_rules.push_back({
            "CWE-79",
            "Cross-site Scripting",
            ComplianceStandard::CWE_TOP_25,
            {"security"},
            [](const std::string& code) {
                return code.find("innerHTML") == std::string::npos || 
                       code.find("sanitize") != std::string::npos;
            },
            "Sanitize user input before rendering"
        });
    }

    std::vector<ComplianceRule> GetRulesForStandard(ComplianceStandard standard) {
        std::vector<ComplianceRule> applicable;
        for (const auto& rule : m_rules) {
            if (rule.standard == standard) {
                applicable.push_back(rule);
            }
        }
        return applicable;
    }

    std::vector<ComplianceFinding> PerformAIComplianceCheck(const std::string& code,
                                                               ComplianceStandard standard) {
        std::vector<ComplianceFinding> findings;
        
        if (!m_aiClient || !m_aiClient->IsLoaded()) {
            return findings;
        }

        std::string prompt = "Check this code for " + StandardToString(standard) + 
                            " compliance:\n```\n" + code.substr(0, 1000) + "\n```";
        
        std::vector<ChatMessage> messages = {
            {"system", "You are a compliance expert. Identify compliance violations."},
            {"user", prompt}
        };

        auto result = m_aiClient->ChatSync(messages);
        
        if (result.success) {
            ComplianceFinding finding;
            finding.ruleId = "AI-CHECK";
            finding.status = ComplianceStatus::PARTIAL;
            finding.details = "AI Analysis: " + result.response;
            findings.push_back(finding);
        }
        
        return findings;
    }

    std::vector<std::string> GenerateRecommendations(const ComplianceReport& report) {
        std::vector<std::string> recommendations;
        
        int nonCompliant = report.summary[ComplianceStatus::NON_COMPLIANT];
        if (nonCompliant > 0) {
            recommendations.push_back("Address " + std::to_string(nonCompliant) + " non-compliant findings");
        }
        
        if (report.complianceScore < 80.0) {
            recommendations.push_back("Improve overall compliance score to at least 80%");
        }
        
        return recommendations;
    }

    std::string StandardToString(ComplianceStandard standard) {
        switch (standard) {
            case ComplianceStandard::OWASP_TOP_10: return "OWASP Top 10";
            case ComplianceStandard::CWE_TOP_25: return "CWE Top 25";
            case ComplianceStandard::PCI_DSS: return "PCI DSS";
            case ComplianceStandard::HIPAA: return "HIPAA";
            case ComplianceStandard::GDPR: return "GDPR";
            case ComplianceStandard::SOC2: return "SOC 2";
            case ComplianceStandard::ISO27001: return "ISO 27001";
            case ComplianceStandard::NIST_CSF: return "NIST CSF";
            default: return "Unknown";
        }
    }

    std::string StatusToString(ComplianceStatus status) {
        switch (status) {
            case ComplianceStatus::COMPLIANT: return "Compliant";
            case ComplianceStatus::NON_COMPLIANT: return "Non-Compliant";
            case ComplianceStatus::PARTIAL: return "Partial";
            case ComplianceStatus::NOT_APPLICABLE: return "Not Applicable";
            default: return "Unknown";
        }
    }

    std::string FormatTime(std::chrono::system_clock::time_point time) {
        auto timeT = std::chrono::system_clock::to_time_t(time);
        std::ostringstream oss;
        oss << std::put_time(std::localtime(&timeT), "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }
};

} // namespace RawrXD::Security
