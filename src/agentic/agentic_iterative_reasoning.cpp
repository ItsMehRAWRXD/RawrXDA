/**
 * @file agentic_iterative_reasoning.cpp
 * @brief Production implementation of iterative reasoning loop
 */

#include "agentic_iterative_reasoning.h"
#include "agentic_loop_state.h"
#include "agentic_engine.h"
#include "inference_engine.hpp"
#include <algorithm>
#include <sstream>
#include <iostream>
#include <regex>
#include <math>

AgenticIterativeReasoning::AgenticIterativeReasoning() = default;
AgenticIterativeReasoning::~AgenticIterativeReasoning() = default;

void AgenticIterativeReasoning::initialize(AgenticEngine* engine, AgenticLoopState* state, InferenceEngine* inference) {
    if (!engine || !state || !inference) {
        m_lastError = "Invalid initialization parameters";
        return;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    m_engine = engine;
    m_state = state;
    m_inference = inference;
    m_initialized = true;
    m_history.clear();
    m_lastError.clear();
}

std::vector<ReasoningStep> AgenticIterativeReasoning::reason(const std::string& problem, const ReasoningStrategy& strategy) {
    std::vector<ReasoningStep> steps;
    
    if (!m_initialized) {
        m_lastError = "Reasoning engine not initialized";
        if (onError) onError(m_lastError);
        return steps;
    }
    
    m_reasoning = true;
    
    // Phase 1: Problem Analysis
    auto analysis = analyzeProblem(problem);
    steps.push_back(analysis);
    if (onStepCompleted) onStepCompleted(analysis);
    
    // Phase 2: Generate Hypotheses
    auto hypotheses = generateHypotheses(problem, strategy.maxHypotheses);
    for (auto& h : hypotheses) {
        steps.push_back(h);
        if (onStepCompleted) onStepCompleted(h);
    }
    
    // Phase 3: Test Hypotheses (if enabled)
    if (strategy.enableHypothesisTesting) {
        for (auto& h : hypotheses) {
            if (!h.successful) continue;
            auto test = testHypothesis(h.output, problem);
            steps.push_back(test);
            if (onStepCompleted) onStepCompleted(test);
        }
    }
    
    // Phase 4: Evaluate Evidence
    auto evaluation = evaluateEvidence(steps);
    steps.push_back(evaluation);
    if (onStepCompleted) onStepCompleted(evaluation);
    
    // Phase 5: Self-Correction (if needed)
    if (strategy.enableSelfCorrection && !meetsThreshold(steps, strategy.confidenceThreshold)) {
        for (int i = 0; i < strategy.maxIterations && !meetsThreshold(steps, strategy.confidenceThreshold); ++i) {
            auto correction = selfCorrect(steps.back(), "Confidence below threshold");
            steps.push_back(correction);
            if (onStepCompleted) onStepCompleted(correction);
        }
    }
    
    // Update confidence
    float finalConfidence = calculateConfidence(steps);
    if (onConfidenceUpdated) onConfidenceUpdated(finalConfidence);
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_history.insert(m_history.end(), steps.begin(), steps.end());
    }
    
    m_reasoning = false;
    return steps;
}

ReasoningStrategy AgenticIterativeReasoning::selectStrategy(const std::string& problem) const {
    ReasoningStrategy strategy;
    
    std::string lower = problem;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    // Analyze problem characteristics
    if (lower.find("debug") != std::string::npos || lower.find("error") != std::string::npos) {
        strategy.name = "debug";
        strategy.maxIterations = 8;
        strategy.confidenceThreshold = 0.8f;
        strategy.enableSelfCorrection = true;
        strategy.maxHypotheses = 5;
    } else if (lower.find("refactor") != std::string::npos || lower.find("improve") != std::string::npos) {
        strategy.name = "refactor";
        strategy.maxIterations = 4;
        strategy.confidenceThreshold = 0.75f;
        strategy.enableHypothesisTesting = true;
        strategy.maxHypotheses = 3;
    } else if (lower.find("create") != std::string::npos || lower.find("generate") != std::string::npos) {
        strategy.name = "create";
        strategy.maxIterations = 6;
        strategy.confidenceThreshold = 0.7f;
        strategy.explorationFactor = 0.5f;
        strategy.maxHypotheses = 4;
    } else {
        strategy.name = "general";
        strategy.maxIterations = 5;
        strategy.confidenceThreshold = 0.7f;
    }
    
    return strategy;
}

