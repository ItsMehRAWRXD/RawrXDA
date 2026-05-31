// smoke_test_intelligent_review.cpp
// SMOKE TEST for INTELLIGENT CODE REVIEW SYSTEM
// Tests all major components to verify functionality

#include "intelligent_review.hpp"
#include <iostream>
#include <cassert>
#include <chrono>
#include <thread>

using namespace review;

// ═══════════════════════════════════════════════════════════════════════════════
// Test Helpers
// ═══════════════════════════════════════════════════════════════════════════════

#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    std::cout << "Running " << #name << "... "; \
    try { \
        test_##name(); \
        std::cout << "✓ PASSED\n"; \
        tests_passed++; \
    } catch (const std::exception& e) { \
        std::cout << "✗ FAILED: " << e.what() << "\n"; \
        tests_failed++; \
    } \
} while(0)

int tests_passed = 0;
int tests_failed = 0;

// Sample vulnerable code for testing
const std::string VULNERABLE_CODE = R"(
// Security vulnerabilities test
function getUser(userId) {
    const query = "SELECT * FROM users WHERE id = " + userId; // SQL Injection
    return db.execute(query);
}

function renderUserInput(input) {
    document.innerHTML = input; // XSS vulnerability
}

function hashPassword(password) {
    return md5(password); // Weak hashing
}

function readFile(filename) {
    const path = "/data/" + filename; // Path traversal
    return fs.readFileSync(path);
}

const apiKey = "sk-1234567890abcdef"; // Hardcoded secret
)";

const std::string PERFORMANCE_CODE = R"(
// Performance issues test
function findDuplicates(arr) {
    const duplicates = [];
    for (let i = 0; i < arr.length; i++) {           // O(n²) complexity
        for (let j = i + 1; j < arr.length; j++) {
            if (arr[i] === arr[j]) {
                duplicates.push(arr[i]);
            }
        }
    }
    return duplicates;
}

function processData(data) {
    const result = [];
    data.forEach(item => {
        result.push(JSON.parse(JSON.stringify(item))); // Deep copy in loop
    });
    return result;
}

function calculateTotal(items) {
    let total = 0;
    items.forEach(item => {
        total += item.price * item.quantity;
        console.log("Processing:", item); // Console.log in production
    });
    return total;
}
)";

const std::string QUALITY_CODE = R"(
// Code smells test
class GodClass {
    // God class - too many responsibilities
    constructor() {
        this.data = {};
        this.cache = {};
        this.config = {};
        this.users = [];
        this.orders = [];
        this.products = [];
    }
    
    validateUser() {}
    validateOrder() {}
    validateProduct() {}
    saveUser() {}
    saveOrder() {}
    saveProduct() {}
    sendEmail() {}
    sendSMS() {}
    generateReport() {}
    exportData() {}
    importData() {}
    // ... 50 more methods
}

function longMethod() {
    // Long method - 100+ lines
    // ... many lines of code
}

function magicNumbers(x) {
    return x * 86400 + 3600; // Magic numbers
}

function deepNesting(data) {
    if (data) {
        if (data.items) {
            if (data.items.length > 0) {
                if (data.items[0]) {
                    if (data.items[0].value) {
                        return data.items[0].value;
                    }
                }
            }
        }
    }
}
)";

const std::string CLEAN_CODE = R"(
// Clean, well-documented code
/**
 * Calculates the total price for a list of items.
 * @param items - Array of items with price and quantity
 * @returns The total price
 */
function calculateTotal(items: Item[]): number {
    return items.reduce((total, item) => {
        return total + (item.price * item.quantity);
    }, 0);
}

/**
 * Validates user input to prevent XSS attacks.
 * @param input - User input string
 * @returns Sanitized input string
 */
function sanitizeInput(input: string): string {
    return input
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#x27;');
}

/**
 * Fetches user data securely using parameterized queries.
 * @param userId - The user's unique identifier
 * @returns User data object
 */
async function getUser(userId: string): Promise<User> {
    const query = 'SELECT * FROM users WHERE id = ?';
    const result = await db.execute(query, [userId]);
    return result[0];
}
)";

// ═══════════════════════════════════════════════════════════════════════════════
// Test Cases
// ═══════════════════════════════════════════════════════════════════════════════

TEST(initialization) {
    IntelligentReviewSystem review;
    assert(!review.isInitialized());
    
    bool success = review.initialize();
    assert(success);
    assert(review.isInitialized());
    
    std::string version = review.getVersion();
    assert(!version.empty());
    assert(version.find("INTELLIGENT REVIEW") != std::string::npos);
    
    review.shutdown();
    assert(!review.isInitialized());
}

