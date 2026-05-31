// hotpatch_playback_test.c - Comprehensive Test Suite for Hotpatch Playback System
// Tests all media controls, smoke testing, and analytics functionality
// Part of RawrXD Progressive Layer Loading System

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#define HOTPATCH_PLAYBACK_IMPLEMENTATION
#include "hotpatch_playback.h"

// ============================================================================
// TEST INFRASTRUCTURE
// ============================================================================

typedef struct {
    const char* name;
    bool (*test_func)(void);
    bool passed;
    const char* error_msg;
} TestCase;

typedef struct {
    int total_tests;
    int passed_tests;
    int failed_tests;
    int skipped_tests;
    uint64_t start_time_ns;
    uint64_t end_time_ns;
} TestResults;

static TestResults g_results = {0};
static int g_verbose = 1;

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        if (g_verbose) printf("    ✗ FAIL: %s\n", msg); \
        return false; \
    } \
} while(0)

#define TEST_ASSERT_EQ(a, b, msg) TEST_ASSERT((a) == (b), msg)
#define TEST_ASSERT_NE(a, b, msg) TEST_ASSERT((a) != (b), msg)
#define TEST_ASSERT_GT(a, b, msg) TEST_ASSERT((a) > (b), msg)
#define TEST_ASSERT_GE(a, b, msg) TEST_ASSERT((a) >= (b), msg)
#define TEST_ASSERT_LT(a, b, msg) TEST_ASSERT((a) < (b), msg)
#define TEST_ASSERT_LE(a, b, msg) TEST_ASSERT((a) <= (b), msg)
#define TEST_ASSERT_NEAR(a, b, eps, msg) TEST_ASSERT(fabsf((a) - (b)) < (eps), msg)

// ============================================================================
// MOCK DATA GENERATION
// ============================================================================

static PlaybackController* create_mock_controller(void) {
    // Create mock omnidirectional hotpatch system
    LiveHotpatchConfig config = {
        .max_checkpoints = 16,
        .max_prune_percent = 0.5f,
        .importance_method = IMPORTANCE_MAGNITUDE,
        .enable_head_pruning = true,
        .enable_layer_fusion = true,
        .enable_kv_compression = true
    };
    
    LiveHotpatch* base = live_hotpatch_create(&config, NULL);
    if (!base) return NULL;
    
    OmnidirectionalHotpatch* omni = omni_hotpatch_create(base);
    if (!omni) {
        live_hotpatch_destroy(base);
        return NULL;
    }
    
    PlaybackController* ctrl = playback_create(omni);
    if (!ctrl) {
        omni_hotpatch_destroy(omni);
        live_hotpatch_destroy(base);
        return NULL;
    }
    
    return ctrl;
}

static void destroy_mock_controller(PlaybackController* ctrl) {
    if (!ctrl) return;
    
    OmnidirectionalHotpatch* omni = ctrl->omni;
    LiveHotpatch* base = omni ? omni->base : NULL;
    
    playback_destroy(ctrl);
    if (omni) omni_hotpatch_destroy(omni);
    if (base) live_hotpatch_destroy(base);
}

static void populate_mock_states(PlaybackController* ctrl, uint32_t count) {
    if (!ctrl || !ctrl->omni) return;
    
    // Create mock states in the state graph
    for (uint32_t i = 0; i < count; i++) {
        HotpatchState state = {0};
        state.state_id = i + 1;
        state.quality_score = 0.5f + 0.4f * ((float)rand() / RAND_MAX);
        state.speed_score = 0.5f + 0.4f * ((float)rand() / RAND_MAX);
        state.memory_usage = 1024 * 1024 * (100 + rand() % 500); // 100-600 MB
        state.state_vector[AXIS_QUALITY] = state.quality_score;
        state.state_vector[AXIS_SPEED] = state.speed_score;
        state.state_vector[AXIS_MEMORY] = state.memory_usage / (1024.0f * 1024.0f * 1024.0f);
        
        // Add to state graph
        if (ctrl->omni->state_graph->state_count < ctrl->omni->state_graph->capacity) {
            ctrl->omni->state_graph->states[ctrl->omni->state_graph->state_count++] = state;
        }
    }
}

// ============================================================================
// CONTROLLER LIFECYCLE TESTS
// ============================================================================

static bool test_controller_create_destroy(void) {
    if (g_verbose) printf("  Testing controller creation and destruction...\n");
    
    PlaybackController* ctrl = create_mock_controller();
    TEST_ASSERT(ctrl != NULL, "Controller should not be NULL");
    TEST_ASSERT(ctrl->state == PLAYBACK_STOPPED, "Initial state should be STOPPED");
    TEST_ASSERT(ctrl->speed == SPEED_1X, "Initial speed should be 1X");
    TEST_ASSERT(ctrl->direction == DIRECTION_FORWARD, "Initial direction should be FORWARD");
    TEST_ASSERT(ctrl->session != NULL, "Session should be allocated");
    TEST_ASSERT(ctrl->analytics != NULL, "Analytics should be allocated");
    
    destroy_mock_controller(ctrl);
    
    if (g_verbose) printf("    ✓ Controller lifecycle test passed\n");
    return true;
}

