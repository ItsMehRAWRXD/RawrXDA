#include "gtest/gtest.h"
#include "streaming/MappedWindowStreamer.hpp"
#include <fstream>
#include <vector>
#include <cstdio>

using namespace RawrXD;

class MappedWindowStreamerTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        // Create a temporary test file (10 MB with deterministic pattern)
        test_file_ = "test_mapped_window_temp_12345.bin";
        std::ofstream out(test_file_, std::ios::binary);
        
        // Fill with 0-255 pattern repeating
        size_t file_size = 10 * 1024 * 1024;  // 10 MB
        std::vector<uint8_t> buffer(file_size);
        for (size_t i = 0; i < buffer.size(); ++i)
        {
            buffer[i] = static_cast<uint8_t>(i % 256);
        }
        out.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
        out.close();
    }

    virtual void TearDown()
    {
        if (std::remove(test_file_.c_str()) != 0)
        {
            std::cerr << "Warning: Could not delete test file: " << test_file_ << std::endl;
        }
    }

    const char* test_file_;
};

// Test 1: Basic open and close
TEST_F(MappedWindowStreamerTest, BasicOpenClose)
{
    MappedWindowStreamer streamer(64);  // 64 MB window
    EXPECT_TRUE(streamer.Open(test_file_));
    EXPECT_EQ(streamer.GetFileSize(), 10 * 1024 * 1024);
    EXPECT_TRUE(streamer.Close());
}

// Test 2: Map and unmap region
TEST_F(MappedWindowStreamerTest, MapRegion)
{
    MappedWindowStreamer streamer(64);
    EXPECT_TRUE(streamer.Open(test_file_));

    // Map first 1 MB
    uint8_t* ptr = streamer.MapRegion(0, 1024 * 1024);
    EXPECT_NE(ptr, nullptr);

    // Verify data pattern
    for (int i = 0; i < 256; ++i)
    {
        EXPECT_EQ(ptr[i], i & 0xFF);
    }

    streamer.UnmapRegion();
    streamer.Close();
}

// Test 3: Multiple sequential map calls
TEST_F(MappedWindowStreamerTest, SequentialMapCalls)
{
    MappedWindowStreamer streamer(64);
    EXPECT_TRUE(streamer.Open(test_file_));

    // Map first region
    uint8_t* ptr1 = streamer.MapRegion(0, 1024 * 1024);
    EXPECT_NE(ptr1, nullptr);
    EXPECT_EQ(ptr1[0], 0);  // First byte should be 0

    // Map second region (this unmaps the first automatically)
    uint8_t* ptr2 = streamer.MapRegion(2 * 1024 * 1024, 1024 * 1024);
    EXPECT_NE(ptr2, nullptr);
    EXPECT_EQ(ptr2[0], 0);  // Pattern repeats every 256 bytes

    streamer.Close();
}

// Test 4: Default window size mapping
TEST_F(MappedWindowStreamerTest, DefaultWindowMapping)
{
    MappedWindowStreamer streamer(64);  // 64 MB window
    EXPECT_TRUE(streamer.Open(test_file_));

    // Map without specifying size (should use default window)
    uint8_t* ptr = streamer.MapRegion(0);  // size defaults to 64 MB
    EXPECT_NE(ptr, nullptr);

    // We can access data up to window size
    for (size_t i = 0; i < 256; ++i)
    {
        EXPECT_EQ(ptr[i], static_cast<uint8_t>(i & 0xFF));
    }

    streamer.UnmapRegion();
    streamer.Close();
}

// Test 5: Out-of-bounds detection
TEST_F(MappedWindowStreamerTest, OutOfBoundsDetection)
{
    MappedWindowStreamer streamer(64);
    EXPECT_TRUE(streamer.Open(test_file_));
    
    uint64_t file_size = streamer.GetFileSize();
    
    // Valid range
    EXPECT_TRUE(streamer.IsValidRange(0, file_size));
    
    // Invalid: past end
    EXPECT_FALSE(streamer.IsValidRange(0, file_size + 1));
    
    // Invalid: starting past end
    EXPECT_FALSE(streamer.IsValidRange(file_size + 1, 1));
    
    streamer.Close();
}

// Test 6: Get current view info
TEST_F(MappedWindowStreamerTest, GetCurrentView)
{
    MappedWindowStreamer streamer(64);
    EXPECT_TRUE(streamer.Open(test_file_));

    uint8_t* ptr = streamer.MapRegion(1024, 512);
    EXPECT_NE(ptr, nullptr);

    auto view = streamer.GetCurrentView();
    EXPECT_EQ(view.ptr, ptr);
    EXPECT_EQ(view.file_offset, 1024ULL);
    EXPECT_LE(view.size, 512ULL);  // May be <= requested due to alignment

    streamer.UnmapRegion();
    streamer.Close();
}

// Test 7: Cannot map closed file
TEST_F(MappedWindowStreamerTest, ClosedFileBehavior)
{
    MappedWindowStreamer streamer(64);
    
    // Try to map without opening
    uint8_t* ptr = streamer.MapRegion(0, 1024);
    EXPECT_EQ(ptr, nullptr);
    
    // Open and then close
    EXPECT_TRUE(streamer.Open(test_file_));
    EXPECT_TRUE(streamer.Close());
    
    // Try to map after closing
    ptr = streamer.MapRegion(0, 1024);
    EXPECT_EQ(ptr, nullptr);
}

// Test 8: Small window size configuration
TEST_F(MappedWindowStreamerTest, SmallWindowSize)
{
    MappedWindowStreamer streamer(1);  // 1 MB window
    EXPECT_TRUE(streamer.Open(test_file_));
    
    // Even with 1 MB window, should be able to map
    uint8_t* ptr = streamer.MapRegion(0, 512 * 1024);
    EXPECT_NE(ptr, nullptr);
    
    streamer.UnmapRegion();
    streamer.Close();
}

// Test 9: Data pattern verification across large region
TEST_F(MappedWindowStreamerTest, DataPatternLargeRegion)
{
    MappedWindowStreamer streamer(64);
    EXPECT_TRUE(streamer.Open(test_file_));

    // Map a 512 KB region
    uint8_t* ptr = streamer.MapRegion(0, 512 * 1024);
    EXPECT_NE(ptr, nullptr);

    // Verify pattern is correct throughout
    for (size_t i = 0; i < 512 * 1024; ++i)
    {
        if (ptr[i] != static_cast<uint8_t>(i % 256))
        {
            FAIL() << "Pattern mismatch at offset " << i;
        }
    }

    streamer.UnmapRegion();
    streamer.Close();
}

// Test 10: Repeated map-unmap cycles
TEST_F(MappedWindowStreamerTest, RepeatedCycles)
{
    MappedWindowStreamer streamer(64);
    EXPECT_TRUE(streamer.Open(test_file_));

    for (int cycle = 0; cycle < 10; ++cycle)
    {
        uint64_t offset = (cycle * 1024 * 1024) % (streamer.GetFileSize() - 1024 * 1024);
        uint8_t* ptr = streamer.MapRegion(offset, 1024 * 1024);
        EXPECT_NE(ptr, nullptr) << "Cycle " << cycle;
        EXPECT_EQ(ptr[0], static_cast<uint8_t>(offset % 256)) << "Cycle " << cycle;
        streamer.UnmapRegion();
    }

    streamer.Close();
}