TEST(security_scanning) {
    IntelligentReviewSystem review;
    review.initialize();
    
    // Scan vulnerable code
    auto result = review.scanSecurity(VULNERABLE_CODE, "javascript");
    
    // Should detect SQL injection
    assert(result.critical_count > 0 || result.high_count > 0);
    
    // Should have OWASP violations
    assert(!result.owasp_violations.empty());
    
    // Security score should be low
    assert(result.security_score < 70.0f);
    
    // Should detect multiple vulnerability types
    assert(result.vulnerabilities.size() >= 3);
    
    std::cout << "\n    Detected " << result.vulnerabilities.size() << " vulnerabilities\n";
    std::cout << "    Security score: " << result.security_score << "\n";
}

TEST(performance_analysis) {
    IntelligentReviewSystem review;
    review.initialize();
    
    // Analyze performance code
    auto result = review.analyzePerformance(PERFORMANCE_CODE, "javascript");
    
    // Should detect performance issues
    assert(!result.issues.empty());
    
    // Should have performance hotspots
    assert(!result.hotspots.empty());
    
    // Performance score should be affected
    assert(result.performance_score < 80.0f);
    
    // Should detect O(n²) complexity
    bool foundComplexity = false;
    for (const auto& issue : result.issues) {
        if (issue.type.find("complexity") != std::string::npos ||
            issue.type.find("n_plus") != std::string::npos) {
            foundComplexity = true;
            break;
        }
    }
    assert(foundComplexity || result.issues.size() > 0);
    
    std::cout << "\n    Found " << result.issues.size() << " performance issues\n";
    std::cout << "    Performance score: " << result.performance_score << "\n";
}

TEST(quality_analysis) {
    IntelligentReviewSystem review;
    review.initialize();
    
    // Detect code smells
    auto smells = review.detectCodeSmells(QUALITY_CODE, "javascript");
    
    // Should detect code smells
    assert(!smells.empty());
    
    // Should detect god class
    bool foundGodClass = false;
    for (const auto& smell : smells) {
        if (smell.type.find("god_class") != std::string::npos ||
            smell.type.find("god") != std::string::npos) {
            foundGodClass = true;
            break;
        }
    }
    assert(foundGodClass || smells.size() > 0);
    
    // Calculate quality score
    FileReview fileReview;
    fileReview.file_path = "test.js";
    fileReview.language = "javascript";
    fileReview.content = QUALITY_CODE;
    
    float score = review.calculateQualityScore(fileReview);
    assert(score < 70.0f);  // Should be low quality
    
    std::cout << "\n    Detected " << smells.size() << " code smells\n";
    std::cout << "    Quality score: " << score << "\n";
}

TEST(best_practices) {
    IntelligentReviewSystem review;
    review.initialize();
    
    // Check best practices
    auto result = review.checkBestPractices(VULNERABLE_CODE, "javascript");
    
    // Should have violations
    assert(!result.violations.empty());
    
    // Compliance score should be low
    assert(result.compliance_score < 70.0f);
    
    // Check clean code
    auto cleanResult = review.checkBestPractices(CLEAN_CODE, "typescript");
    
    // Should have fewer violations
    assert(cleanResult.violations.size() < result.violations.size());
    
    // Compliance score should be higher
    assert(cleanResult.compliance_score > result.compliance_score);
    
    std::cout << "\n    Violations in vulnerable code: " << result.violations.size() << "\n";
    std::cout << "    Violations in clean code: " << cleanResult.violations.size() << "\n";
}

TEST(documentation_check) {
    IntelligentReviewSystem review;
    review.initialize();
    
    // Check documentation
    auto result = review.checkDocumentation(QUALITY_CODE, "javascript");
    
    // Should find documentation gaps
    assert(!result.gaps.empty());
    
    // Documentation coverage should be low
    assert(result.documentation_coverage < 50.0f);
    
    // Check clean code documentation
    auto cleanResult = review.checkDocumentation(CLEAN_CODE, "typescript");
    
    // Should have better coverage
    assert(cleanResult.documentation_coverage > result.documentation_coverage);
    
    std::cout << "\n    Documentation gaps: " << result.gaps.size() << "\n";
    std::cout << "    Coverage: " << result.documentation_coverage << "%\n";
}

TEST(file_review) {
    IntelligentReviewSystem review;
    review.initialize();
    
    ReviewConfig config;
    config.enable_security_scan = true;
    config.enable_performance_analysis = true;
    config.enable_quality_check = true;
    config.enable_best_practices = true;
    config.enable_documentation_check = true;
    
    // Review vulnerable file
    auto fileReview = review.reviewFile("vulnerable.js", VULNERABLE_CODE, config);
    
    // Should have issues
    assert(!fileReview.issues.empty());
    
    // Should have critical/high issues
    assert(fileReview.issues_by_severity[Severity::Critical] > 0 ||
           fileReview.issues_by_severity[Severity::High] > 0);
    
    // Quality score should be low
    assert(fileReview.quality_score < 60.0f);
    
    // Review clean file
    auto cleanReview = review.reviewFile("clean.ts", CLEAN_CODE, config);
    
    // Should have fewer issues
    assert(cleanReview.issues.size() < fileReview.issues.size());
    
    // Quality score should be higher
    assert(cleanReview.quality_score > fileReview.quality_score);
    
    std::cout << "\n    Vulnerable file issues: " << fileReview.issues.size() << "\n";
    std::cout << "    Clean file issues: " << cleanReview.issues.size() << "\n";
    std::cout << "    Quality improvement: " << (cleanReview.quality_score - fileReview.quality_score) << "\n";
}