ReasoningStep AgenticIterativeReasoning::analyzeProblem(const std::string& problem) {
    ReasoningStep step;
    step.stepNumber = 1;
    step.type = "analyze";
    step.description = "Analyze problem structure and identify key components";
    step.input = problem;
    step.timestamp = std::chrono::steady_clock::now();
    
    auto issues = extractKeyIssues(problem);
    auto category = categorizeProblem(problem);
    
    std::ostringstream output;
    output << "Category: " << category << "\n";
    output << "Key issues identified: " << issues.size() << "\n";
    for (const auto& issue : issues) {
        output << "  - " << issue << "\n";
        step.findings.push_back(issue);
    }
    
    step.output = output.str();
    step.confidence = std::min(0.9f, 0.5f + 0.1f * static_cast<float>(issues.size()));
    step.successful = !issues.empty();
    
    return step;
}

std::vector<ReasoningStep> AgenticIterativeReasoning::generateHypotheses(const std::string& problem, int maxHypotheses) {
    std::vector<ReasoningStep> hypotheses;
    
    auto issues = extractKeyIssues(problem);
    int count = std::min(maxHypotheses, static_cast<int>(issues.size()));
    
    for (int i = 0; i < count; ++i) {
        ReasoningStep step;
        step.stepNumber = 2 + i;
        step.type = "hypothesize";
        step.description = "Generate hypothesis for: " + issues[i];
        step.input = issues[i];
        step.timestamp = std::chrono::steady_clock::now();
        
        // Query LLM for hypothesis generation
        std::string prompt = "Given the issue: " + issues[i] + 
                           "\nGenerate a concrete hypothesis about the root cause.";
        step.output = queryLLM(prompt, 200);
        
        step.confidence = 0.6f + 0.05f * static_cast<float>(i);
        step.successful = !step.output.empty();
        
        hypotheses.push_back(step);
    }
    
    return hypotheses;
}

ReasoningStep AgenticIterativeReasoning::testHypothesis(const std::string& hypothesis, const std::string& problem) {
    ReasoningStep step;
    step.type = "experiment";
    step.description = "Test hypothesis against evidence";
    step.input = hypothesis;
    step.timestamp = std::chrono::steady_clock::now();
    
    // Validate hypothesis structure
    bool valid = validateHypothesis(hypothesis);
    
    // Score evidence
    float evidenceScore = scoreEvidence(hypothesis, problem);
    
    std::ostringstream output;
    output << "Hypothesis valid: " << (valid ? "yes" : "no") << "\n";
    output << "Evidence score: " << evidenceScore << "\n";
    
    step.output = output.str();
    step.confidence = evidenceScore;
    step.successful = valid && evidenceScore > 0.5f;
    
    return step;
}

