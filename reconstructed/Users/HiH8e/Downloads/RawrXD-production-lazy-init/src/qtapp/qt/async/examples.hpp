#ifndef QT_ASYNC_EXAMPLES_HPP
#define QT_ASYNC_EXAMPLES_HPP

#include "qt_async_file_operations.hpp"
#include <QString>
#include <QDebug>
#include <iostream>
#include <chrono>

// =============================================================================
// Qt Async Operations - Working Examples
// =============================================================================
// All examples demonstrate practical async file operations with callbacks
// =============================================================================

/**
 * Example 1: Basic Asynchronous File Read
 * Demonstrates: Simple file read with completion callback
 */
inline void example1_basic_async_read() {
    qDebug() << "=== Example 1: Basic Async File Read ===";
    
    QtAsyncFileOps async_ops;
    async_ops.startThreadPool();
    
    // Perform async read
    uint64_t work_id = async_ops.readFileAsync(
        "/path/to/file.txt",
        [](const AsyncResult& result) {
            if (result.success) {
                qDebug() << "Read complete:"
                        << result.bytes_processed << "bytes"
                        << "in" << result.elapsed_ms << "ms";
            } else {
                qDebug() << "Read failed:" << result.error_message;
            }
        }
    );
    
    qDebug() << "Work ID:" << work_id;
    qDebug() << "Operation queued for execution...";
}

/**
 * Example 2: File Read with Progress Tracking
 * Demonstrates: Monitoring progress of large file read
 */
inline void example2_async_read_with_progress() {
    qDebug() << "\n=== Example 2: Async Read with Progress ===";
    
    QtAsyncFileOps async_ops;
    async_ops.startThreadPool();
    
    // Read with progress callback
    uint64_t work_id = async_ops.readFileAsync(
        "/path/to/large_file.bin",
        // Completion callback
        [](const AsyncResult& result) {
            qDebug() << "===== READ COMPLETE =====";
            if (result.success) {
                qDebug() << "Success: Read"
                        << result.bytes_processed << "bytes"
                        << "in" << result.elapsed_ms << "ms";
                
                double throughput = (result.bytes_processed / 1024.0 / 1024.0) /
                                   (result.elapsed_ms / 1000.0);
                qDebug() << "Throughput:" << throughput << "MB/s";
            } else {
                qDebug() << "Failed:" << result.error_message;
            }
        },
        // Progress callback
        [](const AsyncProgress& progress) {
            int pct = progress.percentage;
            uint64_t mb_done = progress.bytes_processed / 1024 / 1024;
            uint64_t mb_total = progress.total_bytes / 1024 / 1024;
            
            qDebug() << "Progress:" << pct << "% ("
                    << mb_done << "/" << mb_total << " MB)";
        },
        // Chunk size (128KB for frequent updates)
        131072
    );
    
    qDebug() << "Async read initiated, work ID:" << work_id;
}

/**
 * Example 3: Asynchronous File Write
 * Demonstrates: Writing data asynchronously with callback
 */
inline void example3_async_file_write() {
    qDebug() << "\n=== Example 3: Async File Write ===";
    
    QtAsyncFileOps async_ops;
    async_ops.startThreadPool();
    
    // Prepare data
    QByteArray data;
    for (int i = 0; i < 10000; i++) {
        data.append(QString("Line %1: Hello World\n").arg(i).toUtf8());
    }
    
    qDebug() << "Writing" << data.size() << "bytes...";
    
    // Perform async write
    uint64_t work_id = async_ops.writeFileAsync(
        "/path/to/output.txt",
        data,
        [](const AsyncResult& result) {
            if (result.success) {
                qDebug() << "Write complete:"
                        << result.bytes_processed << "bytes"
                        << "in" << result.elapsed_ms << "ms";
            } else {
                qDebug() << "Write failed:" << result.error_message;
            }
        }
    );
    
    qDebug() << "Write queued, work ID:" << work_id;
}

/**
 * Example 4: Asynchronous File Copy
 * Demonstrates: Copying large file with progress
 */