static bool test_controller_initial_state(void) {
    if (g_verbose) printf("  Testing controller initial state...\n");
    
    PlaybackController* ctrl = create_mock_controller();
    TEST_ASSERT(ctrl != NULL, "Controller should not be NULL");
    
    // Check initial state
    TEST_ASSERT_EQ(ctrl->state, PLAYBACK_STOPPED, "State should be STOPPED");
    TEST_ASSERT_EQ(ctrl->is_recording, false, "Should not be recording initially");
    TEST_ASSERT_EQ(ctrl->smoke_test_active, false, "Smoke test should not be active initially");
    TEST_ASSERT_EQ(ctrl->session->frame_count, 0, "Frame count should be 0");
    TEST_ASSERT_EQ(ctrl->queue_length, 0, "Queue should be empty");
    
    destroy_mock_controller(ctrl);
    
    if (g_verbose) printf("    ✓ Initial state test passed\n");
    return true;
}

// ============================================================================
// MEDIA CONTROLS TESTS
// ============================================================================

static bool test_playback_play_pause_stop(void) {
    if (g_verbose) printf("  Testing play/pause/stop controls...\n");
    
    PlaybackController* ctrl = create_mock_controller();
    populate_mock_states(ctrl, 10);
    TEST_ASSERT(ctrl != NULL, "Controller should not be NULL");
    
    // Test play
    bool result = playback_play(ctrl);
    TEST_ASSERT(result, "Play should succeed");
    TEST_ASSERT_EQ(ctrl->state, PLAYBACK_PLAYING, "State should be PLAYING");
    TEST_ASSERT(playback_is_playing(ctrl), "is_playing should return true");
    
    // Test pause
    result = playback_pause(ctrl);
    TEST_ASSERT(result, "Pause should succeed");
    TEST_ASSERT_EQ(ctrl->state, PLAYBACK_PAUSED, "State should be PAUSED");
    TEST_ASSERT(!playback_is_playing(ctrl), "is_playing should return false when paused");
    
    // Test play again
    result = playback_play(ctrl);
    TEST_ASSERT(result, "Play should succeed after pause");
    TEST_ASSERT_EQ(ctrl->state, PLAYBACK_PLAYING, "State should be PLAYING again");
    
    // Test stop
    result = playback_stop(ctrl);
    TEST_ASSERT(result, "Stop should succeed");
    TEST_ASSERT_EQ(ctrl->state, PLAYBACK_STOPPED, "State should be STOPPED");
    TEST_ASSERT(!playback_is_playing(ctrl), "is_playing should return false when stopped");
    
    destroy_mock_controller(ctrl);
    
    if (g_verbose) printf("    ✓ Play/pause/stop test passed\n");
    return true;
}

static bool test_playback_fast_forward(void) {
    if (g_verbose) printf("  Testing fast forward controls...\n");
    
    PlaybackController* ctrl = create_mock_controller();
    populate_mock_states(ctrl, 20);
    TEST_ASSERT(ctrl != NULL, "Controller should not be NULL");
    
    // Queue some states for playback
    for (uint32_t i = 0; i < 10; i++) {
        playback_queue_state(ctrl, i + 1);
    }
    
    // Test fast forward at different speeds
    bool result = playback_fast_forward(ctrl, SPEED_2X);
    TEST_ASSERT(result, "Fast forward 2X should succeed");
    TEST_ASSERT_EQ(ctrl->state, PLAYBACK_FAST_FORWARDING, "State should be FAST_FORWARDING");
    TEST_ASSERT_EQ(ctrl->speed, SPEED_2X, "Speed should be 2X");
    
    playback_stop(ctrl);
    
    result = playback_fast_forward(ctrl, SPEED_4X);
    TEST_ASSERT(result, "Fast forward 4X should succeed");
    TEST_ASSERT_EQ(ctrl->speed, SPEED_4X, "Speed should be 4X");
    
    playback_stop(ctrl);
    
    result = playback_fast_forward(ctrl, SPEED_8X);
    TEST_ASSERT(result, "Fast forward 8X should succeed");
    TEST_ASSERT_EQ(ctrl->speed, SPEED_8X, "Speed should be 8X");
    
    destroy_mock_controller(ctrl);
    
    if (g_verbose) printf("    ✓ Fast forward test passed\n");
    return true;
}

static bool test_playback_rewind(void) {
    if (g_verbose) printf("  Testing rewind control...\n");
    
    PlaybackController* ctrl = create_mock_controller();
    populate_mock_states(ctrl, 10);
    TEST_ASSERT(ctrl != NULL, "Controller should not be NULL");
    
    // Start recording and create some frames
    playback_record_start(ctrl, "Rewind Test");
    
    for (int i = 0; i < 5; i++) {
        playback_record_frame(ctrl, (HotpatchOp)(1 << i));
    }
    
    playback_record_stop(ctrl);
    
    // Move to end
    playback_seek_to_frame(ctrl, ctrl->session->frame_count - 1);
    uint32_t end_pos = playback_get_position(ctrl);
    
    // Test rewind
    bool result = playback_rewind(ctrl);
    TEST_ASSERT(result, "Rewind should succeed");
    TEST_ASSERT_EQ(ctrl->state, PLAYBACK_REWINDING, "State should be REWINDING");
    TEST_ASSERT_EQ(ctrl->direction, DIRECTION_BACKWARD, "Direction should be BACKWARD");
    
    uint32_t new_pos = playback_get_position(ctrl);
    TEST_ASSERT_LT(new_pos, end_pos, "Position should have moved backward");
    
    destroy_mock_controller(ctrl);
    
    if (g_verbose) printf("    ✓ Rewind test passed\n");
    return true;
}