ReasoningStep AgenticIterativeReasoning::evaluateEvidence(const std::vector<ReasoningStep>& hypotheses) {
    ReasoningStep step;
    step.type = "evaluate";
    step.description = "Evaluate all evidence and select best hypothesis";
    step.timestamp = std::chrono::steady_clock::now();
    
    float totalConfidence = 0.0f;
    int successfulCount = 0;
    std::string bestHypothesis;
    float bestConfidence = 0.0f;
    
    for (const auto& h : hypotheses) {
        if (h.type == "hypothesize" || h.type == "experiment") {
            totalConfidence += h.confidence;
            if (h.successful) successfulCount++;
            if (h.confidence > bestConfidence) {
                bestConfidence = h.confidence;
                bestHypothesis = h.output;
            }
        }
    }
    
    float avgConfidence = hypotheses.empty() ? 0.0f : totalConfidence / hypotheses.size();
    
    std::ostringstream output;
    output << "Total hypotheses: " << hypotheses.size() << "\n";
    output << "Successful: " << successfulCount << "\n";
    output << "Average confidence: " << avgConfidence << "\n";
    output << "Best hypothesis confidence: " << bestConfidence << "\n";
    
    step.output = output.str();
    step.confidence = avgConfidence;
    step.successful = successfulCount > 0;
    
    return step;
}

ReasoningStep AgenticIterativeReasoning::selfCorrect(const ReasoningStep& failedStep, const std::string& feedback) {
    ReasoningStep step;
    step.type = "correct";
    step.description = "Self-correction based on: " + feedback;
    step.input = failedStep.output;
    step.timestamp = std::chrono::steady_clock::now();
    
    // Query LLM for correction
    std::string prompt = "The following reasoning failed:\n" + failedStep.output + 
                       "\n\nFeedback: " + feedback + 
                       "\n\nPlease provide a corrected approach.";
    step.output = queryLLM(prompt, 300);
    
    step.confidence = std::min(0.9f, failedStep.confidence + 0.1f);
    step.successful = !step.output.empty() && step.confidence > 0.5f;
    
    return step;
}

float AgenticIterativeReasoning::calculateConfidence(const std::vector<ReasoningStep>& steps) const {
    if (steps.empty()) return 0.0f;
    
    float total = 0.0f;
    int count = 0;
    for (const auto& step : steps) {
        if (step.successful) {
            total += step.confidence;
            count++;
        }
    }
    
    return count > 0 ? total / count : 0.0f;
}

bool AgenticIterativeReasoning::meetsThreshold(const std::vector<ReasoningStep>& steps, float threshold) const {
    return calculateConfidence(steps) >= threshold;
}

void AgenticIterativeReasoning::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_history.clear();
    m_lastError.clear();
}

std::vector<ReasoningStep> AgenticIterativeReasoning::getHistory() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_history;
}

std::string AgenticIterativeReasoning::queryLLM(const std::string& prompt, int maxTokens) {
    if (!m_inference) {
        return "[Error: Inference engine not available]";
    }
    
    // Use the inference engine to query the LLM
    try {
        RawrXD::InferRequest req;
        req.id = 0;
        req.prompt = prompt;
        req.max_tokens = maxTokens > 0 ? maxTokens : 512;
        req.tenant_id = "agentic_reasoning";
        
        std::promise<std::string> completion_promise;
        req.complete_cb = [&](const std::string& completion, const std::string&) {
            completion_promise.set_value(completion);
        };
        
        auto& bo = RawrXD::BackendOrchestrator::Instance();
        uint64_t req_id = bo.Enqueue(req);
        
        auto future = completion_promise.get_future();
        if (future.wait_for(std::chrono::seconds(60)) != std::future_status::ready) {
            bo.Cancel(req_id);
            return "[Error: LLM query timeout]";
        }
        
        std::string result = future.get();
        if (result.empty()) {
            return "[Error: LLM returned empty response]";
        }
        return result;
    } catch (const std::exception& e) {
        return std::string("[Error: LLM query failed: ") + e.what() + "]";
    } catch (...) {
        return "[Error: LLM query failed (unknown exception)]";
    }
}