inline void example4_async_file_copy() {
    qDebug() << "\n=== Example 4: Async File Copy ===";
    
    QtAsyncFileOps async_ops;
    async_ops.startThreadPool();
    
    // Copy with progress
    uint64_t work_id = async_ops.copyFileAsync(
        "/path/to/source.iso",
        "/path/to/destination.iso",
        // Completion callback
        [](const AsyncResult& result) {
            if (result.success) {
                qDebug() << "Copy complete:"
                        << result.bytes_processed << "bytes"
                        << "in" << result.elapsed_ms << "ms";
            } else {
                qDebug() << "Copy failed:" << result.error_message;
            }
        },
        // Progress callback for status
        [](const AsyncProgress& progress) {
            qDebug() << "Copy progress:" << progress.percentage << "%";
            
            if (progress.elapsed_ms > 0) {
                double throughput = (progress.bytes_processed / 1024.0 / 1024.0) /
                                   (progress.elapsed_ms / 1000.0);
                qDebug() << "  Throughput:" << throughput << "MB/s";
            }
            
            if (progress.estimated_remaining_ms > 0) {
                int remaining_sec = progress.estimated_remaining_ms / 1000;
                qDebug() << "  ETA:" << remaining_sec << "seconds";
            }
        }
    );
    
    qDebug() << "File copy initiated, work ID:" << work_id;
}

/**
 * Example 5: Cancellation of Async Operation
 * Demonstrates: Cancelling a long-running operation
 */
inline void example5_async_cancellation() {
    qDebug() << "\n=== Example 5: Async Operation Cancellation ===";
    
    QtAsyncFileOps async_ops;
    async_ops.startThreadPool();
    
    // Start long operation
    uint64_t work_id = async_ops.readFileAsync(
        "/path/to/huge_file.bin",
        [](const AsyncResult& result) {
            if (result.success) {
                qDebug() << "Read completed successfully";
            } else {
                qDebug() << "Operation failed/cancelled:" << result.error_message;
            }
        },
        [](const AsyncProgress& progress) {
            qDebug() << "Progress:" << progress.percentage << "%";
        }
    );
    
    qDebug() << "Operation started with ID:" << work_id;
    
    // Simulate user deciding to cancel after seeing progress
    // In real code, this would be triggered by user action (button click, etc.)
    qDebug() << "Initiating cancellation...";
    
    bool cancel_initiated = async_ops.cancelOperation(work_id);
    if (cancel_initiated) {
        qDebug() << "Cancellation signal sent to worker thread";
    } else {
        qDebug() << "Could not cancel (operation may have already completed)";
    }
}

/**
 * Example 6: Multiple Concurrent Operations
 * Demonstrates: Handling multiple async operations simultaneously
 */
inline void example6_concurrent_operations() {
    qDebug() << "\n=== Example 6: Multiple Concurrent Operations ===";
    
    QtAsyncFileOps async_ops;
    async_ops.startThreadPool();
    
    QList<uint64_t> work_ids;
    
    // Queue multiple read operations
    for (int i = 0; i < 3; i++) {
        uint64_t id = async_ops.readFileAsync(
            QString("/path/to/file%1.txt").arg(i),
            [i](const AsyncResult& result) {
                if (result.success) {
                    qDebug() << "File" << i << "read:"
                            << result.bytes_processed << "bytes";
                } else {
                    qDebug() << "File" << i << "read failed:"
                            << result.error_message;
                }
            }
        );
        
        work_ids.append(id);
        qDebug() << "Queued file" << i << "with work ID:" << id;
    }
    
    // Monitor pending operations
    qDebug() << "Total pending operations:" << async_ops.getPendingOperationCount();
    
    // Wait for all to complete (with timeout)
    for (uint64_t id : work_ids) {
        AsyncResult result = async_ops.waitForOperation(id, 30000);  // 30 second timeout
        qDebug() << "Operation" << id << "completed with status:"
                << (result.success ? "Success" : "Failed");
    }
    
    qDebug() << "All operations completed";
}

/**
 * Example 7: Qt Signal Integration
 * Demonstrates: Using Qt signals for status updates
 */