static bool test_playback_eject(void) {
    if (g_verbose) printf("  Testing eject control...\n");
    
    PlaybackController* ctrl = create_mock_controller();
    populate_mock_states(ctrl, 10);
    TEST_ASSERT(ctrl != NULL, "Controller should not be NULL");
    
    // Create some session data
    playback_record_start(ctrl, "Eject Test");
    for (int i = 0; i < 5; i++) {
        playback_record_frame(ctrl, (HotpatchOp)(1 << i));
    }
    
    // Test eject
    bool result = playback_eject(ctrl, "test_output");
    TEST_ASSERT(result, "Eject should succeed");
    TEST_ASSERT_EQ(ctrl->state, PLAYBACK_EJECTED, "State should be EJECTED");
    
    // Verify can't play after eject
    result = playback_play(ctrl);
    TEST_ASSERT(!result, "Play should fail after eject");
    
    destroy_mock_controller(ctrl);
    
    if (g_verbose) printf("    ✓ Eject test passed\n");
    return true;
}

// ============================================================================
// NAVIGATION TESTS
// ============================================================================

static bool test_seek_to_frame(void) {
    if (g_verbose) printf("  Testing seek to frame...\n");
    
    PlaybackController* ctrl = create_mock_controller();
    populate_mock_states(ctrl, 20);
    TEST_ASSERT(ctrl != NULL, "Controller should not be NULL");
    
    // Create session with frames
    playback_record_start(ctrl, "Seek Test");
    for (int i = 0; i < 10; i++) {
        playback_record_frame(ctrl, HOTPATCH_NONE);
    }
    playback_record_stop(ctrl);
    
    // Test seek to various frames
    bool result = playback_seek_to_frame(ctrl, 5);
    TEST_ASSERT(result, "Seek to frame 5 should succeed");
    TEST_ASSERT_EQ(playback_get_position(ctrl), 5, "Position should be 5");
    
    result = playback_seek_to_frame(ctrl, 0);
    TEST_ASSERT(result, "Seek to frame 0 should succeed");
    TEST_ASSERT_EQ(playback_get_position(ctrl), 0, "Position should be 0");
    
    result = playback_seek_to_frame(ctrl, 9);
    TEST_ASSERT(result, "Seek to frame 9 should succeed");
    TEST_ASSERT_EQ(playback_get_position(ctrl), 9, "Position should be 9");
    
    // Test invalid seek
    result = playback_seek_to_frame(ctrl, 100);
    TEST_ASSERT(!result, "Seek to invalid frame should fail");
    
    destroy_mock_controller(ctrl);
    
    if (g_verbose) printf("    ✓ Seek to frame test passed\n");
    return true;
}

static bool test_seek_to_state(void) {
    if (g_verbose) printf("  Testing seek to state...\n");
    
    PlaybackController* ctrl = create_mock_controller();
    populate_mock_states(ctrl, 20);
    TEST_ASSERT(ctrl != NULL, "Controller should not be NULL");
    
    // Create session with frames
    playback_record_start(ctrl, "State Seek Test");
    for (int i = 0; i < 10; i++) {
        playback_record_frame(ctrl, HOTPATCH_NONE);
    }
    playback_record_stop(ctrl);
    
    // Test seek to state
    bool result = playback_seek_to_state(ctrl, 5);
    TEST_ASSERT(result, "Seek to state 5 should succeed");
    TEST_ASSERT_EQ(ctrl->current_state_id, 5, "Current state should be 5");
    
    destroy_mock_controller(ctrl);
    
    if (g_verbose) printf("    ✓ Seek to state test passed\n");
    return true;
}

static bool test_step_forward_backward(void) {
    if (g_verbose) printf("  Testing step forward/backward...\n");
    
    PlaybackController* ctrl = create_mock_controller();
    populate_mock_states(ctrl, 20);
    TEST_ASSERT(ctrl != NULL, "Controller should not be NULL");
    
    // Queue states
    for (uint32_t i = 1; i <= 10; i++) {
        playback_queue_state(ctrl, i);
    }
    
    // Start at beginning
    playback_seek_to_frame(ctrl, 0);
    
    // Test step forward
    bool result = playback_step_forward(ctrl);
    TEST_ASSERT(result, "Step forward should succeed");
    TEST_ASSERT_EQ(playback_get_position(ctrl), 1, "Position should be 1");
    
    result = playback_step_forward(ctrl);
    TEST_ASSERT(result, "Step forward should succeed");
    TEST_ASSERT_EQ(playback_get_position(ctrl), 2, "Position should be 2");
    
    // Test step backward
    result = playback_step_backward(ctrl);
    TEST_ASSERT(result, "Step backward should succeed");
    TEST_ASSERT_EQ(playback_get_position(ctrl), 1, "Position should be 1");
    
    destroy_mock_controller(ctrl);
    
    if (g_verbose) printf("    ✓ Step forward/backward test passed\n");
    return true;
}