std::vector<std::string> AgenticIterativeReasoning::extractKeyIssues(const std::string& problem) {
    std::vector<std::string> issues;
    
    std::string lower = problem;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    // Pattern matching for common issue types
    static const std::vector<std::pair<std::string, std::string>> patterns = {
        {"error", "Error condition detected"},
        {"exception", "Exception handling concern"},
        {"crash", "Crash/stability issue"},
        {"memory leak", "Memory leak concern"},
        {"performance", "Performance optimization needed"},
        {"slow", "Performance bottleneck"},
        {"deadlock", "Deadlock potential"},
        {"race condition", "Threading/race condition"},
        {"null pointer", "Null pointer dereference"},
        {"nullptr", "Null pointer dereference"},
        {"buffer overflow", "Buffer overflow vulnerability"},
        {"todo", "Incomplete implementation"},
        {"fixme", "Known defect marker"},
        {"hack", "Technical debt"},
        {"refactor", "Refactoring opportunity"},
        {"deprecated", "Deprecated API usage"},
    };
    
    for (const auto& [pattern, label] : patterns) {
        if (lower.find(pattern) != std::string::npos) {
            issues.push_back(label);
        }
    }
    
    // Extract code references
    std::regex codeRef(R"(\b([A-Z][a-zA-Z]+(?:::[a-zA-Z_]+)+)\b)");
    std::smatch match;
    std::string searchTarget = problem;
    while (std::regex_search(searchTarget, match, codeRef)) {
        issues.push_back("Code reference: " + match[1].str());
        searchTarget = match.suffix().str();
    }
    
    if (issues.empty()) {
        issues.push_back("General analysis required");
    }
    
    return issues;
}

std::string AgenticIterativeReasoning::categorizeProblem(const std::string& problem) const {
    std::string lower = problem;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    if (lower.find("security") != std::string::npos || lower.find("vulnerability") != std::string::npos) {
        return "Security";
    } else if (lower.find("performance") != std::string::npos || lower.find("slow") != std::string::npos) {
        return "Performance";
    } else if (lower.find("memory") != std::string::npos || lower.find("leak") != std::string::npos) {
        return "Memory";
    } else if (lower.find("thread") != std::string::npos || lower.find("deadlock") != std::string::npos) {
        return "Concurrency";
    } else if (lower.find("build") != std::string::npos || lower.find("compile") != std::string::npos) {
        return "Build";
    } else if (lower.find("test") != std::string::npos || lower.find("coverage") != std::string::npos) {
        return "Testing";
    } else if (lower.find("refactor") != std::string::npos || lower.find("clean") != std::string::npos) {
        return "Refactoring";
    }
    
    return "General";
}

bool AgenticIterativeReasoning::validateHypothesis(const std::string& hypothesis) const {
    if (hypothesis.empty()) return false;
    if (hypothesis.length() < 10) return false;
    
    std::string lower = hypothesis;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    // Check for invalid patterns
    if (lower.find("impossible") != std::string::npos && lower.find("not impossible") == std::string::npos) {
        return false;
    }
    
    return true;
}

float AgenticIterativeReasoning::scoreEvidence(const std::string& evidence, const std::string& problem) const {
    if (evidence.empty() || problem.empty()) return 0.0f;
    
    // Simple keyword overlap scoring
    std::set<std::string> problemWords;
    std::set<std::string> evidenceWords;
    
    auto tokenize = [](const std::string& text, std::set<std::string>& words) {
        std::istringstream iss(text);
        std::string word;
        while (iss >> word) {
            std::string lower;
            for (char c : word) {
                if (std::isalnum(static_cast<unsigned char>(c))) {
                    lower += std::tolower(static_cast<unsigned char>(c));
                }
            }
            if (lower.length() > 3) {
                words.insert(lower);
            }
        }
    };
    
    tokenize(problem, problemWords);
    tokenize(evidence, evidenceWords);
    
    if (problemWords.empty()) return 0.0f;
    
    int intersection = 0;
    for (const auto& word : problemWords) {
        if (evidenceWords.count(word)) intersection++;
    }
    
    float jaccard = static_cast<float>(intersection) / 
                   (problemWords.size() + evidenceWords.size() - intersection);
    
    return std::min(1.0f, jaccard * 2.0f + 0.3f);
}
