#pragma once

#include <string>
#include <vector>
#include <memory>

#include "logging/logger.h"
#include "metrics/metrics.h"

struct FeatureRequest {
    std::string description;
    std::string context;
    std::string language;
    std::vector<std::string> requirements;
    std::string complexity;
};

struct GeneratedFeature {
    std::string code;
    std::string explanation;
    std::vector<std::string> dependencies;
    double confidence;
};

struct SecurityIssue {
    std::string severity;
    std::string category;
    std::string description;
    std::string location;
    std::vector<std::string> remediationSteps;
};

class AdvancedCodingAgentIntegration {
private:
    std::shared_ptr<Logger> m_logger;
    std::shared_ptr<Metrics> m_metrics;

    std::string m_featureModel = "quantumide-feature:latest";
    std::string m_testModel = "llama3:latest";
    std::string m_documentationModel = "gemma3:latest";
    std::string m_securityModel = "quantumide-security:latest";

public:
    AdvancedCodingAgentIntegration(
        std::shared_ptr<Logger> logger,
        std::shared_ptr<Metrics> metrics
    );

    // Feature generation
    GeneratedFeature implementFeature(const FeatureRequest& request);
    std::vector<GeneratedFeature> generateImplementationOptions(
        const std::string& description,
        const std::string& context
    );

    // Documentation generation
    std::string generateDocumentation(const std::string& code);
    std::string generateFunctionDocumentation(
        const std::string& functionCode,
        const std::string& style = "doxygen"
    );

    // Testing
    std::vector<std::string> generateTests(const std::string& functionCode);

    // Bug detection
    std::vector<std::string> findBugs(const std::string& code);

    // Code optimization
    std::vector<std::string> optimizeCode(const std::string& code);

    // Security analysis
    std::vector<SecurityIssue> scanSecurity(
        const std::string& code,
        const std::string& language = "cpp"
    );

private:
    std::string buildFeaturePrompt(const FeatureRequest& request);
    bool validateGeneratedCode(const std::string& code);
};
