// hotpatch_playback.c - Media-Style Controls for Smoke Testing Implementation
// Playback, recording, and analysis of hotpatch sessions
// Part of RawrXD Progressive Layer Loading System

#define HOTPATCH_PLAYBACK_IMPLEMENTATION
#include "hotpatch_playback.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#include <sys/time.h>
#endif

// ============================================================================
// UTILITY
// ============================================================================

uint64_t get_time_ns(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (uint64_t)(count.QuadPart * 1000000000ULL / freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
#endif
}

// ============================================================================
// CONTROLLER LIFECYCLE
// ============================================================================

PlaybackController* playback_create(OmnidirectionalHotpatch* omni) {
    PlaybackController* ctrl = (PlaybackController*)calloc(1, sizeof(PlaybackController));
    if (!ctrl) return NULL;
    
    ctrl->omni = omni;
    ctrl->state = PLAYBACK_STOPPED;
    ctrl->speed = SPEED_1X;
    ctrl->direction = DIRECTION_FORWARD;
    
    // Allocate session
    ctrl->session = (TestSession*)calloc(1, sizeof(TestSession));
    if (!ctrl->session) {
        free(ctrl);
        return NULL;
    }
    ctrl->session->frame_capacity = 4096;
    ctrl->session->frames = (PlaybackFrame*)calloc(ctrl->session->frame_capacity, sizeof(PlaybackFrame));
    if (!ctrl->session->frames) {
        free(ctrl->session);
        free(ctrl);
        return NULL;
    }
    
    // Allocate analytics
    ctrl->analytics = (PlaybackAnalytics*)calloc(1, sizeof(PlaybackAnalytics));
    if (!ctrl->analytics) {
        free(ctrl->session->frames);
        free(ctrl->session);
        free(ctrl);
        return NULL;
    }
    
    // Allocate playback queue
    ctrl->queue_capacity = 256;
    ctrl->playback_queue = (uint64_t*)calloc(ctrl->queue_capacity, sizeof(uint64_t));
    if (!ctrl->playback_queue) {
        free(ctrl->analytics);
        free(ctrl->session->frames);
        free(ctrl->session);
        free(ctrl);
        return NULL;
    }
    
    // Initialize current state
    if (omni && omni->state_graph && omni->state_graph->state_count > 0) {
        ctrl->current_state_id = omni->state_graph->states[0].state_id;
    }
    
    // Default smoke test config
    ctrl->smoke_config.mode = SMOKE_STANDARD;
    ctrl->smoke_config.max_iterations = 100;
    ctrl->smoke_config.max_time_seconds = 300;
    ctrl->smoke_config.test_operations = HOTPATCH_ALL;
    ctrl->smoke_config.exploration_rate = 0.3f;
    ctrl->smoke_config.validate_each_step = true;
    ctrl->smoke_config.validation_tokens = 100;
    ctrl->smoke_config.record_session = true;
    ctrl->smoke_config.generate_report = true;
    
    // Initialize best/worst tracking
    ctrl->session->best_quality_score = -FLT_MAX;
    ctrl->session->best_speed_score = -FLT_MAX;
    ctrl->session->best_memory_bytes = UINT64_MAX;
    
    return ctrl;
}

void playback_destroy(PlaybackController* controller) {
    if (!controller) return;
    
    if (controller->session) {
        free(controller->session->frames);
        free(controller->session);
    }
    
    if (controller->analytics) {
        free(controller->analytics->quality_timeline);
        free(controller->analytics->speed_timeline);
        free(controller->analytics->memory_timeline);
        free(controller->analytics->state_visit_counts);
        free(controller->analytics);
    }
    
    free(controller->playback_queue);
    free(controller);
}

// ============================================================================
// MEDIA CONTROLS
// ============================================================================

bool playback_play(PlaybackController* controller) {
    if (controller->state == PLAYBACK_PLAYING) {
        return true; // Already playing
    }
    
    if (controller->state == PLAYBACK_EJECTED) {
        printf("[PLAYBACK] Cannot play - media ejected\n");
        return false;
    }
    
    printf("[PLAYBACK] ▶ PLAY - Resuming from state #%lu\n", 
           (unsigned long)controller->current_state_id);
    
    controller->state = PLAYBACK_PLAYING;
    controller->speed = SPEED_1X;
    controller->direction = DIRECTION_FORWARD;
    controller->benchmark_start_time = get_time_ns();
    
    return true;
}

bool playback_pause(PlaybackController* controller) {
    if (controller->state != PLAYBACK_PLAYING && 
        controller->state != PLAYBACK_FAST_FORWARDING &&
        controller->state != PLAYBACK_REWINDING) {
        printf("[PLAYBACK] Cannot pause - not playing\n");
        return false;
    }
    
    printf("[PLAYBACK] ⏸ PAUSE - Paused at state #%lu\n", 
           (unsigned long)controller->current_state_id);
    
    // Record frame if recording
    if (controller->is_recording) {
        playback_record_frame(controller, HOTPATCH_NONE);
    }
    
    controller->state = PLAYBACK_PAUSED;
    
    return true;
}

bool playback_stop(PlaybackController* controller) {
    printf("[PLAYBACK] ⏹ STOP - Stopped at state #%lu\n", 
           (unsigned long)controller->current_state_id);
    
    controller->state = PLAYBACK_STOPPED;
    controller->speed = SPEED_1X;
    controller->direction = DIRECTION_FORWARD;
    
    // Finalize session
    if (controller->is_recording) {
        playback_record_stop(controller);
    }
    
    return true;
}

bool playback_fast_forward(PlaybackController* controller, PlaybackSpeed speed) {
    if (controller->state == PLAYBACK_EJECTED) {
        printf("[PLAYBACK] Cannot fast forward - media ejected\n");
        return false;
    }
    
    printf("[PLAYBACK] ⏩ FAST FORWARD - Speed %dx, state #%lu\n",
           speed, (unsigned long)controller->current_state_id);
    
    controller->state = PLAYBACK_FAST_FORWARDING;
    controller->speed = speed;
    controller->direction = DIRECTION_FORWARD;
    
    // Process multiple frames quickly
    uint32_t frames_to_process = (uint32_t)speed;
    
    for (uint32_t i = 0; i < frames_to_process; i++) {
        if (!playback_step_forward(controller)) {
            break; // End of playback queue
        }
    }
    
    return true;
}

bool playback_rewind(PlaybackController* controller) {
    if (controller->state == PLAYBACK_EJECTED) {
        printf("[PLAYBACK] Cannot rewind - media ejected\n");
        return false;
    }
    
    if (controller->position_in_history == 0) {
        printf("[PLAYBACK] ⏪ Cannot rewind - at beginning\n");
        return false;
    }
    
    printf("[PLAYBACK] ⏪ REWIND - Back to state #%lu\n",
           (unsigned long)controller->current_state_id);
    
    controller->state = PLAYBACK_REWINDING;
    controller->direction = DIRECTION_BACKWARD;
    
    // Go back one step
    return playback_step_backward(controller);
}

bool playback_eject(PlaybackController* controller, const char* export_path) {
    printf("[PLAYBACK] ⏏ EJECT - Exporting session\n");
    
    controller->state = PLAYBACK_EJECTED;
    
    // Generate final report
    if (controller->smoke_config.generate_report && export_path) {
        playback_generate_report(controller, export_path);
    }
    
    // Export session data
    if (controller->session && controller->session->frame_count > 0) {
        char session_path[512];
        snprintf(session_path, sizeof(session_path), "%s_session.json", export_path);
        playback_export_timeline(controller, session_path);
    }
    
    // Visualize gains
    if (export_path) {
        char viz_path[512];
        snprintf(viz_path, sizeof(viz_path), "%s_gains.txt", export_path);
        playback_visualize_gains(controller, viz_path);
    }
    
    printf("[PLAYBACK] Session exported to: %s\n", export_path ? export_path : "(null)");
    
    return true;
}

// ============================================================================
// NAVIGATION CONTROLS
// ============================================================================

bool playback_seek_to_frame(PlaybackController* controller, uint32_t frame_index) {
    if (frame_index >= controller->session->frame_count) {
        printf("[PLAYBACK] Cannot seek - frame %u out of range (0-%u)\n",
               frame_index, controller->session->frame_count - 1);
        return false;
    }
    
    PlaybackFrame* frame = &controller->session->frames[frame_index];
    controller->current_frame_index = frame_index;
    controller->current_state_id = frame->state_id;
    controller->position_in_history = frame_index;
    
    printf("[PLAYBACK] Seeking to frame %u (state #%lu)\n",
           frame_index, (unsigned long)frame->state_id);
    
    // Navigate to state
    NavigationResult result = navigate_to_state(controller->omni, frame->state_id);
    
    return result.success;
}

bool playback_seek_to_state(PlaybackController* controller, uint64_t state_id) {
    // Find frame for this state
    for (uint32_t i = 0; i < controller->session->frame_count; i++) {
        if (controller->session->frames[i].state_id == state_id) {
            return playback_seek_to_frame(controller, i);
        }
    }
    
    // State not in session, navigate directly
    NavigationResult result = navigate_to_state(controller->omni, state_id);
    
    if (result.success) {
        controller->current_state_id = state_id;
        
        // Record as new frame
        if (controller->is_recording) {
            playback_record_frame(controller, HOTPATCH_NONE);
        }
    }
    
    return result.success;
}

bool playback_seek_to_time(PlaybackController* controller, uint32_t seconds) {
    // Find frame at approximately this time
    uint64_t target_time = controller->session->start_time + 
                          (uint64_t)seconds * 1000000000ULL;
    
    for (uint32_t i = 0; i < controller->session->frame_count; i++) {
        if (controller->session->frames[i].timestamp >= target_time) {
            return playback_seek_to_frame(controller, i);
        }
    }
    
    // Seek to end
    return playback_seek_to_frame(controller, controller->session->frame_count - 1);
}

bool playback_step_forward(PlaybackController* controller) {
    // Get next state from queue or state graph
    if (controller->queue_position < controller->queue_length) {
        uint64_t next_state = controller->playback_queue[controller->queue_position++];
        
        NavigationResult result = navigate_to_state(controller->omni, next_state);
        if (result.success) {
            controller->current_state_id = next_state;
            
            if (controller->is_recording) {
                playback_record_frame(controller, HOTPATCH_NONE);
            }
            
            return true;
        }
    }
    
    // No more in queue, try to find next state in graph
    HotpatchState* current = get_state(controller->omni, controller->current_state_id);
    
    if (current && current->child_count > 0) {
        // Navigate to first child
        uint64_t next_state = current->child_states[0];
        return playback_seek_to_state(controller, next_state);
    }
    
    printf("[PLAYBACK] End of playback (no more states)\n");
    return false;
}

bool playback_step_backward(PlaybackController* controller) {
    if (controller->current_frame_index == 0) {
        printf("[PLAYBACK] Already at beginning\n");
        return false;
    }
    
    return playback_seek_to_frame(controller, controller->current_frame_index - 1);
}

bool playback_jump_to_best(PlaybackController* controller) {
    if (controller->session->frame_count == 0) {
        printf("[PLAYBACK] No frames in session\n");
        return false;
    }
    
    // Find best quality frame
    float best_quality = 0.0f;
    uint32_t best_idx = 0;
    
    for (uint32_t i = 0; i < controller->session->frame_count; i++) {
        if (controller->session->frames[i].quality > best_quality) {
            best_quality = controller->session->frames[i].quality;
            best_idx = i;
        }
    }
    
    printf("[PLAYBACK] Jumping to best quality state (frame %u, quality %.4f)\n",
           best_idx, best_quality);
    
    return playback_seek_to_frame(controller, best_idx);
}

bool playback_jump_to_worst(PlaybackController* controller) {
    if (controller->session->frame_count == 0) {
        printf("[PLAYBACK] No frames in session\n");
        return false;
    }
    
    // Find worst quality frame
    float worst_quality = 1.0f;
    uint32_t worst_idx = 0;
    
    for (uint32_t i = 0; i < controller->session->frame_count; i++) {
        if (controller->session->frames[i].quality < worst_quality) {
            worst_quality = controller->session->frames[i].quality;
            worst_idx = i;
        }
    }
    
    printf("[PLAYBACK] Jumping to worst quality state (frame %u, quality %.4f)\n",
           worst_idx, worst_quality);
    
    return playback_seek_to_frame(controller, worst_idx);
}

// ============================================================================
// RECORDING
// ============================================================================

bool playback_record_start(PlaybackController* controller, const char* session_name) {
    printf("[PLAYBACK] ● RECORD - Starting recording: %s\n", 
           session_name ? session_name : "Unnamed Session");
    
    controller->is_recording = true;
    
    // Initialize session
    memset(controller->session, 0, sizeof(TestSession));
    controller->session->session_id = (uint64_t)time(NULL);
    controller->session->start_time = get_time_ns();
    
    if (session_name) {
        strncpy(controller->session->session_name, session_name, 
                sizeof(controller->session->session_name) - 1);
    }
    
    // Initialize best/worst tracking
    controller->session->best_quality_score = -FLT_MAX;
    controller->session->best_speed_score = -FLT_MAX;
    controller->session->best_memory_bytes = UINT64_MAX;
    
    // Record initial frame
    return playback_record_frame(controller, HOTPATCH_NONE);
}

bool playback_record_stop(PlaybackController* controller) {
    if (!controller->is_recording) {
        return true;
    }
    
    printf("[PLAYBACK] ⏹ Stop Recording - %u frames recorded\n",
           controller->session->frame_count);
    
    controller->is_recording = false;
    controller->session->end_time = get_time_ns();
    
    // Calculate session statistics
    playback_analyze(controller);
    
    return true;
}

bool playback_record_frame(PlaybackController* controller, HotpatchOp operation) {
    if (!controller->is_recording) {
        return false;
    }
    
    if (controller->session->frame_count >= controller->session->frame_capacity) {
        // Grow frame buffer
        uint32_t new_capacity = controller->session->frame_capacity * 2;
        PlaybackFrame* new_frames = (PlaybackFrame*)realloc(controller->session->frames,
                                            new_capacity * sizeof(PlaybackFrame));
        if (!new_frames) return false;
        
        controller->session->frames = new_frames;
        controller->session->frame_capacity = new_capacity;
    }
    
    PlaybackFrame* frame = &controller->session->frames[controller->session->frame_count];
    memset(frame, 0, sizeof(PlaybackFrame));
    
    frame->timestamp = get_time_ns();
    frame->state_id = controller->current_state_id;
    frame->playback_state = controller->state;
    frame->speed = controller->speed;
    frame->operation = operation;
    
    // Get current state metrics
    HotpatchState* state = get_state(controller->omni, controller->current_state_id);
    if (state) {
        frame->quality = state->quality_score;
        frame->speed = state->speed_score;
        frame->memory_used = state->memory_usage;
        
        // Track best/worst
        if (frame->quality > controller->session->best_quality_score) {
            controller->session->best_quality_score = frame->quality;
            controller->session->best_quality_state = frame->state_id;
        }
        if (frame->speed > controller->session->best_speed_score) {
            controller->session->best_speed_score = frame->speed;
            controller->session->best_speed_state = frame->state_id;
        }
        if (frame->memory_used < controller->session->best_memory_bytes) {
            controller->session->best_memory_bytes = frame->memory_used;
            controller->session->best_memory_state = frame->state_id;
        }
    }
    
    // Calculate cumulative gains
    if (controller->session->frame_count > 0) {
        PlaybackFrame* prev_frame = &controller->session->frames[controller->session->frame_count - 1];
        frame->quality_gain = frame->quality - prev_frame->quality;
        frame->speed_gain = frame->speed - prev_frame->speed;
        frame->memory_saved = (int64_t)frame->memory_used - (int64_t)prev_frame->memory_used;
        
        controller->session->total_quality_gain += frame->quality_gain;
        controller->session->total_speed_gain += frame->speed_gain;
        controller->session->total_memory_saved += frame->memory_saved;
    }
    
    // Update counters
    if (operation != HOTPATCH_NONE) {
        controller->session->total_hotpatches++;
    }
    
    snprintf(frame->description, sizeof(frame->description),
             "State #%lu, Q=%.4f, S=%.4f, M=%luMB",
             (unsigned long)frame->state_id, frame->quality, frame->speed,
             (unsigned long)(frame->memory_used / (1024 * 1024)));
    
    controller->session->frame_count++;
    
    // Callback
    if (controller->on_frame_recorded) {
        controller->on_frame_recorded(frame, controller->user_data);
    }
    
    return true;
}

TestSession* playback_get_recorded_session(PlaybackController* controller) {
    return controller->session;
}

// ============================================================================
// SMOKE TESTING
// ============================================================================

bool smoke_test_start(PlaybackController* controller, SmokeTestConfig* config) {
    if (controller->smoke_test_active) {
        printf("[SMOKE] Test already active\n");
        return false;
    }
    
    printf("[SMOKE] ════════════════════════════════════════════════════════\n");
    printf("[SMOKE] STARTING SMOKE TEST: %s\n",
           config->mode == SMOKE_QUICK ? "QUICK" :
           config->mode == SMOKE_STANDARD ? "STANDARD" :
           config->mode == SMOKE_THOROUGH ? "THOROUGH" :
           config->mode == SMOKE_EXHAUSTIVE ? "EXHAUSTIVE" :
           config->mode == SMOKE_STRESS ? "STRESS" : "COMPARISON");
    printf("[SMOKE] ════════════════════════════════════════════════════════\n");
    
    controller->smoke_config = *config;
    controller->smoke_test_active = true;
    controller->smoke_test_iteration = 0;
    controller->smoke_test_start_time = get_time_ns();
    
    // Start recording
    if (config->record_session) {
        char session_name[256];
        snprintf(session_name, sizeof(session_name), "SmokeTest_%s_%lu",
                 config->mode == SMOKE_QUICK ? "Quick" :
                 config->mode == SMOKE_STANDARD ? "Standard" :
                 config->mode == SMOKE_THOROUGH ? "Thorough" :
                 config->mode == SMOKE_EXHAUSTIVE ? "Exhaustive" :
                 config->mode == SMOKE_STRESS ? "Stress" : "Comparison",
                 (unsigned long)controller->smoke_test_start_time);
        playback_record_start(controller, session_name);
    }
    
    // Start playback
    playback_play(controller);
    
    return true;
}

bool smoke_test_stop(PlaybackController* controller) {
    if (!controller->smoke_test_active) {
        return true;
    }
    
    printf("[SMOKE] ════════════════════════════════════════════════════════\n");
    printf("[SMOKE] STOPPING SMOKE TEST\n");
    printf("[SMOKE] ════════════════════════════════════════════════════════\n");
    
    controller->smoke_test_active = false;
    
    // Stop recording
    if (controller->is_recording) {
        playback_record_stop(controller);
    }
    
    // Stop playback
    playback_stop(controller);
    
    // Generate report
    if (controller->smoke_config.generate_report) {
        playback_generate_report(controller, "smoke_test_report.md");
    }
    
    return true;
}

bool smoke_test_step(PlaybackController* controller) {
    if (!controller->smoke_test_active) {
        return false;
    }
    
    controller->smoke_test_iteration++;
    
    // Check limits
    if (controller->smoke_config.max_iterations > 0 &&
        controller->smoke_test_iteration >= controller->smoke_config.max_iterations) {
        printf("[SMOKE] Max iterations reached (%u)\n", controller->smoke_config.max_iterations);
        smoke_test_stop(controller);
        return false;
    }
    
    uint64_t elapsed = get_time_ns() - controller->smoke_test_start_time;
    if (controller->smoke_config.max_time_seconds > 0 &&
        elapsed >= (uint64_t)controller->smoke_config.max_time_seconds * 1000000000ULL) {
        printf("[SMOKE] Max time reached (%u seconds)\n", controller->smoke_config.max_time_seconds);
        smoke_test_stop(controller);
        return false;
    }
    
    // Generate test operation based on exploration rate
    HotpatchOp ops = HOTPATCH_NONE;
    float rand_val = (float)rand() / RAND_MAX;
    
    if (rand_val < controller->smoke_config.exploration_rate) {
        // Explore: try random operations
        uint32_t op_count = rand() % 3 + 1;
        for (uint32_t i = 0; i < op_count; i++) {
            uint32_t op_idx = rand() % 8;
            switch (op_idx) {
                case 0: ops |= HOTPATCH_PRUNE_WEIGHTS; break;
                case 1: ops |= HOTPATCH_QUANTIZE; break;
                case 2: ops |= HOTPATCH_COMPRESS_KV; break;
                case 3: ops |= HOTPATCH_PRUNE_HEADS; break;
                case 4: ops |= HOTPATCH_FUSE_LAYERS; break;
                case 5: ops |= HOTPATCH_OPTIMIZE_ATTENTION; break;
                case 6: ops |= HOTPATCH_MERGE_EMBEDDINGS; break;
                case 7: ops |= HOTPATCH_PRUNE_EXPERTS; break;
            }
        }
    } else {
        // Exploit: move toward better states
        NavigationResult result = navigate_to_best(controller->omni);
        if (result.success) {
            controller->current_state_id = result.to_state_id;
            playback_record_frame(controller, ops);
            return true;
        }
    }
    
    // Apply operation
    if (ops != HOTPATCH_NONE) {
        printf("[SMOKE] Iteration %u: Testing ops 0x%X\n", 
               controller->smoke_test_iteration, ops);
        
        if (controller->smoke_config.test_operations & ops) {
            bool success = apply_omni_hotpatch(controller->omni, ops, NULL, 0);
            
            if (success) {
                controller->current_state_id = controller->omni->current_state_id;
                playback_record_frame(controller, ops);
                
                // Validate if configured
                if (controller->smoke_config.validate_each_step) {
                    float quality = benchmark_quality(controller, 
                                                       controller->current_state_id,
                                                       controller->smoke_config.validation_tokens);
                    
                    if (quality < controller->smoke_config.quality_degradation_max) {
                        printf("[SMOKE] WARNING: Quality dropped below threshold (%.4f < %.4f)\n",
                               quality, controller->smoke_config.quality_degradation_max);
                    }
                }
            } else {
                controller->session->failed_operations++;
                printf("[SMOKE] Operation failed, rollback performed\n");
            }
        }
    }
    
    return true;
}

SmokeTestResult smoke_test_run(PlaybackController* controller, SmokeTestConfig* config) {
    SmokeTestResult result;
    memset(&result, 0, sizeof(result));
    
    // Start test
    if (!smoke_test_start(controller, config)) {
        result.passed = false;
        snprintf(result.summary, sizeof(result.summary), "Failed to start smoke test");
        return result;
    }
    
    // Get initial metrics
    float initial_quality = 0.0f;
    float initial_speed = 0.0f;
    uint64_t initial_memory = 0;
    
    HotpatchState* initial_state = get_state(controller->omni, controller->current_state_id);
    if (initial_state) {
        initial_quality = initial_state->quality_score;
        initial_speed = initial_state->speed_score;
        initial_memory = initial_state->memory_usage;
    }
    
    printf("[SMOKE] Initial: Q=%.4f, S=%.4f, M=%luMB\n",
           initial_quality, initial_speed, (unsigned long)(initial_memory / (1024 * 1024)));
    
    // Run test iterations
    while (controller->smoke_test_active) {
        if (!smoke_test_step(controller)) {
            break;
        }
    }
    
    // Get final metrics
    float final_quality = 0.0f;
    float final_speed = 0.0f;
    uint64_t final_memory = 0;
    
    HotpatchState* final_state = get_state(controller->omni, controller->current_state_id);
    if (final_state) {
        final_quality = final_state->quality_score;
        final_speed = final_state->speed_score;
        final_memory = final_state->memory_usage;
    }
    
    // Calculate results
    result.final_quality = final_quality;
    result.final_speed = final_speed;
    result.final_memory = final_memory;
    result.quality_improvement = final_quality - initial_quality;
    result.speed_improvement = final_speed - initial_speed;
    if (initial_memory > 0) {
        result.memory_improvement = 100.0f * (initial_memory - final_memory) / initial_memory;
    }
    result.states_tested = controller->omni->state_graph->state_count;
    result.hotpatches_applied = controller->session->total_hotpatches;
    result.failures = controller->session->failed_operations;
    result.test_duration_ns = get_time_ns() - controller->smoke_test_start_time;
    
    // Determine pass/fail
    result.passed = true;
    if (config->target_quality_min > 0 && final_quality < config->target_quality_min) {
        result.passed = false;
    }
    if (config->target_speed_min > 0 && final_speed < config->target_speed_min) {
        result.passed = false;
    }
    if (config->target_memory_max > 0 && final_memory > config->target_memory_max) {
        result.passed = false;
    }
    if (result.failures > controller->session->total_hotpatches / 4) {
        result.passed = false; // Too many failures
    }
    
    // Generate summary
    snprintf(result.summary, sizeof(result.summary),
             "SMOKE TEST %s\n"
             "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n"
             "Initial:  Q=%.4f  S=%.4f  M=%luMB\n"
             "Final:    Q=%.4f  S=%.4f  M=%luMB\n"
             "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n"
             "Quality:  %+.4f (%+.2f%%)\n"
             "Speed:    %+.4f (%+.2f%%)\n"
             "Memory:   %.1f%% saved\n"
             "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n"
             "States tested:    %u\n"
             "Hotpatches:       %u\n"
             "Failures:         %u\n"
             "Duration:         %.2f seconds\n"
             "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n"
             "RESULT: %s",
             result.passed ? "PASSED ✓" : "FAILED ✗",
             initial_quality, initial_speed, (unsigned long)(initial_memory / (1024 * 1024)),
             final_quality, final_speed, (unsigned long)(final_memory / (1024 * 1024)),
             result.quality_improvement, 
             initial_quality > 0 ? 100.0f * result.quality_improvement / initial_quality : 0,
             result.speed_improvement,
             initial_speed > 0 ? 100.0f * result.speed_improvement / initial_speed : 0,
             result.memory_improvement,
             result.states_tested,
             result.hotpatches_applied,
             result.failures,
             result.test_duration_ns / 1e9f,
             result.passed ? "PASS ✓" : "FAIL ✗");
    
    printf("\n%s\n", result.summary);
    
    return result;
}

bool smoke_test_compare(PlaybackController* controller, uint64_t* state_ids, uint32_t count) {
    if (count < 2) {
        printf("[SMOKE] Need at least 2 states to compare\n");
        return false;
    }
    
    printf("[SMOKE] Comparing %u states:\n", count);
    printf("┌────────┬─────────┬─────────┬──────────┬─────────┐\n");
    printf("│ State  │ Quality │ Speed   │ Memory   │ Score   │\n");
    printf("├────────┼─────────┼─────────┼──────────┼─────────┤\n");
    
    for (uint32_t i = 0; i < count; i++) {
        HotpatchState* state = get_state(controller->omni, state_ids[i]);
        if (!state) continue;
        
        float score = state->quality_score * 0.4f +
                     state->speed_score * 0.3f +
                     (1.0f - state->state_vector[AXIS_MEMORY]) * 0.3f;
        
        printf("│ #%4lu │ %.4f  │ %.4f  │ %6luMB │ %.4f  │\n",
               (unsigned long)state_ids[i], state->quality_score, state->speed_score,
               (unsigned long)(state->memory_usage / (1024 * 1024)), score);
    }
    
    printf("└────────┴─────────┴─────────┴──────────┴─────────┘\n");
    
    return true;
}

// ============================================================================
// ANALYTICS
// ============================================================================

void playback_analyze(PlaybackController* controller) {
    if (!controller->session || controller->session->frame_count == 0) {
        return;
    }
    
    printf("[PLAYBACK] Analyzing %u frames...\n", controller->session->frame_count);
    
    PlaybackAnalytics* analytics = controller->analytics;
    
    // Allocate timeline arrays
    analytics->timeline_length = controller->session->frame_count;
    analytics->quality_timeline = (float*)calloc(analytics->timeline_length, sizeof(float));
    analytics->speed_timeline = (float*)calloc(analytics->timeline_length, sizeof(float));
    analytics->memory_timeline = (uint64_t*)calloc(analytics->timeline_length, sizeof(uint64_t));
    
    // Fill timeline and calculate averages
    float quality_sum = 0.0f;
    float speed_sum = 0.0f;
    uint64_t memory_sum = 0;
    
    for (uint32_t i = 0; i < analytics->timeline_length; i++) {
        PlaybackFrame* frame = &controller->session->frames[i];
        
        analytics->quality_timeline[i] = frame->quality;
        analytics->speed_timeline[i] = frame->speed;
        analytics->memory_timeline[i] = frame->memory_used;
        
        quality_sum += frame->quality;
        speed_sum += frame->speed;
        memory_sum += frame->memory_used;
    }
    
    analytics->avg_quality = quality_sum / analytics->timeline_length;
    analytics->avg_speed = speed_sum / analytics->timeline_length;
    analytics->avg_memory_mb = (float)memory_sum / analytics->timeline_length / (1024.0f * 1024.0f);
    
    // Calculate variance
    float quality_var_sum = 0.0f;
    float speed_var_sum = 0.0f;
    
    for (uint32_t i = 0; i < analytics->timeline_length; i++) {
        float q_diff = analytics->quality_timeline[i] - analytics->avg_quality;
        float s_diff = analytics->speed_timeline[i] - analytics->avg_speed;
        
        quality_var_sum += q_diff * q_diff;
        speed_var_sum += s_diff * s_diff;
    }
    
    analytics->quality_variance = quality_var_sum / analytics->timeline_length;
    analytics->speed_variance = speed_var_sum / analytics->timeline_length;
    
    // Find peak gains
    analytics->peak_quality_gain = 0.0f;
    analytics->peak_speed_gain = 0.0f;
    analytics->peak_memory_saved = 0;
    
    for (uint32_t i = 1; i < analytics->timeline_length; i++) {
        float q_gain = analytics->quality_timeline[i] - analytics->quality_timeline[i-1];
        float s_gain = analytics->speed_timeline[i] - analytics->speed_timeline[i-1];
        int64_t m_saved = (int64_t)analytics->memory_timeline[i-1] - 
                         (int64_t)analytics->memory_timeline[i];
        
        if (q_gain > analytics->peak_quality_gain) {
            analytics->peak_quality_gain = q_gain;
        }
        if (s_gain > analytics->peak_speed_gain) {
            analytics->peak_speed_gain = s_gain;
        }
        if (m_saved > (int64_t)analytics->peak_memory_saved) {
            analytics->peak_memory_saved = (uint64_t)m_saved;
        }
    }
    
    // Calculate average gain per hotpatch
    if (controller->session->total_hotpatches > 0) {
        analytics->avg_gain_per_hotpatch = 
            (controller->session->total_quality_gain + 
             controller->session->total_speed_gain) / 
            (2.0f * controller->session->total_hotpatches);
    }
    
    // State visit analysis
    analytics->unique_states_visited = controller->omni->state_graph->state_count;
    analytics->state_revisits = analytics->timeline_length - analytics->unique_states_visited;
    
    // Time analysis
    if (analytics->timeline_length >= 2) {
        analytics->total_playback_time_ns = 
            controller->session->frames[analytics->timeline_length - 1].timestamp -
            controller->session->frames[0].timestamp;
    }
    
    if (analytics->total_playback_time_ns > 0) {
        analytics->hotpatches_per_second = 
            (float)controller->session->total_hotpatches / 
            (analytics->total_playback_time_ns / 1e9f);
        analytics->state_changes_per_second = 
            (float)analytics->timeline_length / 
            (analytics->total_playback_time_ns / 1e9f);
    }
    
    printf("[PLAYBACK] Analysis complete:\n");
    printf("  Avg Quality: %.4f (σ²=%.6f)\n", analytics->avg_quality, analytics->quality_variance);
    printf("  Avg Speed: %.4f (σ²=%.6f)\n", analytics->avg_speed, analytics->speed_variance);
    printf("  Avg Memory: %.2f MB\n", analytics->avg_memory_mb);
    printf("  Peak Quality Gain: %.4f\n", analytics->peak_quality_gain);
    printf("  Peak Speed Gain: %.4f\n", analytics->peak_speed_gain);
    printf("  Peak Memory Saved: %lu MB\n", (unsigned long)(analytics->peak_memory_saved / (1024 * 1024)));
}

void playback_generate_report(PlaybackController* controller, const char* output_path) {
    FILE* f = fopen(output_path, "w");
    if (!f) {
        printf("[PLAYBACK] Error: Cannot open %s for writing\n", output_path);
        return;
    }
    
    fprintf(f, "# Playback Session Report\n\n");
    
    // Session info
    fprintf(f, "## Session Information\n");
    fprintf(f, "- Session ID: %lu\n", (unsigned long)controller->session->session_id);
    fprintf(f, "- Name: %s\n", controller->session->session_name);
    fprintf(f, "- Frames: %u\n", controller->session->frame_count);
    fprintf(f, "- Duration: %.2f seconds\n", 
            controller->session->end_time > controller->session->start_time ?
            (controller->session->end_time - controller->session->start_time) / 1e9f : 0.0f);
    fprintf(f, "- Hotpatches: %u\n", controller->session->total_hotpatches);
    fprintf(f, "- Failures: %u\n\n", controller->session->failed_operations);
    
    // Best states
    fprintf(f, "## Best States\n");
    fprintf(f, "| Metric | State | Value |\n");
    fprintf(f, "|--------|-------|-------|\n");
    fprintf(f, "| Quality | #%lu | %.4f |\n", 
            (unsigned long)controller->session->best_quality_state,
            controller->session->best_quality_score);
    fprintf(f, "| Speed | #%lu | %.4f |\n",
            (unsigned long)controller->session->best_speed_state,
            controller->session->best_speed_score);
    fprintf(f, "| Memory | #%lu | %lu MB |\n\n",
            (unsigned long)controller->session->best_memory_state,
            (unsigned long)(controller->session->best_memory_bytes / (1024 * 1024)));
    
    // Gains
    fprintf(f, "## Cumulative Gains\n");
    fprintf(f, "- Quality: %+.4f\n", controller->session->total_quality_gain);
    fprintf(f, "- Speed: %+.4f\n", controller->session->total_speed_gain);
    fprintf(f, "- Memory: %+ld bytes\n\n", (long)controller->session->total_memory_saved);
    
    // Timeline
    if (controller->analytics && controller->analytics->timeline_length > 0) {
        fprintf(f, "## Timeline\n");
        fprintf(f, "```\n");
        fprintf(f, "Frame | Quality | Speed | Memory (MB) | Operation\n");
        fprintf(f, "------|---------|-------|-------------|----------\n");
        
        for (uint32_t i = 0; i < controller->analytics->timeline_length && i < 50; i++) {
            PlaybackFrame* frame = &controller->session->frames[i];
            fprintf(f, "%5u | %.4f | %.4f | %11lu | 0x%X\n",
                    i, frame->quality, frame->speed,
                    (unsigned long)(frame->memory_used / (1024 * 1024)), frame->operation);
        }
        
        if (controller->analytics->timeline_length > 50) {
            fprintf(f, "... (%u more frames)\n", controller->analytics->timeline_length - 50);
        }
        fprintf(f, "```\n\n");
    }
    
    // Analytics summary
    if (controller->analytics) {
        fprintf(f, "## Analytics Summary\n");
        fprintf(f, "- Average Quality: %.4f (variance: %.6f)\n",
                controller->analytics->avg_quality, controller->analytics->quality_variance);
        fprintf(f, "- Average Speed: %.4f (variance: %.6f)\n",
                controller->analytics->avg_speed, controller->analytics->speed_variance);
        fprintf(f, "- Average Memory: %.2f MB\n", controller->analytics->avg_memory_mb);
        fprintf(f, "- Peak Quality Gain: %.4f\n", controller->analytics->peak_quality_gain);
        fprintf(f, "- Peak Speed Gain: %.4f\n", controller->analytics->peak_speed_gain);
        fprintf(f, "- Peak Memory Saved: %lu MB\n",
                (unsigned long)(controller->analytics->peak_memory_saved / (1024 * 1024)));
        fprintf(f, "- States Visited: %u\n", controller->analytics->unique_states_visited);
        fprintf(f, "- Hotpatches/sec: %.2f\n", controller->analytics->hotpatches_per_second);
    }
    
    fclose(f);
    
    printf("[PLAYBACK] Report saved to: %s\n", output_path);
}

void playback_export_timeline(PlaybackController* controller, const char* output_path) {
    FILE* f = fopen(output_path, "w");
    if (!f) {
        printf("[PLAYBACK] Error: Cannot open %s for writing\n", output_path);
        return;
    }
    
    fprintf(f, "{\n");
    fprintf(f, "  \"session_id\": %lu,\n", (unsigned long)controller->session->session_id);
    fprintf(f, "  \"session_name\": \"%s\",\n", controller->session->session_name);
    fprintf(f, "  \"frame_count\": %u,\n", controller->session->frame_count);
    fprintf(f, "  \"frames\": [\n");
    
    for (uint32_t i = 0; i < controller->session->frame_count; i++) {
        PlaybackFrame* frame = &controller->session->frames[i];
        
        fprintf(f, "    {\n");
        fprintf(f, "      \"frame\": %u,\n", i);
        fprintf(f, "      \"timestamp\": %lu,\n", (unsigned long)frame->timestamp);
        fprintf(f, "      \"state_id\": %lu,\n", (unsigned long)frame->state_id);
        fprintf(f, "      \"quality\": %.6f,\n", frame->quality);
        fprintf(f, "      \"speed\": %.6f,\n", frame->speed);
        fprintf(f, "      \"memory_used\": %lu,\n", (unsigned long)frame->memory_used);
        fprintf(f, "      \"quality_gain\": %.6f,\n", frame->quality_gain);
        fprintf(f, "      \"speed_gain\": %.6f,\n", frame->speed_gain);
        fprintf(f, "      \"memory_saved\": %ld,\n", (long)frame->memory_saved);
        fprintf(f, "      \"operation\": %u,\n", frame->operation);
        fprintf(f, "      \"description\": \"%s\"\n", frame->description);
        fprintf(f, "    }%s\n", i < controller->session->frame_count - 1 ? "," : "");
    }
    
    fprintf(f, "  ],\n");
    fprintf(f, "  \"summary\": {\n");
    fprintf(f, "    \"total_quality_gain\": %.6f,\n", controller->session->total_quality_gain);
    fprintf(f, "    \"total_speed_gain\": %.6f,\n", controller->session->total_speed_gain);
    fprintf(f, "    \"total_memory_saved\": %ld,\n", (long)controller->session->total_memory_saved);
    fprintf(f, "    \"total_hotpatches\": %u,\n", controller->session->total_hotpatches);
    fprintf(f, "    \"failed_operations\": %u\n", controller->session->failed_operations);
    fprintf(f, "  }\n");
    fprintf(f, "}\n");
    
    fclose(f);
    
    printf("[PLAYBACK] Timeline exported to: %s\n", output_path);
}

void playback_visualize_gains(PlaybackController* controller, const char* output_path) {
    FILE* f = fopen(output_path, "w");
    if (!f) {
        printf("[PLAYBACK] Error: Cannot open %s for writing\n", output_path);
        return;
    }
    
    if (!controller->analytics || controller->analytics->timeline_length == 0) {
        fprintf(f, "No data to visualize\n");
        fclose(f);
        return;
    }
    
    fprintf(f, "# Quality/Speed/Memory Gains Over Time\n\n");
    
    // Quality chart
    fprintf(f, "## Quality Timeline\n");
    fprintf(f, "```\n");
    
    float q_min = 1.0f, q_max = 0.0f;
    for (uint32_t i = 0; i < controller->analytics->timeline_length; i++) {
        float q = controller->analytics->quality_timeline[i];
        if (q < q_min) q_min = q;
        if (q > q_max) q_max = q;
    }
    
    float q_range = q_max - q_min;
    if (q_range < 0.001f) q_range = 0.001f;
    
    for (uint32_t i = 0; i < controller->analytics->timeline_length && i < 60; i++) {
        float q = controller->analytics->quality_timeline[i];
        int bar_len = (int)((q - q_min) / q_range * 50);
        
        fprintf(f, "%3u |", i);
        for (int j = 0; j < bar_len; j++) fprintf(f, "█");
        fprintf(f, " %.4f\n", q);
    }
    
    fprintf(f, "```\n\n");
    
    // Speed chart
    fprintf(f, "## Speed Timeline\n");
    fprintf(f, "```\n");
    
    float s_min = 1.0f, s_max = 0.0f;
    for (uint32_t i = 0; i < controller->analytics->timeline_length; i++) {
        float s = controller->analytics->speed_timeline[i];
        if (s < s_min) s_min = s;
        if (s > s_max) s_max = s;
    }
    
    float s_range = s_max - s_min;
    if (s_range < 0.001f) s_range = 0.001f;
    
    for (uint32_t i = 0; i < controller->analytics->timeline_length && i < 60; i++) {
        float s = controller->analytics->speed_timeline[i];
        int bar_len = (int)((s - s_min) / s_range * 50);
        
        fprintf(f, "%3u |", i);
        for (int j = 0; j < bar_len; j++) fprintf(f, "█");
        fprintf(f, " %.4f\n", s);
    }
    
    fprintf(f, "```\n\n");
    
    // Memory chart
    fprintf(f, "## Memory Timeline (MB)\n");
    fprintf(f, "```\n");
    
    uint64_t m_min = UINT64_MAX, m_max = 0;
    for (uint32_t i = 0; i < controller->analytics->timeline_length; i++) {
        uint64_t m = controller->analytics->memory_timeline[i];
        if (m < m_min) m_min = m;
        if (m > m_max) m_max = m;
    }
    
    uint64_t m_range = m_max - m_min;
    if (m_range < 1024) m_range = 1024;
    
    for (uint32_t i = 0; i < controller->analytics->timeline_length && i < 60; i++) {
        uint64_t m = controller->analytics->memory_timeline[i];
        int bar_len = (int)((float)(m - m_min) / m_range * 50);
        
        fprintf(f, "%3u |", i);
        for (int j = 0; j < bar_len; j++) fprintf(f, "█");
        fprintf(f, " %lu MB\n", (unsigned long)(m / (1024 * 1024)));
    }
    
    fprintf(f, "```\n");
    
    fclose(f);
    
    printf("[PLAYBACK] Visualization saved to: %s\n", output_path);
}

// ============================================================================
// BENCHMARK HELPERS
// ============================================================================

float benchmark_quality(PlaybackController* controller, uint64_t state_id, uint32_t tokens) {
    HotpatchState* state = get_state(controller->omni, state_id);
    if (!state) return 0.0f;
    
    // Would run actual inference and measure quality
    // For now, return state's quality score with small variation
    float variation = ((float)rand() / RAND_MAX - 0.5f) * 0.02f;
    return state->quality_score + variation;
}

float benchmark_speed(PlaybackController* controller, uint64_t state_id, uint32_t tokens) {
    HotpatchState* state = get_state(controller->omni, state_id);
    if (!state) return 0.0f;
    
    // Would run actual inference and measure speed
    float variation = ((float)rand() / RAND_MAX - 0.5f) * 0.05f;
    return state->speed_score + variation;
}

uint64_t benchmark_memory(PlaybackController* controller, uint64_t state_id) {
    HotpatchState* state = get_state(controller->omni, state_id);
    if (!state) return 0;
    
    return state->memory_usage;
}

// ============================================================================
// PROGRESS TRACKING
// ============================================================================

float playback_get_progress(PlaybackController* controller) {
    if (controller->session->frame_count == 0) return 0.0f;
    return (float)controller->current_frame_index / controller->session->frame_count;
}

uint32_t playback_get_position(PlaybackController* controller) {
    return controller->current_frame_index;
}

uint32_t playback_get_total_frames(PlaybackController* controller) {
    return controller->session->frame_count;
}

bool playback_is_playing(PlaybackController* controller) {
    return controller->state == PLAYBACK_PLAYING ||
           controller->state == PLAYBACK_FAST_FORWARDING ||
           controller->state == PLAYBACK_REWINDING;
}

bool playback_is_recording(PlaybackController* controller) {
    return controller->is_recording;
}

// ============================================================================
// QUEUE MANAGEMENT
// ============================================================================

bool playback_queue_state(PlaybackController* controller, uint64_t state_id) {
    if (controller->queue_length >= controller->queue_capacity) {
        uint32_t new_capacity = controller->queue_capacity * 2;
        uint64_t* new_queue = (uint64_t*)realloc(controller->playback_queue,
                                      new_capacity * sizeof(uint64_t));
        if (!new_queue) return false;
        
        controller->playback_queue = new_queue;
        controller->queue_capacity = new_capacity;
    }
    
    controller->playback_queue[controller->queue_length++] = state_id;
    return true;
}

bool playback_queue_path(PlaybackController* controller, uint64_t* path, uint32_t length) {
    for (uint32_t i = 0; i < length; i++) {
        if (!playback_queue_state(controller, path[i])) {
            return false;
        }
    }
    return true;
}

bool playback_clear_queue(PlaybackController* controller) {
    controller->queue_length = 0;
    controller->queue_position = 0;
    return true;
}

bool playback_process_queue(PlaybackController* controller) {
    playback_play(controller);
    
    while (controller->queue_position < controller->queue_length) {
        if (!playback_step_forward(controller)) {
            break;
        }
    }
    
    playback_stop(controller);
    return true;
}

// ============================================================================
// EVENT HANDLERS
// ============================================================================

void playback_set_callbacks(PlaybackController* controller,
                           void (*on_state_change)(uint64_t, void*),
                           void (*on_frame_recorded)(PlaybackFrame*, void*),
                           void (*on_smoke_complete)(TestSession*, void*),
                           void* user_data) {
    controller->on_state_change = on_state_change;
    controller->on_frame_recorded = on_frame_recorded;
    controller->on_smoke_complete = on_smoke_complete;
    controller->user_data = user_data;
}

// ============================================================================
// STATE COMPARISON
// ============================================================================

void compare_states(PlaybackController* controller, uint64_t state1, uint64_t state2,
                    StateComparison* result) {
    memset(result, 0, sizeof(StateComparison));
    
    HotpatchState* s1 = get_state(controller->omni, state1);
    HotpatchState* s2 = get_state(controller->omni, state2);
    
    if (!s1 || !s2) {
        snprintf(result->recommendation, sizeof(result->recommendation), "Invalid state IDs");
        return;
    }
    
    result->state1_id = state1;
    result->state2_id = state2;
    
    result->quality_diff = s2->quality_score - s1->quality_score;
    result->speed_diff = s2->speed_score - s1->speed_score;
    result->memory_diff = (int64_t)s2->memory_usage - (int64_t)s1->memory_usage;
    
    if (s1->quality_score > 0) result->quality_ratio = s2->quality_score / s1->quality_score;
    if (s1->speed_score > 0) result->speed_ratio = s2->speed_score / s1->speed_score;
    if (s1->memory_usage > 0) result->memory_ratio = (float)s2->memory_usage / s1->memory_usage;
    
    // Calculate overall difference (weighted)
    result->overall_diff = fabsf(result->quality_diff) * 0.4f +
                          fabsf(result->speed_diff) * 0.3f +
                          fabsf((float)result->memory_diff / (1024.0f * 1024.0f)) * 0.0003f;
    
    // Determine winner
    float score1 = s1->quality_score * 0.4f + s1->speed_score * 0.3f +
                  (1.0f - s1->state_vector[AXIS_MEMORY]) * 0.3f;
    float score2 = s2->quality_score * 0.4f + s2->speed_score * 0.3f +
                  (1.0f - s2->state_vector[AXIS_MEMORY]) * 0.3f;
    
    if (score2 > score1) {
        snprintf(result->winner, sizeof(result->winner), "State #%lu wins (+%.4f)", 
                (unsigned long)state2, score2 - score1);
    } else if (score1 > score2) {
        snprintf(result->winner, sizeof(result->winner), "State #%lu wins (+%.4f)",
                (unsigned long)state1, score1 - score2);
    } else {
        snprintf(result->winner, sizeof(result->winner), "Tie");
    }
    
    // Generate recommendation
    if (result->quality_diff > 0.01f && result->speed_diff > 0.01f) {
        snprintf(result->recommendation, sizeof(result->recommendation),
                "State #%lu recommended: Better quality (+%.4f) and speed (+%.4f)",
                (unsigned long)state2, result->quality_diff, result->speed_diff);
    } else if (result->quality_diff > 0.01f) {
        snprintf(result->recommendation, sizeof(result->recommendation),
                "State #%lu recommended: Better quality (+%.4f)",
                (unsigned long)state2, result->quality_diff);
    } else if (result->speed_diff > 0.01f) {
        snprintf(result->recommendation, sizeof(result->recommendation),
                "State #%lu recommended: Better speed (+%.4f)",
                (unsigned long)state2, result->speed_diff);
    } else if (result->memory_diff < 0) {
        snprintf(result->recommendation, sizeof(result->recommendation),
                "State #%lu recommended: Lower memory usage (%ld MB saved)",
                (unsigned long)state2, (long)(-result->memory_diff / (1024 * 1024)));
    } else {
        snprintf(result->recommendation, sizeof(result->recommendation),
                "States are similar, prefer based on specific requirements");
    }
}

// ============================================================================
// DEMO / TEST
// ============================================================================

#ifdef HOTPATCH_PLAYBACK_DEMO

int main(void) {
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║     HOTPATCH PLAYBACK SYSTEM - SMOKE TEST DEMO             ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    // Create systems
    AutoConfig config;
    memset(&config, 0, sizeof(config));
    config.hw = detect_hardware();
    
    LiveHotpatch* base = live_hotpatch_create(&config, NULL);
    OmnidirectionalHotpatch* omni = omni_hotpatch_create(base);
    PlaybackController* playback = playback_create(omni);
    
    // Example 1: Record and playback session
    printf("\n=== Example 1: Recording Session ===\n");
    
    playback_record_start(playback, "Demo Session");
    
    // Simulate some hotpatch operations
    for (int i = 0; i < 5; i++) {
        HotpatchOp ops = 1 << (i % 8);
        apply_omni_hotpatch(omni, ops, NULL, 0);
        playback_record_frame(playback, ops);
    }
    
    playback_record_stop(playback);
    
    // Example 2: Play/Pause/Fast Forward
    printf("\n=== Example 2: Media Controls ===\n");
    
    playback_play(playback);                              // ▶ Play
    printf("Playing: %s\n", playback_is_playing(playback) ? "Yes" : "No");
    
    playback_pause(playback);                             // ⏸ Pause
    printf("Playing: %s\n", playback_is_playing(playback) ? "Yes" : "No");
    
    playback_fast_forward(playback, SPEED_4X);           // ⏩ Fast Forward 4x
    
    playback_rewind(playback);                            // ⏪ Rewind
    
    playback_jump_to_best(playback);                      // Jump to best quality state
    
    // Example 3: Smoke Test
    printf("\n=== Example 3: Smoke Test ===\n");
    
    SmokeTestConfig smoke = {
        .mode = SMOKE_QUICK,
        .max_iterations = 20,
        .max_time_seconds = 10,
        .test_operations = HOTPATCH_PRUNE_WEIGHTS | HOTPATCH_QUANTIZE | HOTPATCH_COMPRESS_KV,
        .exploration_rate = 0.5f,
        .record_session = true,
        .generate_report = true
    };
    
    SmokeTestResult result = smoke_test_run(playback, &smoke);
    printf("\n%s\n", result.summary);
    
    // Example 4: Compare states
    printf("\n=== Example 4: State Comparison ===\n");
    
    uint64_t states_to_compare[4];
    for (int i = 0; i < 4 && i < (int)omni->state_graph->state_count; i++) {
        states_to_compare[i] = omni->state_graph->states[i].state_id;
    }
    smoke_test_compare(playback, states_to_compare, 4);
    
    // Example 5: Eject and export
    printf("\n=== Example 5: Eject ===\n");
    
    playback_eject(playback, "smoke_test_output");
    
    // Generate additional reports
    playback_analyze(playback);
    playback_generate_report(playback, "detailed_report.md");
    
    // Cleanup
    playback_destroy(playback);
    omni_hotpatch_destroy(omni);
    live_hotpatch_destroy(base);
    
    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║               DEMO COMPLETE                               ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    return 0;
}

#endif // HOTPATCH_PLAYBACK_DEMO