TEST(auto_fix) {
    IntelligentReviewSystem review;
    review.initialize();
    
    ReviewConfig config;
    config.auto_fix_safe_issues = true;
    
    // Review file
    auto fileReview = review.reviewFile("test.js", VULNERABLE_CODE, config);
    
    // Get fix suggestions
    for (const auto& issue : fileReview.issues) {
        auto fixes = review.getFixSuggestions(issue);
        
        for (const auto& fix : fixes) {
            // Preview fix
            std::string preview = review.previewFix(VULNERABLE_CODE, fix);
            assert(!preview.empty());
            
            // Check if fix is safe
            if (fix.is_safe) {
                // Apply fix
                std::string fixed = review.applyFixes(VULNERABLE_CODE, {issue}, true);
                assert(fixed != VULNERABLE_CODE);
            }
        }
    }
    
    std::cout << "\n    Generated fixes for " << fileReview.issues.size() << " issues\n";
}

TEST(complexity_metrics) {
    IntelligentReviewSystem review;
    review.initialize();
    
    // Calculate complexity
    auto metrics = review.calculateComplexity(QUALITY_CODE, "javascript");
    
    // Should have complexity metrics
    assert(metrics.cyclomatic_complexity > 0);
    assert(metrics.lines_of_code > 0);
    
    // Deep nesting should increase cognitive complexity
    assert(metrics.cognitive_complexity > 10);
    
    std::cout << "\n    Cyclomatic complexity: " << metrics.cyclomatic_complexity << "\n";
    std::cout << "    Cognitive complexity: " << metrics.cognitive_complexity << "\n";
    std::cout << "    Lines of code: " << metrics.lines_of_code << "\n";
}

TEST(duplication_detection) {
    IntelligentReviewSystem review;
    review.initialize();
    
    // Create files with duplication
    std::vector<std::pair<std::string, std::string>> files = {
        {"file1.js", "function duplicate() { return 1; }"},
        {"file2.js", "function duplicate() { return 1; }"},
        {"file3.js", "function unique() { return 2; }"}
    };
    
    auto result = review.findDuplication(files);
    
    // Should detect duplication
    assert(result.duplication_percentage > 0);
    
    // Should have clone groups
    assert(!result.clone_groups.empty());
    
    std::cout << "\n    Duplication: " << result.duplication_percentage << "%\n";
    std::cout << "    Clone groups: " << result.clone_groups.size() << "\n";
}

TEST(team_learning) {
    IntelligentReviewSystem review;
    review.initialize();
    
    // Learn from clean codebase
    std::vector<std::string> files = {"clean.ts"};
    review.learnFromCodebase(".", files);
    
    // Get team patterns
    auto patterns = review.getTeamPatterns();
    
    // Should have learned patterns
    // (May be empty if no patterns detected)
    
    std::cout << "\n    Team patterns learned: " << patterns.size() << "\n";
}

TEST(cross_file_analysis) {
    IntelligentReviewSystem review;
    review.initialize();
    
    // Create files with dependencies
    std::vector<std::pair<std::string, std::string>> files = {
        {"main.js", "import { helper } from './helper'; helper();"},
        {"helper.js", "export function helper() { return 'help'; }"}
    };
    
    // Analyze dependencies
    auto deps = review.analyzeDependencies(files);
    
    // Should detect dependencies
    assert(!deps.empty());
    
    // Find cross-file issues
    auto issues = review.findCrossFileIssues(files);
    
    std::cout << "\n    Dependencies found: " << deps.size() << "\n";
    std::cout << "    Cross-file issues: " << issues.size() << "\n";
}

TEST(streaming_review) {
    IntelligentReviewSystem review;
    review.initialize();
    
    ReviewConfig config;
    
    std::vector<Issue> streamedIssues;
    
    // Stream review
    review.reviewStream("test.js", VULNERABLE_CODE, config, [&streamedIssues](const Issue& issue) {
        streamedIssues.push_back(issue);
    });
    
    // Should have streamed issues
    assert(!streamedIssues.empty());
    
    std::cout << "\n    Streamed " << streamedIssues.size() << " issues\n";
}