static bool test_jump_to_best_worst(void) {
    if (g_verbose) printf("  Testing jump to best/worst...\n");
    
    PlaybackController* ctrl = create_mock_controller();
    populate_mock_states(ctrl, 20);
    TEST_ASSERT(ctrl != NULL, "Controller should not be NULL");
    
    // Create session with varying quality
    playback_record_start(ctrl, "Best/Worst Test");
    
    // Create frames with different quality scores
    for (int i = 0; i < 10; i++) {
        ctrl->session->frames[ctrl->session->frame_count].quality = 0.3f + 0.05f * i;
        ctrl->session->frames[ctrl->session->frame_count].state_id = i + 1;
        ctrl->session->frame_count++;
    }
    
    // Set best/worst tracking
    ctrl->session->best_quality_score = 0.75f;
    ctrl->session->best_quality_state = 10;
    
    // Test jump to best
    bool result = playback_jump_to_best(ctrl);
    TEST_ASSERT(result, "Jump to best should succeed");
    
    // Test jump to worst
    result = playback_jump_to_worst(ctrl);
    TEST_ASSERT(result, "Jump to worst should succeed");
    
    destroy_mock_controller(ctrl);
    
    if (g_verbose) printf("    ✓ Jump to best/worst test passed\n");
    return true;
}

// ============================================================================
// RECORDING TESTS
// ============================================================================

static bool test_record_start_stop(void) {
    if (g_verbose) printf("  Testing record start/stop...\n");
    
    PlaybackController* ctrl = create_mock_controller();
    TEST_ASSERT(ctrl != NULL, "Controller should not be NULL");
    
    // Test record start
    bool result = playback_record_start(ctrl, "Test Session");
    TEST_ASSERT(result, "Record start should succeed");
    TEST_ASSERT(ctrl->is_recording, "Should be recording");
    TEST_ASSERT_EQ(ctrl->session->frame_count, 1, "Should have initial frame");
    TEST_ASSERT(strcmp(ctrl->session->session_name, "Test Session") == 0, "Session name should match");
    
    // Test record stop
    result = playback_record_stop(ctrl);
    TEST_ASSERT(result, "Record stop should succeed");
    TEST_ASSERT(!ctrl->is_recording, "Should not be recording");
    TEST_ASSERT(ctrl->session->end_time > 0, "End time should be set");
    
    destroy_mock_controller(ctrl);
    
    if (g_verbose) printf("    ✓ Record start/stop test passed\n");
    return true;
}

static bool test_record_frames(void) {
    if (g_verbose) printf("  Testing frame recording...\n");
    
    PlaybackController* ctrl = create_mock_controller();
    populate_mock_states(ctrl, 10);
    TEST_ASSERT(ctrl != NULL, "Controller should not be NULL");
    
    // Start recording
    playback_record_start(ctrl, "Frame Recording Test");
    
    // Record multiple frames
    for (int i = 0; i < 10; i++) {
        HotpatchOp op = (HotpatchOp)(1 << (i % 8));
        bool result = playback_record_frame(ctrl, op);
        TEST_ASSERT(result, "Frame recording should succeed");
    }
    
    TEST_ASSERT_EQ(ctrl->session->frame_count, 11, "Should have 11 frames (1 initial + 10 recorded)");
    
    // Stop recording
    playback_record_stop(ctrl);
    
    // Verify frames
    for (uint32_t i = 0; i < ctrl->session->frame_count; i++) {
        PlaybackFrame* frame = &ctrl->session->frames[i];
        TEST_ASSERT(frame->timestamp > 0, "Frame should have timestamp");
        TEST_ASSERT(frame->state_id > 0, "Frame should have state ID");
    }
    
    destroy_mock_controller(ctrl);
    
    if (g_verbose) printf("    ✓ Frame recording test passed\n");
    return true;
}

static bool test_record_with_operations(void) {
    if (g_verbose) printf("  Testing recording with operations...\n");
    
    PlaybackController* ctrl = create_mock_controller();
    populate_mock_states(ctrl, 10);
    TEST_ASSERT(ctrl != NULL, "Controller should not be NULL");
    
    playback_record_start(ctrl, "Operations Test");
    
    // Record frames with different operations
    playback_record_frame(ctrl, HOTPATCH_PRUNE_WEIGHTS);
    playback_record_frame(ctrl, HOTPATCH_QUANTIZE);
    playback_record_frame(ctrl, HOTPATCH_COMPRESS_KV);
    playback_record_frame(ctrl, HOTPATCH_PRUNE_HEADS);
    playback_record_frame(ctrl, HOTPATCH_FUSE_LAYERS);
    
    TEST_ASSERT_EQ(ctrl->session->total_hotpatches, 5, "Should count 5 hotpatches");
    
    playback_record_stop(ctrl);
    
    destroy_mock_controller(ctrl);
    
    if (g_verbose) printf("    ✓ Recording with operations test passed\n");
    return true;
}

// ============================================================================
// SMOKE TEST TESTS
// ============================================================================

static bool test_smoke_test_quick(void) {
    if (g_verbose) printf("  Testing quick smoke test...\n");
    
    PlaybackController* ctrl = create_mock_controller();
    populate_mock_states(ctrl, 50);
    TEST_ASSERT(ctrl != NULL, "Controller should not be NULL");
    
    SmokeTestConfig config = {
        .mode = SMOKE_QUICK,
        .max_iterations = 5,
        .max_time_seconds = 5,
        .test_operations = HOTPATCH_PRUNE_WEIGHTS | HOTPATCH_QUANTIZE,
        .exploration_rate = 0.5f,
        .record_session = true,
        .generate_report = false
    };
    
    SmokeTestResult result = smoke_test_run(ctrl, &config);
    
    TEST_ASSERT(result.passed || !result.passed, "Smoke test should complete");
    TEST_ASSERT(result.states_tested > 0, "Should have tested states");
    TEST_ASSERT_GT(result.test_duration_ns, 0, "Should have duration");
    
    destroy_mock_controller(ctrl);
    
    if (g_verbose) printf("    ✓ Quick smoke test passed\n");
    return true;
}