inline void example7_qt_signals() {
    qDebug() << "\n=== Example 7: Qt Signal Integration ===";
    
    QtAsyncFileOps* async_ops = new QtAsyncFileOps();
    async_ops->startThreadPool();
    
    // Connect to signals
    QObject::connect(
        async_ops, &QtAsyncFileOps::operationComplete,
        [](uint64_t work_id, const AsyncResult& result) {
            qDebug() << "SIGNAL: Operation" << work_id << "completed"
                    << (result.success ? "successfully" : "with error");
        }
    );
    
    QObject::connect(
        async_ops, &QtAsyncFileOps::operationProgress,
        [](uint64_t work_id, const AsyncProgress& progress) {
            qDebug() << "SIGNAL: Operation" << work_id
                    << "progress:" << progress.percentage << "%";
        }
    );
    
    QObject::connect(
        async_ops, &QtAsyncFileOps::operationError,
        [](uint64_t work_id, AsyncErrorCode code, const QString& message) {
            qDebug() << "SIGNAL: Operation" << work_id << "error:"
                    << asyncErrorCodeString(code) << "-" << message;
        }
    );
    
    // Queue operation
    async_ops->readFileAsync(
        "/path/to/file.txt",
        [](const AsyncResult&) {}  // Callback (also receives signal)
    );
    
    qDebug() << "Operations queued; signals will be emitted...";
}

/**
 * Example 8: Error Handling and Recovery
 * Demonstrates: Handling errors gracefully
 */
inline void example8_error_handling() {
    qDebug() << "\n=== Example 8: Error Handling & Recovery ===";
    
    QtAsyncFileOps async_ops;
    async_ops.startThreadPool();
    
    // Attempt to read non-existent file
    uint64_t work_id = async_ops.readFileAsync(
        "/path/to/nonexistent_file.txt",
        [](const AsyncResult& result) {
            if (result.success) {
                qDebug() << "Unexpected success!";
            } else {
                qDebug() << "Expected error occurred:";
                qDebug() << "  Error Code:" << result.error_code;
                qDebug() << "  Error Message:" << result.error_message;
                qDebug() << "  Handling...";
                
                // Recovery logic
                qDebug() << "  Creating file with default content...";
            }
        }
    );
    
    qDebug() << "Read operation queued; error will be handled...";
}

/**
 * Example 9: Performance Benchmarking
 * Demonstrates: Measuring throughput and latency
 */
inline void example9_performance_benchmark() {
    qDebug() << "\n=== Example 9: Performance Benchmarking ===";
    
    QtAsyncFileOps async_ops;
    async_ops.startThreadPool();
    
    // Create test data
    QByteArray test_data;
    test_data.resize(10 * 1024 * 1024);  // 10 MB
    for (int i = 0; i < test_data.size(); i += 4) {
        *(uint32_t*)(test_data.data() + i) = i;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    uint64_t write_id = async_ops.writeFileAsync(
        "/tmp/benchmark_write.bin",
        test_data,
        [start_time](const AsyncResult& result) {
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_time - start_time);
            
            if (result.success) {
                double mb = result.bytes_processed / 1024.0 / 1024.0;
                double sec = result.elapsed_ms / 1000.0;
                double throughput = mb / sec;
                
                qDebug() << "Write benchmark:";
                qDebug() << "  Size:" << mb << "MB";
                qDebug() << "  Time:" << result.elapsed_ms << "ms";
                qDebug() << "  Throughput:" << throughput << "MB/s";
            }
        }
    );
    
    qDebug() << "Benchmark operation queued...";
}

/**
 * Example 10: Read with Partial Reads
 * Demonstrates: Reading specific portions of files
 */
inline void example10_partial_read() {
    qDebug() << "\n=== Example 10: Partial File Read ===";
    
    QtAsyncFileOps async_ops;
    async_ops.startThreadPool();
    
    // Read from offset 1000, size 5000
    uint64_t work_id = async_ops.readFilePartAsync(
        "/path/to/large_file.bin",
        1000,           // offset
        5000,           // size
        [](const AsyncResult& result) {
            if (result.success) {
                qDebug() << "Partial read complete:"
                        << result.bytes_processed << "bytes";
            } else {
                qDebug() << "Partial read failed:" << result.error_message;
            }
        }
    );
    
    qDebug() << "Partial read initiated, work ID:" << work_id;
}

/**
 * Run all examples
 */
inline void run_all_async_examples() {
    qDebug() << "========================================";
    qDebug() << "Qt Async File Operations - All Examples";
    qDebug() << "========================================";
    
    example1_basic_async_read();
    example2_async_read_with_progress();
    example3_async_file_write();
    example4_async_file_copy();
    example5_async_cancellation();
    example6_concurrent_operations();
    example7_qt_signals();
    example8_error_handling();
    example9_performance_benchmarking();
    example10_partial_read();
    
    qDebug() << "========================================";
    qDebug() << "All examples demonstrated";
    qDebug() << "========================================";
}

#endif // QT_ASYNC_EXAMPLES_HPP