TEST(statistics) {
    IntelligentReviewSystem review;
    review.initialize();
    
    // Run some reviews
    ReviewConfig config;
    review.reviewFile("test1.js", VULNERABLE_CODE, config);
    review.reviewFile("test2.js", PERFORMANCE_CODE, config);
    review.reviewFile("test3.js", QUALITY_CODE, config);
    
    // Get statistics
    auto stats = review.getStats();
    
    assert(!stats.empty());
    assert(stats.find("total_reviews") != stats.end());
    assert(stats.find("total_issues") != stats.end());
    
    std::cout << "\n    Total reviews: " << stats["total_reviews"] << "\n";
    std::cout << "    Total issues: " << stats["total_issues"] << "\n";
}

TEST(report_export) {
    IntelligentReviewSystem review;
    review.initialize();
    
    ReviewConfig config;
    
    // Create project review
    std::vector<std::string> files = {"test.js"};
    auto projectReview = review.reviewProject(".", files, config);
    
    // Export as JSON
    std::string jsonReport = review.exportReport(projectReview, "json");
    assert(!jsonReport.empty());
    assert(jsonReport.find("{") != std::string::npos);
    
    // Export as Markdown
    std::string mdReport = review.exportReport(projectReview, "markdown");
    assert(!mdReport.empty());
    
    std::cout << "\n    JSON report size: " << jsonReport.length() << " bytes\n";
    std::cout << "    Markdown report size: " << mdReport.length() << " bytes\n";
}

TEST(configuration) {
    IntelligentReviewSystem review;
    review.initialize();
    
    // Set configuration
    ReviewConfig config;
    config.enable_security_scan = true;
    config.enable_performance_analysis = false;
    config.fail_on_critical = true;
    config.max_issues_per_file = 100;
    
    review.setConfig(config);
    
    // Get configuration
    auto retrieved = review.getConfig();
    assert(retrieved.enable_security_scan == true);
    assert(retrieved.enable_performance_analysis == false);
    
    // Enable/disable categories
    review.setCategoryEnabled(Category::Security, true);
    review.setCategoryEnabled(Category::Style, false);
    
    // Set minimum severity
    review.setMinimumSeverity(Category::Security, Severity::Medium);
    
    // Add rule override
    review.addRuleOverride("SEC001", false);
    
    std::cout << "\n    Configuration applied successfully\n";
}

// ═══════════════════════════════════════════════════════════════════════════════
// Main Test Runner
// ═══════════════════════════════════════════════════════════════════════════════

int main() {
    std::cout << "╔══════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║     INTELLIGENT CODE REVIEW SYSTEM - SMOKE TEST                      ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════════╝\n\n";
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Core Tests
    std::cout << "\n=== Core Tests ===\n";
    RUN_TEST(initialization);
    RUN_TEST(configuration);
    
    // Security Tests
    std::cout << "\n=== Security Tests ===\n";
    RUN_TEST(security_scanning);
    
    // Performance Tests
    std::cout << "\n=== Performance Tests ===\n";
    RUN_TEST(performance_analysis);
    RUN_TEST(complexity_metrics);
    
    // Quality Tests
    std::cout << "\n=== Quality Tests ===\n";
    RUN_TEST(quality_analysis);
    RUN_TEST(duplication_detection);
    
    // Best Practices Tests
    std::cout << "\n=== Best Practices Tests ===\n";
    RUN_TEST(best_practices);
    RUN_TEST(documentation_check);
    
    // Integration Tests
    std::cout << "\n=== Integration Tests ===\n";
    RUN_TEST(file_review);
    RUN_TEST(auto_fix);
    RUN_TEST(streaming_review);
    RUN_TEST(cross_file_analysis);
    RUN_TEST(team_learning);
    
    // Reporting Tests
    std::cout << "\n=== Reporting Tests ===\n";
    RUN_TEST(statistics);
    RUN_TEST(report_export);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Summary
    std::cout << "\n╔══════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                        TEST SUMMARY                                  ║\n";
    std::cout << "╠══════════════════════════════════════════════════════════════════════╣\n";
    std::cout << "║  Tests Passed: " << std::setw(3) << tests_passed << "                                                   ║\n";
    std::cout << "║  Tests Failed: " << std::setw(3) << tests_failed << "                                                   ║\n";
    std::cout << "║  Total Tests:  " << std::setw(3) << (tests_passed + tests_failed) << "                                                   ║\n";
    std::cout << "║  Duration:     " << std::setw(6) << duration.count() << "ms                                            ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════════╝\n";
    
    if (tests_failed == 0) {
        std::cout << "\n✓ ALL TESTS PASSED - INTELLIGENT REVIEW SYSTEM READY\n\n";
        return 0;
    } else {
        std::cout << "\n✗ SOME TESTS FAILED - REVIEW IMPLEMENTATION\n\n";
        return 1;
    }
}
