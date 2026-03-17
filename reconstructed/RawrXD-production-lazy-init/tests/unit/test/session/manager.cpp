#include <gtest/gtest.h>
#include "../src/masm_stubs.cpp" // Include to test C functions
#include <thread>
#include <vector>
#include <set>

/**
 * @class SessionManagerTest
 * @brief Unit tests for SessionManager (C interface)
 * 
 * Tests cover:
 * - Initialization and shutdown
 * - Session creation with unique IDs
 * - Session destruction
 * - Thread safety with concurrent operations
 * - Resource cleanup
 */

class SessionManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Ensure clean state before each test
        int init_result = session_manager_init();
        ASSERT_EQ(init_result, 1);
        ASSERT_EQ(session_manager_get_session_count(), 0);
    }
    
    void TearDown() override {
        // Cleanup after each test
        session_manager_shutdown();
        ASSERT_EQ(session_manager_get_session_count(), 0);
    }
};

// ============================================================================
// Initialization and Shutdown Tests
// ============================================================================

TEST(SessionManagerInitTest, InitializeSuccess) {
    int result = session_manager_init();
    EXPECT_EQ(result, 1);
    EXPECT_EQ(session_manager_get_session_count(), 0);
    session_manager_shutdown();
}

TEST(SessionManagerInitTest, ShutdownSuccess) {
    session_manager_init();
    EXPECT_EQ(session_manager_get_session_count(), 0);
    
    session_manager_shutdown();
    EXPECT_EQ(session_manager_get_session_count(), 0);
}

TEST(SessionManagerInitTest, MultipleInitialize) {
    session_manager_init();
    // Should handle multiple init calls gracefully
    session_manager_init();
    
    EXPECT_EQ(session_manager_get_session_count(), 0);
    session_manager_shutdown();
}

// ============================================================================
// Session Creation Tests
// ============================================================================

TEST_F(SessionManagerTest, CreateSessionBasic) {
    int session_id = session_manager_create_session("test_session");
    
    EXPECT_GT(session_id, 0);
    EXPECT_EQ(session_manager_get_session_count(), 1);
}

TEST_F(SessionManagerTest, CreateSessionWithEmptyName) {
    int session_id = session_manager_create_session("");
    
    // Should handle empty name
    if (session_id > 0) {
        EXPECT_EQ(session_manager_get_session_count(), 1);
    }
}

TEST_F(SessionManagerTest, CreateMultipleSessions) {
    int id1 = session_manager_create_session("session_1");
    int id2 = session_manager_create_session("session_2");
    int id3 = session_manager_create_session("session_3");
    
    EXPECT_GT(id1, 0);
    EXPECT_GT(id2, 0);
    EXPECT_GT(id3, 0);
    EXPECT_EQ(session_manager_get_session_count(), 3);
}

TEST_F(SessionManagerTest, SessionIDsAreUnique) {
    std::set<int> ids;
    
    for (int i = 0; i < 10; ++i) {
        int id = session_manager_create_session(("session_" + std::to_string(i)).c_str());
        ASSERT_GT(id, 0);
        ids.insert(id);
    }
    
    // All IDs should be unique
    EXPECT_EQ(ids.size(), 10);
    EXPECT_EQ(session_manager_get_session_count(), 10);
}

TEST_F(SessionManagerTest, SessionIDsIncrement) {
    int id1 = session_manager_create_session("first");
    int id2 = session_manager_create_session("second");
    int id3 = session_manager_create_session("third");
    
    // IDs should increment or be monotonic
    EXPECT_LT(id1, id2);
    EXPECT_LT(id2, id3);
}

TEST_F(SessionManagerTest, CreateWithLongName) {
    std::string long_name(1000, 'a');
    int session_id = session_manager_create_session(long_name.c_str());
    
    // Should handle long names
    if (session_id > 0) {
        EXPECT_EQ(session_manager_get_session_count(), 1);
    }
}

// ============================================================================
// Session Destruction Tests
// ============================================================================

TEST_F(SessionManagerTest, DestroySessionSuccess) {
    int id = session_manager_create_session("to_destroy");
    ASSERT_GT(id, 0);
    EXPECT_EQ(session_manager_get_session_count(), 1);
    
    session_manager_destroy_session(id);
    EXPECT_EQ(session_manager_get_session_count(), 0);
}

TEST_F(SessionManagerTest, DestroyNonExistentSession) {
    // Destroying non-existent session should not crash
    session_manager_destroy_session(999);
    EXPECT_EQ(session_manager_get_session_count(), 0);
}

TEST_F(SessionManagerTest, DestroySessionTwice) {
    int id = session_manager_create_session("session");
    
    session_manager_destroy_session(id);
    EXPECT_EQ(session_manager_get_session_count(), 0);
    
    // Destroying again should be safe
    session_manager_destroy_session(id);
    EXPECT_EQ(session_manager_get_session_count(), 0);
}

