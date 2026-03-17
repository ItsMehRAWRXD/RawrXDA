#pragma once
#include <string>
#include <iostream>

class Metrics {
public:
    virtual ~Metrics() = default;
    
    virtual void recordHistogram(const std::string& name, double value) {
        // Placeholder for metrics recording
    }
    
    virtual void incrementCounter(const std::string& name, int count = 1) {
        // Placeholder for counter increment
    }
    
    virtual void recordGauge(const std::string& name, double value) {
        // Placeholder for gauge
    }
};