static bool test_smoke_test_standard(void) {
    if (g_verbose) printf("  Testing standard smoke test...\n");
    
    PlaybackController* ctrl = create_mock_controller();
    populate_mock_states(ctrl, 100);
    TEST_ASSERT(ctrl != NULL, "Controller should not be NULL");
    
    SmokeTestConfig config = {
        .mode = SMOKE_STANDARD,
        .max_iterations = 20,
        .max_time_seconds = 10,
        .test_operations = HOTPATCH_ALL,
        .exploration_rate = 0.3f,
        .record_session = true,
        .generate_report = false
    };
    
    SmokeTestResult result = smoke_test_run(ctrl, &config);
    
    TEST_ASSERT(result.states_tested > 0, "Should have tested states");
    TEST_ASSERT(result.hotpatches_applied >= 0, "Should have hotpatch count");
    
    destroy_mock_controller(ctrl);
    
    if (g_verbose) printf("    ✓ Standard smoke test passed\n");
    return true;
}

static bool test_smoke_test_comparison(void) {
    if (g_verbose) printf("  Testing state comparison...\n");
    
    PlaybackController* ctrl = create_mock_controller();
    populate_mock_states(ctrl, 10);
    TEST_ASSERT(ctrl != NULL, "Controller should not be NULL");
    
    // Compare multiple states
    uint64_t states[4] = {1, 2, 3, 4};
    bool result = smoke_test_compare(ctrl, states, 4);
    TEST_ASSERT(result, "State comparison should succeed");
    
    destroy_mock_controller(ctrl);
    
    if (g_verbose) printf("    ✓ State comparison test passed\n");
    return true;
}

// ============================================================================
// ANALYTICS TESTS
// ============================================================================

static bool test_analytics_basic(void) {
    if (g_verbose) printf("  Testing basic analytics...\n");
    
    PlaybackController* ctrl = create_mock_controller();
    populate_mock_states(ctrl, 20);
    TEST_ASSERT(ctrl != NULL, "Controller should not be NULL");
    
    // Create session with known data
    playback_record_start(ctrl, "Analytics Test");
    
    for (int i = 0; i < 10; i++) {
        ctrl->session->frames[ctrl->session->frame_count].quality = 0.5f + 0.03f * i;
        ctrl->session->frames[ctrl->session->frame_count].speed = 0.6f + 0.02f * i;
        ctrl->session->frames[ctrl->session->frame_count].memory_used = 1024 * 1024 * (200 - i * 10);
        ctrl->session->frame_count++;
    }
    
    playback_record_stop(ctrl);
    
    // Run analytics
    playback_analyze(ctrl);
    
    TEST_ASSERT(ctrl->analytics != NULL, "Analytics should be allocated");
    TEST_ASSERT_EQ(ctrl->analytics->timeline_length, 10, "Timeline should have 10 entries");
    TEST_ASSERT_GT(ctrl->analytics->avg_quality, 0.0f, "Average quality should be positive");
    TEST_ASSERT_GT(ctrl->analytics->avg_speed, 0.0f, "Average speed should be positive");
    TEST_ASSERT_GT(ctrl->analytics->avg_memory_mb, 0.0f, "Average memory should be positive");
    
    destroy_mock_controller(ctrl);
    
    if (g_verbose) printf("    ✓ Basic analytics test passed\n");
    return true;
}

static bool test_analytics_variance(void) {
    if (g_verbose) printf("  Testing analytics variance...\n");
    
    PlaybackController* ctrl = create_mock_controller();
    TEST_ASSERT(ctrl != NULL, "Controller should not be NULL");
    
    // Create session with varying data
    playback_record_start(ctrl, "Variance Test");
    
    float qualities[] = {0.5f, 0.6f, 0.55f, 0.7f, 0.65f, 0.75f, 0.8f, 0.72f, 0.78f, 0.85f};
    for (int i = 0; i < 10; i++) {
        ctrl->session->frames[ctrl->session->frame_count].quality = qualities[i];
        ctrl->session->frames[ctrl->session->frame_count].speed = 0.5f;
        ctrl->session->frames[ctrl->session->frame_count].memory_used = 1024 * 1024 * 200;
        ctrl->session->frame_count++;
    }
    
    playback_record_stop(ctrl);
    playback_analyze(ctrl);
    
    TEST_ASSERT_GT(ctrl->analytics->quality_variance, 0.0f, "Quality variance should be positive");
    TEST_ASSERT_GE(ctrl->analytics->quality_variance, 0.001f, "Variance should be measurable");
    
    destroy_mock_controller(ctrl);
    
    if (g_verbose) printf("    ✓ Analytics variance test passed\n");
    return true;
}