TEST_F(SessionManagerTest, CreateDestroyMultiple) {
    std::vector<int> ids;
    
    // Create 5 sessions
    for (int i = 0; i < 5; ++i) {
        ids.push_back(session_manager_create_session(("s_" + std::to_string(i)).c_str()));
    }
    
    EXPECT_EQ(session_manager_get_session_count(), 5);
    
    // Destroy 3 of them
    session_manager_destroy_session(ids[0]);
    session_manager_destroy_session(ids[2]);
    session_manager_destroy_session(ids[4]);
    
    EXPECT_EQ(session_manager_get_session_count(), 2);
    
    // Destroy remaining
    session_manager_destroy_session(ids[1]);
    session_manager_destroy_session(ids[3]);
    
    EXPECT_EQ(session_manager_get_session_count(), 0);
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(SessionManagerTest, ConcurrentSessionCreation) {
    std::vector<std::thread> threads;
    std::vector<int> session_ids(10);
    
    // Create 10 sessions from different threads
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&, i]() {
            session_ids[i] = session_manager_create_session(
                ("concurrent_session_" + std::to_string(i)).c_str()
            );
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // All sessions should be created
    EXPECT_EQ(session_manager_get_session_count(), 10);
    
    // All IDs should be valid and unique
    std::set<int> unique_ids(session_ids.begin(), session_ids.end());
    EXPECT_EQ(unique_ids.size(), 10);
}

TEST_F(SessionManagerTest, ConcurrentSessionDestruction) {
    // Create sessions
    std::vector<int> ids;
    for (int i = 0; i < 10; ++i) {
        ids.push_back(session_manager_create_session(("s_" + std::to_string(i)).c_str()));
    }
    
    EXPECT_EQ(session_manager_get_session_count(), 10);
    
    // Destroy concurrently
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&, i]() {
            session_manager_destroy_session(ids[i]);
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // All should be destroyed
    EXPECT_EQ(session_manager_get_session_count(), 0);
}

TEST_F(SessionManagerTest, ConcurrentMixedOperations) {
    std::vector<std::thread> threads;
    std::vector<int> created_ids;
    std::mutex id_mutex;
    
    // Mix of create and destroy operations
    for (int i = 0; i < 20; ++i) {
        if (i % 2 == 0) {
            // Create thread
            threads.emplace_back([&, i]() {
                int id = session_manager_create_session(("mixed_" + std::to_string(i)).c_str());
                {
                    std::lock_guard<std::mutex> lock(id_mutex);
                    if (id > 0) created_ids.push_back(id);
                }
            });
        } else {
            // Destroy thread (destroys first created ID)
            threads.emplace_back([&]() {
                {
                    std::lock_guard<std::mutex> lock(id_mutex);
                    if (!created_ids.empty()) {
                        int id = created_ids.back();
                        created_ids.pop_back();
                        session_manager_destroy_session(id);
                    }
                }
            });
        }
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // State should be consistent
    int final_count = session_manager_get_session_count();
    EXPECT_GE(final_count, 0);
}

TEST_F(SessionManagerTest, NoDeadlocks) {
    // Create high contention scenario
    std::vector<std::thread> threads;
    
    for (int i = 0; i < 50; ++i) {
        threads.emplace_back([&, i]() {
            if (i % 3 == 0) {
                session_manager_create_session(("no_dl_" + std::to_string(i)).c_str());
            } else if (i % 3 == 1) {
                session_manager_destroy_session(i);
            } else {
                session_manager_get_session_count();
            }
        });
    }
    
    // Set timeout - if this takes > 5 seconds, likely deadlock
    auto start = std::chrono::high_resolution_clock::now();
    
    for (auto& t : threads) {
        t.join();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start);
    
    EXPECT_LT(duration.count(), 5); // Should complete quickly, not deadlock
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST_F(SessionManagerTest, CreateSessionPerformance) {
    auto start = std::chrono::high_resolution_clock::now();
    
    int id = session_manager_create_session("perf_test");
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    EXPECT_GT(id, 0);
    EXPECT_LT(duration.count(), 1000); // Should be < 1ms
}

TEST_F(SessionManagerTest, Create1000SessionsPerformance) {
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 1000; ++i) {
        session_manager_create_session(("perf_" + std::to_string(i)).c_str());
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_EQ(session_manager_get_session_count(), 1000);
    EXPECT_LT(duration.count(), 1000); // Should be < 1 second
}

TEST_F(SessionManagerTest, GetCountPerformance) {
    session_manager_create_session("test");
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 10000; ++i) {
        session_manager_get_session_count();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_LT(duration.count(), 100); // Should be < 100ms for 10k calls
}

// ============================================================================
// Stress Tests
// ============================================================================

TEST_F(SessionManagerTest, CreateManySessionsStress) {
    int session_count = 10000;
    
    for (int i = 0; i < session_count; ++i) {
        int id = session_manager_create_session(("stress_" + std::to_string(i)).c_str());
        EXPECT_GT(id, 0);
    }
    
    EXPECT_EQ(session_manager_get_session_count(), session_count);
}

TEST_F(SessionManagerTest, RapidCreateDestroyStress) {
    // Rapidly create and destroy sessions
    for (int cycle = 0; cycle < 100; ++cycle) {
        std::vector<int> ids;
        
        // Create 10
        for (int i = 0; i < 10; ++i) {
            ids.push_back(session_manager_create_session("rapid"));
        }
        
        EXPECT_EQ(session_manager_get_session_count(), 10);
        
        // Destroy all
        for (int id : ids) {
            session_manager_destroy_session(id);
        }
        
        EXPECT_EQ(session_manager_get_session_count(), 0);
    }
}

// ============================================================================
// Test Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
