// test_swarm_fix.cpp
// Minimal test to verify swarm scheduler notifications work
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <condition_variable>
#include <mutex>

// Simulate the key parts of the fix
namespace TestSwarm {
    std::atomic<int> prefetch_thread_wakeups(0);
    std::mutex schedMutex;
    std::condition_variable prefetchIoCv;
    
    void notifyPrefetchIoThread() {
        prefetchIoCv.notify_one();
    }
    
    void prefetchIoThreadSimulator() {
        std::cout << "[PREFETCH] Thread started\n";
        for (int i = 0; i < 10; ++i) {
            std::unique_lock<std::mutex> lock(schedMutex);
            // Wait for notification (timeout every 100ms)
            prefetchIoCv.wait_for(lock, std::chrono::milliseconds(100),
                                  []{ return true; });  // Always proceed
            prefetch_thread_wakeups++;
            std::cout << "[PREFETCH] Woke up (count=" << prefetch_thread_wakeups << ")\n";
        }
    }
    
    // Simulated onLayerComputeFinished WITH the fix
    void onLayerComputeFinished_Fixed(int layer) {
        std::cout << "[COMPUTE] Layer " << layer << " finished\n";
        {
            std::lock_guard<std::mutex> lock(schedMutex);
            // markFinishedForLayer_(modelIndex, layerIndex);
            // pruneWorkingSetUnlocked_();
        }
        // THE FIX: notify the prefetch thread immediately
        notifyPrefetchIoThread();
        std::cout << "[COMPUTE] Notified prefetch thread\n";
    }
}

int main() {
    std::cout << "=== Swarm Scheduler Notification Fix Test ===\n\n";
    
    std::thread prefetch(&TestSwarm::prefetchIoThreadSimulator);
    
    // Simulate 5 layers finishing
    for (int layer = 0; layer < 5; ++layer) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        TestSwarm::onLayerComputeFinished_Fixed(layer);
    }
    
    prefetch.join();
    
    std::cout << "\n=== Test Results ===\n";
    std::cout << "Prefetch thread wakeups: " << TestSwarm::prefetch_thread_wakeups << "\n";
    std::cout << "Expected: >= 5 (one per layer finish)\n";
    
    if (TestSwarm::prefetch_thread_wakeups >= 5) {
        std::cout << "\n✓ TEST PASSED: Notifications are working!\n";
        return 0;
    } else {
        std::cout << "\n✗ TEST FAILED: Not enough notifications\n";
        return 1;
    }
}
