// ================================================================
// Circuit Breaker - Fault tolerance and error recovery
// ================================================================
#include <windows.h>
#include <atomic>

class CircuitBreaker {
public:
    CircuitBreaker(int threshold = 5) 
        : failure_threshold(threshold)
        , failure_count(0)
        , is_open(false)
    {}
    
    bool allow_request() {
        if (is_open.load()) {
            return false;
        }
        return true;
    }
    
    void record_success() {
        failure_count.store(0);
        is_open.store(false);
    }
    
    void record_failure() {
        int count = failure_count.fetch_add(1) + 1;
        if (count >= failure_threshold) {
            is_open.store(true);
        }
    }
    
    void reset() {
        failure_count.store(0);
        is_open.store(false);
    }
    
private:
    int failure_threshold;
    std::atomic<int> failure_count;
    std::atomic<bool> is_open;
};
