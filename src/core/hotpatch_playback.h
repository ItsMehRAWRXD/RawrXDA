// hotpatch_playback.h - Media-Style Controls for Smoke Testing
// Playback, recording, and analysis of hotpatch sessions
// Part of RawrXD Progressive Layer Loading System

#ifndef HOTPATCH_PLAYBACK_H
#define HOTPATCH_PLAYBACK_H

#include "omnidirectional_hotpatch.h"
#include "live_hotpatch.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// PLAYBACK STATE MACHINE
// ============================================================================

typedef enum {
    PLAYBACK_STOPPED = 0,
    PLAYBACK_PLAYING,
    PLAYBACK_PAUSED,
    PLAYBACK_FAST_FORWARDING,
    PLAYBACK_REWINDING,
    PLAYBACK_RECORDING,
    PLAYBACK_EJECTED,
    
    PLAYBACK_STATE_COUNT
} PlaybackState;

typedef enum {
    SPEED_1X = 1,
    SPEED_2X = 2,
    SPEED_4X = 4,
    SPEED_8X = 8,
    SPEED_16X = 16,
    SPEED_32X = 32,
    SPEED_64X = 64,
    SPEED_MAX = 128
} PlaybackSpeed;

typedef enum {
    DIRECTION_FORWARD = 1,
    DIRECTION_BACKWARD = -1
} PlaybackDirection;

// ============================================================================
// RECORDED TEST SESSION
// ============================================================================

typedef struct {
    uint64_t timestamp;
    uint64_t state_id;
    PlaybackState playback_state;
    PlaybackSpeed speed;
    
    // Metrics at this point
    float quality;
    float speed;
    uint64_t memory_used;
    uint64_t tokens_generated;
    
    // Cumulative gains
    float quality_gain;
    float speed_gain;
    int64_t memory_saved;
    
    // Operation performed
    HotpatchOp operation;
    char description[128];
    
} PlaybackFrame;

typedef struct {
    PlaybackFrame* frames;
    uint32_t frame_count;
    uint32_t frame_capacity;
    
    uint64_t session_id;
    char session_name[256];
    uint64_t start_time;
    uint64_t end_time;
    
    // Session statistics
    float total_quality_gain;
    float total_speed_gain;
    int64_t total_memory_saved;
    uint32_t total_hotpatches;
    uint32_t total_reversals;
    uint32_t failed_operations;
    
    // Best/worst states
    uint64_t best_quality_state;
    uint64_t best_speed_state;
    uint64_t best_memory_state;
    float best_quality_score;
    float best_speed_score;
    uint64_t best_memory_bytes;
    
} TestSession;

// ============================================================================
// SMOKE TEST CONFIGURATION
// ============================================================================

typedef enum {
    SMOKE_QUICK,           // Fast test (1-2 minutes)
    SMOKE_STANDARD,        // Normal test (5-10 minutes)
    SMOKE_THOROUGH,        // Deep test (30+ minutes)
    SMOKE_EXHAUSTIVE,      // Complete test (hours)
    SMOKE_STRESS,          // Stress test (extreme conditions)
    SMOKE_COMPARISON       // Compare multiple configurations
} SmokeTestMode;

typedef struct {
    SmokeTestMode mode;
    uint32_t max_iterations;
    uint32_t max_time_seconds;
    
    // Test targets
    float target_quality_min;
    float target_speed_min;
    uint64_t target_memory_max;
    
    // Hotpatch operations to test
    HotpatchOp test_operations;
    
    // Exploration parameters
    float exploration_rate;      // How much to explore vs exploit
    float random_jump_chance;    // Chance to jump to random state
    bool test_all_paths;         // Test all possible paths
    bool test_merge_combinations; // Test merging states
    
    // Validation
    bool validate_each_step;     // Run inference after each change
    uint32_t validation_tokens;  // Tokens to generate for validation
    float quality_degradation_max; // Max allowed quality drop
    
    // Recording
    bool record_session;         // Record all frames
    bool generate_report;         // Generate detailed report
    bool export_best_states;      // Export best states found
    
} SmokeTestConfig;

// ============================================================================
// PLAYBACK ANALYTICS
// ============================================================================

typedef struct {
    // Timeline metrics
    float* quality_timeline;
    float* speed_timeline;
    uint64_t* memory_timeline;
    uint32_t timeline_length;
    
    // Aggregated metrics
    float avg_quality;
    float avg_speed;
    float avg_memory_mb;
    float quality_variance;
    float speed_variance;
    
    // Gain analysis
    float peak_quality_gain;
    float peak_speed_gain;
    int64_t peak_memory_saved;
    float avg_gain_per_hotpatch;
    
    // State visit analysis
    uint32_t* state_visit_counts;
    uint32_t unique_states_visited;
    uint32_t state_revisits;
    
    // Path analysis
    uint32_t unique_paths_taken;
    uint32_t optimal_paths_found;
    uint32_t dead_ends_encountered;
    float avg_path_length;
    
    // Time analysis
    uint64_t total_playback_time_ns;
    uint64_t time_in_play_ns;
    uint64_t time_in_fast_forward_ns;
    uint64_t time_in_rewind_ns;
    uint64_t time_in_pause_ns;
    
    // Efficiency metrics
    float hotpatches_per_second;
    float state_changes_per_second;
    float quality_gain_per_second;
    
} PlaybackAnalytics;

// ============================================================================
// SMOKE TEST RESULT
// ============================================================================

typedef struct {
    bool passed;
    float final_quality;
    float final_speed;
    uint64_t final_memory;
    float quality_improvement;
    float speed_improvement;
    float memory_improvement;
    uint32_t states_tested;
    uint32_t hotpatches_applied;
    uint32_t failures;
    uint64_t test_duration_ns;
    char summary[2048];
} SmokeTestResult;