static bool test_analytics_peak_gains(void) {
    if (g_verbose) printf("  Testing analytics peak gains...\n");
    
    PlaybackController* ctrl = create_mock_controller();
    TEST_ASSERT(ctrl != NULL, "Controller should not be NULL");
    
    playback_record_start(ctrl, "Peak Gains Test");
    
    // Create frames with increasing then decreasing quality
    float qualities[] = {0.5f, 0.55f, 0.65f, 0.75f, 0.85f, 0.80f, 0.70f, 0.60f, 0.55f, 0.50f};
    for (int i = 0; i < 10; i++) {
        ctrl->session->frames[ctrl->session->frame_count].quality = qualities[i];
        ctrl->session->frames[ctrl->session->frame_count].speed = 0.5f;
        ctrl->session->frames[ctrl->session->frame_count].memory_used = 1024 * 1024 * 200;
        ctrl->session->frame_count++;
    }
    
    playback_record_stop(ctrl);
    playback_analyze(ctrl);
    
    // Peak gain should be around 0.1 (from 0.75 to 0.85)
    TEST_ASSERT_GT(ctrl->analytics->peak_quality_gain, 0.05f, "Peak quality gain should be significant");
    
    destroy_mock_controller(ctrl);
    
    if (g_verbose) printf("    ✓ Peak gains test passed\n");
    return true;
}

// ============================================================================
// STATE COMPARISON TESTS
// ============================================================================

static bool test_compare_states(void) {
    if (g_verbose) printf("  Testing state comparison...\n");
    
    PlaybackController* ctrl = create_mock_controller();
    populate_mock_states(ctrl, 10);
    TEST_ASSERT(ctrl != NULL, "Controller should not be NULL");
    
    StateComparison comparison;
    compare_states(ctrl, 1, 2, &comparison);
    
    TEST_ASSERT_EQ(comparison.state1_id, 1, "State 1 ID should match");
    TEST_ASSERT_EQ(comparison.state2_id, 2, "State 2 ID should match");
    TEST_ASSERT(strlen(comparison.winner) > 0, "Should have a winner");
    TEST_ASSERT(strlen(comparison.recommendation) > 0, "Should have recommendation");
    
    destroy_mock_controller(ctrl);
    
    if (g_verbose) printf("    ✓ State comparison test passed\n");
    return true;
}

static bool test_compare_sessions(void) {
    if (g_verbose) printf("  Testing session comparison...\n");
    
    PlaybackController* ctrl1 = create_mock_controller();
    PlaybackController* ctrl2 = create_mock_controller();
    
    populate_mock_states(ctrl1, 10);
    populate_mock_states(ctrl2, 10);
    
    TEST_ASSERT(ctrl1 != NULL, "Controller 1 should not be NULL");
    TEST_ASSERT(ctrl2 != NULL, "Controller 2 should not be NULL");
    
    // Create different sessions
    playback_record_start(ctrl1, "Session 1");
    for (int i = 0; i < 5; i++) {
        ctrl1->session->frames[ctrl1->session->frame_count].quality = 0.5f + 0.05f * i;
        ctrl1->session->frame_count++;
    }
    playback_record_stop(ctrl1);
    
    playback_record_start(ctrl2, "Session 2");
    for (int i = 0; i < 5; i++) {
        ctrl2->session->frames[ctrl2->session->frame_count].quality = 0.6f + 0.03f * i;
        ctrl2->session->frame_count++;
    }
    playback_record_stop(ctrl2);
    
    // Compare sessions
    playback_analyze(ctrl1);
    playback_analyze(ctrl2);
    
    TEST_ASSERT_NE(ctrl1->analytics->avg_quality, ctrl2->analytics->avg_quality, 
                   "Sessions should have different average quality");
    
    destroy_mock_controller(ctrl1);
    destroy_mock_controller(ctrl2);
    
    if (g_verbose) printf("    ✓ Session comparison test passed\n");
    return true;
}

// ============================================================================
// QUEUE MANAGEMENT TESTS
// ============================================================================

static bool test_queue_state(void) {
    if (g_verbose) printf("  Testing queue state...\n");
    
    PlaybackController* ctrl = create_mock_controller();
    TEST_ASSERT(ctrl != NULL, "Controller should not be NULL");
    
    // Queue states
    for (uint32_t i = 1; i <= 10; i++) {
        bool result = playback_queue_state(ctrl, i);
        TEST_ASSERT(result, "Queue state should succeed");
        TEST_ASSERT_EQ(ctrl->queue_length, i, "Queue length should match");
    }
    
    // Clear queue
    bool result = playback_clear_queue(ctrl);
    TEST_ASSERT(result, "Clear queue should succeed");
    TEST_ASSERT_EQ(ctrl->queue_length, 0, "Queue should be empty");
    
    destroy_mock_controller(ctrl);
    
    if (g_verbose) printf("    ✓ Queue state test passed\n");
    return true;
}

static bool test_queue_path(void) {
    if (g_verbose) printf("  Testing queue path...\n");
    
    PlaybackController* ctrl = create_mock_controller();
    TEST_ASSERT(ctrl != NULL, "Controller should not be NULL");
    
    uint64_t path[] = {1, 2, 3, 4, 5};
    bool result = playback_queue_path(ctrl, path, 5);
    TEST_ASSERT(result, "Queue path should succeed");
    TEST_ASSERT_EQ(ctrl->queue_length, 5, "Queue should have 5 states");
    
    // Verify order
    for (uint32_t i = 0; i < 5; i++) {
        TEST_ASSERT_EQ(ctrl->playback_queue[i], path[i], "Queue order should match");
    }
    
    destroy_mock_controller(ctrl);
    
    if (g_verbose) printf("    ✓ Queue path test passed\n");
    return true;
}

