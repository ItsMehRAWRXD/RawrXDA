#pragma once

/**
 * @file common_types.h
 * Centralized definition of common structures used across RawrXD
 * This prevents duplicate definitions and ensures consistency
 */

#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

#include <string>
#include <vector>
#include <cstdint>

// ============================================================================
// SHARED DATA STRUCTURES
// ============================================================================

/**
 * @struct ChatMessage
 * Represents a single message in a chat conversation
 * Compatible with OpenAI API format
 */
struct ChatMessage {
    std::string role;      // "system", "user", "assistant"
    std::string content;   // message text
    std::string name;      // optional, used for function calling
};

/**
 * @struct DownloadProgress
 * Tracks progress of file/model downloads
 * Used by HFDownloader and other async operations
 */
struct DownloadProgress {
    uint64_t bytes_downloaded = 0;
    uint64_t total_bytes = 0;
    float progress_percent = 0.0f;
    std::string current_file;
};
