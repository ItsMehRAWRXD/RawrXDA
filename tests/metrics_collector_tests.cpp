// ============================================================================
// Metrics Collector Tests — Analytics Testing
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include "../src/metrics/metrics_collector.cpp"

using namespace RawrXD::Metrics;

// Mock Session Manager
class MockMetricsSessionManager : public Core::SessionManager {
public:
    void SetValue(const std::string& key, const std::string& value) override {}
    std::string GetValue(const std::string& key) override { return ""; }
};

TEST_CASE("Metrics Collector - Basic Operations", "[metrics][analytics]") {
    auto sessionManager = std::make_shared<MockMetricsSessionManager>();
    MetricsCollector collector(sessionManager);
    
    SECTION("Empty metrics state") {
        auto stats = collector.GetStats();
        REQUIRE(stats.totalMetrics == 0);
        REQUIRE(stats.avgCollectionTime == 0.0);
    }
    
    SECTION("Counter metric") {
        collector.IncrementCounter("requests", {{"endpoint", "/api/v1/users"}});
        collector.IncrementCounter("requests", {{"endpoint", "/api/v1/users"}});
        collector.IncrementCounter("requests", {{"endpoint", "/api/v1/users"}});
        
        auto timeSeries = collector.GetTimeSeries("requests", 
            std::chrono::system_clock::now() - std::chrono::hours(1),
            std::chrono::system_clock::now()
        );
        
        REQUIRE(timeSeries.values.size() == 3);
    }
    
    SECTION("Gauge metric") {
        collector.SetGauge("memory_usage", 1024.5, {{"type", "heap"}});
        collector.SetGauge("memory_usage", 2048.0, {{"type", "heap"}});
        
        auto timeSeries = collector.GetTimeSeries("memory_usage",
            std::chrono::system_clock::now() - std::chrono::hours(1),
            std::chrono::system_clock::now()
        );
        
        REQUIRE(timeSeries.values.size() == 2);
    }
}

TEST_CASE("Metrics Collector - Time Series", "[metrics][time-series]") {
    auto sessionManager = std::make_shared<MockMetricsSessionManager>();
    MetricsCollector collector(sessionManager);
    
    SECTION("Record and retrieve time series") {
        auto now = std::chrono::system_clock::now();
        
        collector.RecordHistogram("response_time", 150.5);
        collector.RecordHistogram("response_time", 120.3);
        collector.RecordHistogram("response_time", 180.0);
        collector.RecordHistogram("response_time", 95.2);
        
        auto timeSeries = collector.GetTimeSeries("response_time",
            now - std::chrono::hours(1),
            now + std::chrono::hours(1)
        );
        
        REQUIRE(timeSeries.values.size() == 4);
        REQUIRE(timeSeries.metricName == "response_time");
    }
    
    SECTION("Aggregation results") {
        auto now = std::chrono::system_clock::now();
        
        collector.RecordHistogram("latency", 100.0);
        collector.RecordHistogram("latency", 200.0);
        collector.RecordHistogram("latency", 300.0);
        collector.RecordHistogram("latency", 400.0);
        collector.RecordHistogram("latency", 500.0);
        
        auto result = collector.Aggregate("latency",
            now - std::chrono::hours(1),
            now + std::chrono::hours(1)
        );
        
        REQUIRE(result.metricName == "latency");
        REQUIRE(result.min == 100.0);
        REQUIRE(result.max == 500.0);
        REQUIRE(result.avg == 300.0);
        REQUIRE(result.count == 5);
        REQUIRE(result.p50 == 300.0);
        REQUIRE(result.p95 == 500.0);
    }
}

TEST_CASE("Metrics Collector - Alert Rules", "[metrics][alerts]") {
    auto sessionManager = std::make_shared<MockMetricsSessionManager>();
    MetricsCollector collector(sessionManager);
    
    SECTION("Add alert rule") {
        AlertRule rule;
        rule.id = "rule-1";
        rule.metricName = "cpu_usage";
        rule.condition = ">";
        rule.threshold = 80.0;
        rule.severity = "warning";
        rule.isActive = true;
        
        collector.AddAlertRule(rule);
        
        auto activeAlerts = collector.GetActiveAlerts();
        REQUIRE(activeAlerts.size() == 1);
        REQUIRE(activeAlerts[0].metricName == "cpu_usage");
    }
    
    SECTION("Multiple alert rules") {
        AlertRule rule1;
        rule1.id = "rule-1";
        rule1.metricName = "memory_usage";
        rule1.condition = ">";
        rule1.threshold = 90.0;
        rule1.isActive = true;
        
        AlertRule rule2;
        rule2.id = "rule-2";
        rule2.metricName = "disk_usage";
        rule2.condition = ">";
        rule2.threshold = 95.0;
        rule2.isActive = true;
        
        AlertRule rule3;
        rule3.id = "rule-3";
        rule3.metricName = "network_latency";
        rule3.condition = ">";
        rule3.threshold = 1000.0;
        rule3.isActive = false; // Inactive
        
        collector.AddAlertRule(rule1);
        collector.AddAlertRule(rule2);
        collector.AddAlertRule(rule3);
        
        auto activeAlerts = collector.GetActiveAlerts();
        REQUIRE(activeAlerts.size() == 2);
    }
    
    SECTION("Remove alert rule") {
        AlertRule rule;
        rule.id = "removable-rule";
        rule.metricName = "test_metric";
        rule.isActive = true;
        
        collector.AddAlertRule(rule);
        REQUIRE(collector.GetActiveAlerts().size() == 1);
        
        collector.RemoveAlertRule("removable-rule");
        REQUIRE(collector.GetActiveAlerts().empty());
    }
}

TEST_CASE("Metrics Collector - Report Generation", "[metrics][reporting]") {
    auto sessionManager = std::make_shared<MockMetricsSessionManager>();
    MetricsCollector collector(sessionManager);
    
    SECTION("Generate metrics report") {
        collector.SetGauge("cpu", 75.0);
        collector.SetGauge("memory", 80.0);
        collector.SetGauge("disk", 60.0);
        
        auto start = std::chrono::system_clock::now() - std::chrono::hours(1);
        auto end = std::chrono::system_clock::now();
        
        auto report = collector.GenerateMetricsReport(start, end);
        
        REQUIRE_FALSE(report.empty());
        REQUIRE(report.find("# Metrics Report") != std::string::npos);
    }
}