// ============================================================================
// STATE COMPARISON
// ============================================================================

typedef struct {
    uint64_t state1_id;
    uint64_t state2_id;
    
    float quality_diff;
    float speed_diff;
    int64_t memory_diff;
    float overall_diff;
    
    float quality_ratio;
    float speed_ratio;
    float memory_ratio;
    
    char winner[64];
    char recommendation[256];
} StateComparison;

typedef struct {
    TestSession* session1;
    TestSession* session2;
    
    float quality_diff;
    float speed_diff;
    int64_t memory_diff;
    
    uint32_t frame_count_diff;
    uint32_t hotpatch_diff;
    
    char recommendation[256];
} SessionComparison;

// ============================================================================
// PLAYBACK CONTROLLER
// ============================================================================

typedef struct {
    // Core systems
    OmnidirectionalHotpatch* omni;
    TestSession* session;
    PlaybackAnalytics* analytics;
    
    // Playback state
    PlaybackState state;
    PlaybackSpeed speed;
    PlaybackDirection direction;
    uint32_t current_frame_index;
    
    // Position in state history
    uint64_t current_state_id;
    uint32_t position_in_history;
    
    // Recording
    bool is_recording;
    TestSession recorded_session;
    
    // Smoke testing
    SmokeTestConfig smoke_config;
    bool smoke_test_active;
    uint32_t smoke_test_iteration;
    uint64_t smoke_test_start_time;
    
    // Playback queue
    uint64_t* playback_queue;
    uint32_t queue_length;
    uint32_t queue_capacity;
    uint32_t queue_position;
    
    // Benchmark state
    uint64_t benchmark_start_time;
    uint32_t tokens_generated_during_session;
    float last_quality;
    float last_speed;
    
    // User callbacks
    void (*on_state_change)(uint64_t new_state_id, void* user_data);
    void (*on_frame_recorded)(PlaybackFrame* frame, void* user_data);
    void (*on_smoke_complete)(TestSession* session, void* user_data);
    void* user_data;
    
} PlaybackController;

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

// Controller lifecycle
PlaybackController* playback_create(OmnidirectionalHotpatch* omni);
void playback_destroy(PlaybackController* controller);

// Media controls
bool playback_play(PlaybackController* controller);
bool playback_pause(PlaybackController* controller);
bool playback_stop(PlaybackController* controller);
bool playback_fast_forward(PlaybackController* controller, PlaybackSpeed speed);
bool playback_rewind(PlaybackController* controller);
bool playback_eject(PlaybackController* controller, const char* export_path);

// Navigation controls
bool playback_seek_to_frame(PlaybackController* controller, uint32_t frame_index);
bool playback_seek_to_state(PlaybackController* controller, uint64_t state_id);
bool playback_seek_to_time(PlaybackController* controller, uint32_t seconds);
bool playback_step_forward(PlaybackController* controller);
bool playback_step_backward(PlaybackController* controller);
bool playback_jump_to_best(PlaybackController* controller);
bool playback_jump_to_worst(PlaybackController* controller);

// Recording
bool playback_record_start(PlaybackController* controller, const char* session_name);
bool playback_record_stop(PlaybackController* controller);
bool playback_record_frame(PlaybackController* controller, HotpatchOp operation);
TestSession* playback_get_recorded_session(PlaybackController* controller);

// Smoke testing
bool smoke_test_start(PlaybackController* controller, SmokeTestConfig* config);
bool smoke_test_stop(PlaybackController* controller);
bool smoke_test_step(PlaybackController* controller);
SmokeTestResult smoke_test_run(PlaybackController* controller, SmokeTestConfig* config);
bool smoke_test_compare(PlaybackController* controller, uint64_t* state_ids, uint32_t count);

// Analytics
void playback_analyze(PlaybackController* controller);
void playback_generate_report(PlaybackController* controller, const char* output_path);
void playback_export_timeline(PlaybackController* controller, const char* output_path);
void playback_visualize_gains(PlaybackController* controller, const char* output_path);

// State comparison
void compare_states(PlaybackController* controller, uint64_t state1, uint64_t state2, 
                    StateComparison* result);
void compare_sessions(TestSession* session1, TestSession* session2, 
                      SessionComparison* result);

// Benchmark helpers
float benchmark_quality(PlaybackController* controller, uint64_t state_id, uint32_t tokens);
float benchmark_speed(PlaybackController* controller, uint64_t state_id, uint32_t tokens);
uint64_t benchmark_memory(PlaybackController* controller, uint64_t state_id);

// Progress tracking
float playback_get_progress(PlaybackController* controller);
uint32_t playback_get_position(PlaybackController* controller);
uint32_t playback_get_total_frames(PlaybackController* controller);
bool playback_is_playing(PlaybackController* controller);
bool playback_is_recording(PlaybackController* controller);

// Queue management
bool playback_queue_state(PlaybackController* controller, uint64_t state_id);
bool playback_queue_path(PlaybackController* controller, uint64_t* path, uint32_t length);
bool playback_clear_queue(PlaybackController* controller);
bool playback_process_queue(PlaybackController* controller);

// Event handlers
void playback_set_callbacks(PlaybackController* controller,
                           void (*on_state_change)(uint64_t, void*),
                           void (*on_frame_recorded)(PlaybackFrame*, void*),
                           void (*on_smoke_complete)(TestSession*, void*),
                           void* user_data);

// Utility
uint64_t get_time_ns(void);

#ifdef __cplusplus
}
#endif

#endif // HOTPATCH_PLAYBACK_H