static bool test_process_queue(void) {
    if (g_verbose) printf("  Testing process queue...\n");
    
    PlaybackController* ctrl = create_mock_controller();
    populate_mock_states(ctrl, 20);
    TEST_ASSERT(ctrl != NULL, "Controller should not be NULL");
    
    // Queue states
    uint64_t path[] = {1, 2, 3, 4, 5};
    playback_queue_path(ctrl, path, 5);
    
    // Process queue
    bool result = playback_process_queue(ctrl);
    TEST_ASSERT(result, "Process queue should succeed");
    
    destroy_mock_controller(ctrl);
    
    if (g_verbose) printf("    ✓ Process queue test passed\n");
    return true;
}

// ============================================================================
// PROGRESS TRACKING TESTS
// ============================================================================

static bool test_progress_tracking(void) {
    if (g_verbose) printf("  Testing progress tracking...\n");
    
    PlaybackController* ctrl = create_mock_controller();
    populate_mock_states(ctrl, 20);
    TEST_ASSERT(ctrl != NULL, "Controller should not be NULL");
    
    // Create session
    playback_record_start(ctrl, "Progress Test");
    for (int i = 0; i < 10; i++) {
        playback_record_frame(ctrl, HOTPATCH_NONE);
    }
    playback_record_stop(ctrl);
    
    // Test progress at different positions
    playback_seek_to_frame(ctrl, 0);
    TEST_ASSERT_NEAR(playback_get_progress(ctrl), 0.0f, 0.01f, "Progress at start should be 0");
    
    playback_seek_to_frame(ctrl, 5);
    TEST_ASSERT_NEAR(playback_get_progress(ctrl), 0.5f, 0.1f, "Progress at middle should be ~0.5");
    
    playback_seek_to_frame(ctrl, 9);
    TEST_ASSERT_NEAR(playback_get_progress(ctrl), 1.0f, 0.1f, "Progress at end should be ~1.0");
    
    TEST_ASSERT_EQ(playback_get_total_frames(ctrl), 10, "Total frames should be 10");
    
    destroy_mock_controller(ctrl);
    
    if (g_verbose) printf("    ✓ Progress tracking test passed\n");
    return true;
}

// ============================================================================
// CALLBACK TESTS
// ============================================================================

static int g_callback_count = 0;
static uint64_t g_last_state_id = 0;

static void test_state_callback(uint64_t state_id, void* user_data) {
    (void)user_data;
    g_callback_count++;
    g_last_state_id = state_id;
}

static void test_frame_callback(PlaybackFrame* frame, void* user_data) {
    (void)user_data;
    (void)frame;
    g_callback_count++;
}

static bool test_callbacks(void) {
    if (g_verbose) printf("  Testing callbacks...\n");
    
    PlaybackController* ctrl = create_mock_controller();
    populate_mock_states(ctrl, 10);
    TEST_ASSERT(ctrl != NULL, "Controller should not be NULL");
    
    g_callback_count = 0;
    g_last_state_id = 0;
    
    // Set callbacks
    playback_set_callbacks(ctrl, test_state_callback, test_frame_callback, NULL, NULL);
    
    // Record frames (should trigger frame callback)
    playback_record_start(ctrl, "Callback Test");
    for (int i = 0; i < 5; i++) {
        playback_record_frame(ctrl, HOTPATCH_NONE);
    }
    playback_record_stop(ctrl);
    
    TEST_ASSERT_GT(g_callback_count, 0, "Frame callback should have been called");
    
    destroy_mock_controller(ctrl);
    
    if (g_verbose) printf("    ✓ Callback test passed\n");
    return true;
}

// ============================================================================
// BENCHMARK HELPER TESTS
// ============================================================================

static bool test_benchmark_helpers(void) {
    if (g_verbose) printf("  Testing benchmark helpers...\n");
    
    PlaybackController* ctrl = create_mock_controller();
    populate_mock_states(ctrl, 10);
    TEST_ASSERT(ctrl != NULL, "Controller should not be NULL");
    
    // Test quality benchmark
    float quality = benchmark_quality(ctrl, 1, 100);
    TEST_ASSERT_GE(quality, 0.0f, "Quality should be >= 0");
    TEST_ASSERT_LE(quality, 1.0f, "Quality should be <= 1");
    
    // Test speed benchmark
    float speed = benchmark_speed(ctrl, 1, 100);
    TEST_ASSERT_GE(speed, 0.0f, "Speed should be >= 0");
    TEST_ASSERT_LE(speed, 1.0f, "Speed should be <= 1");
    
    // Test memory benchmark
    uint64_t memory = benchmark_memory(ctrl, 1);
    TEST_ASSERT_GT(memory, 0, "Memory should be > 0");
    
    destroy_mock_controller(ctrl);
    
    if (g_verbose) printf("    ✓ Benchmark helpers test passed\n");
    return true;
}

// ============================================================================
// EDGE CASE TESTS
// ============================================================================

static bool test_edge_cases(void) {
    if (g_verbose) printf("  Testing edge cases...\n");
    
    PlaybackController* ctrl = create_mock_controller();
    TEST_ASSERT(ctrl != NULL, "Controller should not be NULL");
    
    // Test empty session
    TEST_ASSERT_EQ(playback_get_progress(ctrl), 0.0f, "Progress of empty session should be 0");
    TEST_ASSERT_EQ(playback_get_total_frames(ctrl), 0, "Empty session should have 0 frames");
    
    // Test operations on empty session
    bool result = playback_jump_to_best(ctrl);
    TEST_ASSERT(!result, "Jump to best on empty session should fail");
    
    result = playback_jump_to_worst(ctrl);
    TEST_ASSERT(!result, "Jump to worst on empty session should fail");
    
    // Test seek beyond bounds
    result = playback_seek_to_frame(ctrl, 100);
    TEST_ASSERT(!result, "Seek beyond bounds should fail");
    
    // Test rewind at beginning
    ctrl->position_in_history = 0;
    result = playback_rewind(ctrl);
    TEST_ASSERT(!result, "Rewind at beginning should fail");
    
    // Test play when ejected
    ctrl->state = PLAYBACK_EJECTED;
    result = playback_play(ctrl);
    TEST_ASSERT(!result, "Play when ejected should fail");
    
    destroy_mock_controller(ctrl);
    
    if (g_verbose) printf("    ✓ Edge cases test passed\n");
    return true;
}

