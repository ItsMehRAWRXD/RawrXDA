#include "maximus_streamer.h"
#include <iostream>

using namespace Maximus;

// =============================================================================
// EXAMPLE 1: Simplest possible usage
// =============================================================================

void example_simple() {
    auto streamer = makeStreamer();
    
    streamer->stream(
        "Explain quantum computing",
        [](const Token& tok) {
            std::cout << tok.text;  // Just print
        },
        []() {
            std::cout << "\n\n[DONE]\n";
        }
    );
    
    // Wait for completion
    while (streamer->isCancelled() == false && streamer->contextTokenCount() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// =============================================================================
// EXAMPLE 2: Semantic streaming - complete thoughts
// =============================================================================

void example_semantic() {
    auto streamer = StreamerBuilder()
        .bySentence()
        .byThought()
        .byCodeBlock()
        .highThroughput()
        .build();
    
    streamer->streamSemantic(
        "Write a Python function to sort a list and explain it",
        [](std::string_view sentence) {
            std::cout << sentence << "\n";
            std::cout << "--- sentence boundary ---\n";
        },
        [](std::string_view thought, float confidence) {
            std::cout << "\n[Thought: " << thought << "]\n";
            std::cout << "(confidence: " << (confidence * 100) << "%)\n\n";
        },
        [](std::string_view code, const std::string& lang) {
            std::cout << "\n```" << lang << "\n" << code << "\n```\n\n";
        }
    );
}

// =============================================================================
// EXAMPLE 3: Time-travel debugging
// =============================================================================

void example_timetravel() {
    auto streamer = StreamerBuilder()
        .checkpoints(true, 50)  // Every 50 tokens
        .build();
    
    std::vector<Checkpoint> checkpoints;
    
    streamer->stream(
        "Write a long story",
        [&](const Token& tok) {
            std::cout << tok.text;
            
            // Capture checkpoint every 100 tokens
            if (streamer->contextTokenCount() % 100 == 0) {
                checkpoints.push_back(streamer->checkpoint());
                std::cout << "\n[CHECKPOINT " << checkpoints.size() << "]\n";
            }
        }
    );
    
    // Later: rewind and try different generation
    std::cout << "\n\n--- Rewinding to checkpoint 2 ---\n";
    
    if (streamer->rewind(checkpoints[1])) {
        std::cout << "Rewound to position " << checkpoints[1].position << "\n";
        std::cout << "Text so far: " << streamer->text() << "\n";
        
        // Continue with different temperature or seed
        // streamer->continueGeneration(differentParams);
    }
}

// =============================================================================
// EXAMPLE 4: Backpressure-aware streaming
// =============================================================================

void example_backpressure() {
    auto streamer = StreamerBuilder()
        .backpressure(true, 100)  // Max 100 tokens ahead
        .build();
    
    std::atomic<uint32_t> processed{0};
    
    streamer->stream(
        "Generate a long explanation",
        [&](const Token& tok) {
            // Simulate slow consumer
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            
            std::cout << tok.text;
            processed++;
            
            // Tell streamer we're ready for more
            streamer->requestMore(10);
        }
    );
    
    // Streamer automatically slows down to match consumer
}

// =============================================================================
// EXAMPLE 5: Metrics and observability
// =============================================================================

void example_metrics() {
    auto streamer = makeStreamer();
    
    streamer->setMetricsCallback([](const StreamMetrics& m) {
        std::cout << "\r" << std::string(80, ' ') << "\r";  // Clear line
        std::cout << std::fixed << std::setprecision(1)
                  << m.tokensPerSecond << " TPS | "
                  << (m.averageConfidence * 100) << "% conf | "
                  << "first token: " << m.firstTokenLatency.count() << "us";
        std::cout.flush();
    }, std::chrono::milliseconds(100));
    
    streamer->stream(
        "Explain machine learning",
        [](const Token& tok) {
            std::cout << tok.text;
        }
    );
    
    std::cout << "\n\nFinal metrics:\n";
    auto m = streamer->metrics();
    std::cout << "Total tokens: " << m.totalTokens << "\n";
    std::cout << "Average TPS: " << m.tokensPerSecond << "\n";
    std::cout << "Peak memory: " << (m.peakMemoryBytes / 1024) << " KB\n";
}

// =============================================================================
// EXAMPLE 6: Iterator-style (modern C++)
// =============================================================================

void example_iterator() {
    auto streamer = makeStreamer();
    
    // Start streaming in background
    std::thread t([&]() {
        streamer->stream("Count from 1 to 10", [](const Token&) {});
    });
    
    // Iterate over tokens
    for (auto it = streamer->begin(); it != streamer->end(); ++it) {
        Token tok = *it;
        std::cout << tok.text << " [" << tok.confidence << "]\n";
        
        if (it.done()) break;
    }
    
    t.join();
}

// =============================================================================
// EXAMPLE 7: Confidence filtering
// =============================================================================

void example_confidence_filter() {
    auto streamer = StreamerBuilder()
        .minConfidence(0.8f)  // Only show confident tokens
        .build();
    
    streamer->stream(
        "Translate 'hello world' to 5 languages",
        [](const Token& tok) {
            if (tok.confidence > 0.8f) {
                std::cout << tok.text;
            } else {
                std::cout << "[" << tok.text << "?]";  // Mark uncertain
            }
        }
    );
}

// =============================================================================
// MAIN
// =============================================================================

int main() {
    std::cout << "=== Maximus TPS Streamer Demo ===\n\n";
    
    std::cout << "1. Simple streaming\n";
    example_simple();
    
    std::cout << "\n\n2. Semantic streaming\n";
    // example_semantic();
    
    std::cout << "\n\n3. Time-travel\n";
    // example_timetravel();
    
    return 0;
}