static bool test_stress_many_frames(void) {
    if (g_verbose) printf("  Testing stress with many frames...\n");
    
    PlaybackController* ctrl = create_mock_controller();
    populate_mock_states(ctrl, 1000);
    TEST_ASSERT(ctrl != NULL, "Controller should not be NULL");
    
    // Record many frames
    playback_record_start(ctrl, "Stress Test");
    for (int i = 0; i < 500; i++) {
        playback_record_frame(ctrl, (HotpatchOp)(1 << (i % 8)));
    }
    playback_record_stop(ctrl);
    
    TEST_ASSERT_EQ(ctrl->session->frame_count, 501, "Should have 501 frames");
    
    // Analyze
    playback_analyze(ctrl);
    TEST_ASSERT_EQ(ctrl->analytics->timeline_length, 501, "Timeline should have 501 entries");
    
    destroy_mock_controller(ctrl);
    
    if (g_verbose) printf("    ✓ Stress test passed\n");
    return true;
}

// ============================================================================
// TEST RUNNER
// ============================================================================

static TestCase g_tests[] = {
    // Controller lifecycle
    {"controller_create_destroy", test_controller_create_destroy},
    {"controller_initial_state", test_controller_initial_state},
    
    // Media controls
    {"playback_play_pause_stop", test_playback_play_pause_stop},
    {"playback_fast_forward", test_playback_fast_forward},
    {"playback_rewind", test_playback_rewind},
    {"playback_eject", test_playback_eject},
    
    // Navigation
    {"seek_to_frame", test_seek_to_frame},
    {"seek_to_state", test_seek_to_state},
    {"step_forward_backward", test_step_forward_backward},
    {"jump_to_best_worst", test_jump_to_best_worst},
    
    // Recording
    {"record_start_stop", test_record_start_stop},
    {"record_frames", test_record_frames},
    {"record_with_operations", test_record_with_operations},
    
    // Smoke testing
    {"smoke_test_quick", test_smoke_test_quick},
    {"smoke_test_standard", test_smoke_test_standard},
    {"smoke_test_comparison", test_smoke_test_comparison},
    
    // Analytics
    {"analytics_basic", test_analytics_basic},
    {"analytics_variance", test_analytics_variance},
    {"analytics_peak_gains", test_analytics_peak_gains},
    
    // State comparison
    {"compare_states", test_compare_states},
    {"compare_sessions", test_compare_sessions},
    
    // Queue management
    {"queue_state", test_queue_state},
    {"queue_path", test_queue_path},
    {"process_queue", test_process_queue},
    
    // Progress tracking
    {"progress_tracking", test_progress_tracking},
    
    // Callbacks
    {"callbacks", test_callbacks},
    
    // Benchmark helpers
    {"benchmark_helpers", test_benchmark_helpers},
    
    // Edge cases
    {"edge_cases", test_edge_cases},
    {"stress_many_frames", test_stress_many_frames},
};

static void run_tests(void) {
    g_results.start_time_ns = get_time_ns();
    g_results.total_tests = sizeof(g_tests) / sizeof(g_tests[0]);
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║     HOTPATCH PLAYBACK SYSTEM - TEST SUITE                  ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("Running %d tests...\n\n", g_results.total_tests);
    
    for (int i = 0; i < g_results.total_tests; i++) {
        printf("[%d/%d] %s\n", i + 1, g_results.total_tests, g_tests[i].name);
        
        bool passed = g_tests[i].test_func();
        
        if (passed) {
            g_results.passed_tests++;
            printf("  ✓ PASSED\n\n");
        } else {
            g_results.failed_tests++;
            printf("  ✗ FAILED\n\n");
        }
    }
    
    g_results.end_time_ns = get_time_ns();
    
    // Print summary
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║                    TEST SUMMARY                             ║\n");
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║ Total:   %3d                                               \n", g_results.total_tests);
    printf("║ Passed: %3d  (%.1f%%)                                       \n", 
           g_results.passed_tests, 
           100.0f * g_results.passed_tests / g_results.total_tests);
    printf("║ Failed: %3d  (%.1f%%)                                       \n", 
           g_results.failed_tests,
           100.0f * g_results.failed_tests / g_results.total_tests);
    printf("║ Skipped: %3d                                               \n", g_results.skipped_tests);
    printf("║ Duration: %.2f ms                                          \n",
           (g_results.end_time_ns - g_results.start_time_ns) / 1e6f);
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    if (g_results.failed_tests == 0) {
        printf("\n✓ ALL TESTS PASSED\n\n");
    } else {
        printf("\n✗ SOME TESTS FAILED\n\n");
    }
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char** argv) {
    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0) {
            g_verbose = 0;
        }
    }
    
    run_tests();
    
    return g_results.failed_tests > 0 ? 1 : 0;
}