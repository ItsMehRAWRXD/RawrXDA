// ============================================================================
// OMNIDIRECTIONAL HOTPATCH SYSTEM - Implementation
// State Graph, Vector Navigation, Multi-Path Hotpatching
// ============================================================================

#include "omnidirectional_hotpatch.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <float.h>
#include <time.h>

#define MAX_STATES 1024
#define MAX_EDGES 8192
#define MAX_VISITED 256
#define STATE_GROWTH 64
#define EDGE_GROWTH 256

// ============================================================================
// INITIALIZATION
// ============================================================================

OmnidirectionalHotpatch* omni_hotpatch_create(void) {
    OmnidirectionalHotpatch* ctx = (OmnidirectionalHotpatch*)calloc(1, sizeof(OmnidirectionalHotpatch));
    if (!ctx) return NULL;
    
    // Create state graph
    ctx->state_graph = (StateGraph*)calloc(1, sizeof(StateGraph));
    if (!ctx->state_graph) {
        free(ctx);
        return NULL;
    }
    
    // Allocate initial storage
    ctx->state_graph->states = (HotpatchState*)calloc(MAX_STATES, sizeof(HotpatchState));
    ctx->state_graph->edges = (HotpatchEdge*)calloc(MAX_EDGES, sizeof(HotpatchEdge));
    ctx->state_graph->state_index = (HotpatchState**)calloc(MAX_STATES * 2, sizeof(HotpatchState*));
    ctx->state_graph->anchor_state_ids = (uint64_t*)calloc(MAX_STATES, sizeof(uint64_t));
    
    ctx->state_graph->state_capacity = MAX_STATES;
    ctx->state_graph->edge_capacity = MAX_EDGES;
    
    // Allocate visited states history
    ctx->visited_states = (uint64_t*)calloc(MAX_VISITED, sizeof(uint64_t));
    ctx->visited_capacity = MAX_VISITED;
    
    // Create initial state (pristine model)
    uint64_t initial_state = create_state(ctx, 1.0f, 0.5f, 0, "Initial state (pristine)");
    if (initial_state == 0) {
        omni_hotpatch_destroy(ctx);
        return NULL;
    }
    
    ctx->current_state_id = initial_state;
    ctx->state_graph->current_state_id = initial_state;
    
    // Set defaults
    ctx->max_state_distance = 1.0f;
    ctx->max_path_length = 16;
    ctx->quality_weight = 0.4f;
    ctx->speed_weight = 0.3f;
    ctx->memory_weight = 0.3f;
    ctx->current_strategy = NAV_BALANCED;
    
    // Initialize state vector for current state
    memset(ctx->current_state_vector, 0, sizeof(ctx->current_state_vector));
    ctx->current_state_vector[AXIS_QUALITY] = 1.0f;
    ctx->current_state_vector[AXIS_STABILITY] = 1.0f;
    
    return ctx;
}

void omni_hotpatch_destroy(OmnidirectionalHotpatch* ctx) {
    if (!ctx) return;
    
    // Free state graph
    if (ctx->state_graph) {
        for (uint32_t i = 0; i < ctx->state_graph->state_count; i++) {
            if (ctx->state_graph->states[i].delta_data) {
                free(ctx->state_graph->states[i].delta_data);
            }
        }
        for (uint32_t i = 0; i < ctx->state_graph->edge_count; i++) {
            if (ctx->state_graph->edges[i].forward_delta) {
                free(ctx->state_graph->edges[i].forward_delta);
            }
            if (ctx->state_graph->edges[i].reverse_delta) {
                free(ctx->state_graph->edges[i].reverse_delta);
            }
        }
        
        free(ctx->state_graph->states);
        free(ctx->state_graph->edges);
        free(ctx->state_graph->state_index);
        free(ctx->state_graph->anchor_state_ids);
        free(ctx->state_graph);
    }
    
    free(ctx->visited_states);
    free(ctx);
}

// ============================================================================
// STATE MANAGEMENT
// ============================================================================

uint64_t create_state(OmnidirectionalHotpatch* ctx, float quality, float speed,
                      uint64_t memory, const char* description) {
    if (ctx->state_graph->state_count >= ctx->state_graph->state_capacity) {
        // Grow storage
        uint32_t new_capacity = ctx->state_graph->state_capacity + STATE_GROWTH;
        HotpatchState* new_states = (HotpatchState*)realloc(ctx->state_graph->states,
                                            new_capacity * sizeof(HotpatchState));
        if (!new_states) return 0;
        
        ctx->state_graph->states = new_states;
        ctx->state_graph->state_capacity = new_capacity;
    }
    
    HotpatchState* state = &ctx->state_graph->states[ctx->state_graph->state_count];
    memset(state, 0, sizeof(HotpatchState));
    
    state->state_id = ++ctx->state_graph->max_state_id;
    state->timestamp = (uint64_t)time(NULL);
    
    state->quality_score = quality;
    state->speed_score = speed;
    state->memory_usage = memory;
    
    // Calculate state vector
    state->state_vector[AXIS_QUALITY] = quality;
    state->state_vector[AXIS_SPEED] = speed;
    state->state_vector[AXIS_MEMORY] = 1.0f - (memory / (float)(1024ULL * 1024 * 1024 * 100)); // Normalized
    state->state_vector[AXIS_COMPUTE] = 0.5f;  // Default
    state->state_vector[AXIS_SPARSITY] = 0.0f;
    state->state_vector[AXIS_PRECISION] = 1.0f; // Full precision
    state->state_vector[AXIS_FEATURES] = 1.0f;  // All features
    state->state_vector[AXIS_STABILITY] = 1.0f;
    
    if (description) {
        strncpy(state->description, description, sizeof(state->description) - 1);
    }
    
    // Add to index
    if (state->state_id < MAX_STATES * 2) {
        ctx->state_graph->state_index[state->state_id] = state;
    }
    
    // Track as visited
    if (ctx->visited_count < ctx->visited_capacity) {
        ctx->visited_states[ctx->visited_count++] = state->state_id;
    }
    
    ctx->state_graph->state_count++;
    
    return state->state_id;
}

bool delete_state(OmnidirectionalHotpatch* ctx, uint64_t state_id) {
    // Find state
    int32_t idx = -1;
    for (uint32_t i = 0; i < ctx->state_graph->state_count; i++) {
        if (ctx->state_graph->states[i].state_id == state_id) {
            idx = (int32_t)i;
            break;
        }
    }
    
    if (idx < 0) return false;
    
    // Remove edges connected to this state
    uint32_t new_edge_count = 0;
    for (uint32_t i = 0; i < ctx->state_graph->edge_count; i++) {
        HotpatchEdge* edge = &ctx->state_graph->edges[i];
        if (edge->from_state_id != state_id && edge->to_state_id != state_id) {
            if (new_edge_count != i) {
                ctx->state_graph->edges[new_edge_count] = *edge;
            }
            new_edge_count++;
        }
    }
    ctx->state_graph->edge_count = new_edge_count;
    
    // Remove state
    if (ctx->state_graph->states[idx].delta_data) {
        free(ctx->state_graph->states[idx].delta_data);
    }
    
    // Shift states down
    for (uint32_t i = (uint32_t)idx; i < ctx->state_graph->state_count - 1; i++) {
        ctx->state_graph->states[i] = ctx->state_graph->states[i + 1];
    }
    
    ctx->state_graph->state_count--;
    ctx->state_graph->state_index[state_id] = NULL;
    
    return true;
}

HotpatchState* get_state(OmnidirectionalHotpatch* ctx, uint64_t state_id) {
    if (state_id < MAX_STATES * 2) {
        return ctx->state_graph->state_index[state_id];
    }
    
    // Linear search as fallback
    for (uint32_t i = 0; i < ctx->state_graph->state_count; i++) {
        if (ctx->state_graph->states[i].state_id == state_id) {
            return &ctx->state_graph->states[i];
        }
    }
    
    return NULL;
}

// ============================================================================
// EDGE MANAGEMENT
// ============================================================================

bool add_edge(OmnidirectionalHotpatch* ctx, uint64_t from_id, uint64_t to_id,
              HotpatchOp ops, bool bidirectional) {
    if (ctx->state_graph->edge_count >= ctx->state_graph->edge_capacity) {
        uint32_t new_capacity = ctx->state_graph->edge_capacity + EDGE_GROWTH;
        HotpatchEdge* new_edges = (HotpatchEdge*)realloc(ctx->state_graph->edges,
                                          new_capacity * sizeof(HotpatchEdge));
        if (!new_edges) return false;
        
        ctx->state_graph->edges = new_edges;
        ctx->state_graph->edge_capacity = new_capacity;
    }
    
    HotpatchState* from_state = get_state(ctx, from_id);
    HotpatchState* to_state = get_state(ctx, to_id);
    
    if (!from_state || !to_state) return false;
    
    // Add forward edge
    HotpatchEdge* edge = &ctx->state_graph->edges[ctx->state_graph->edge_count];
    memset(edge, 0, sizeof(HotpatchEdge));
    
    edge->from_state_id = from_id;
    edge->to_state_id = to_id;
    edge->operations = ops;
    edge->bidirectional = bidirectional;
    edge->reversible = true;
    
    // Calculate deltas
    edge->quality_delta = to_state->quality_score - from_state->quality_score;
    edge->speed_delta = to_state->speed_score - from_state->speed_score;
    edge->memory_delta = (int64_t)to_state->memory_usage - (int64_t)from_state->memory_usage;
    
    // Calculate direction vector
    calculate_direction_vector(ctx, from_id, to_id, edge->direction_vector);
    
    // Estimate transition cost
    edge->transition_cost = fabsf(edge->quality_delta) * 0.4f +
                           fabsf(edge->memory_delta) / (1024.0f * 1024.0f) * 0.0001f;
    
    // Update parent/child relationships
    if (from_state->child_count < 16) {
        from_state->child_states[from_state->child_count++] = to_id;
    }
    if (to_state->parent_count < 4) {
        to_state->parent_states[to_state->parent_count++] = from_id;
    }
    
    ctx->state_graph->edge_count++;
    
    // Add reverse edge if bidirectional
    if (bidirectional && from_id != to_id) {
        add_edge(ctx, to_id, from_id, ops, false); // Don't recurse
    }
    
    return true;
}

bool remove_edge(OmnidirectionalHotpatch* ctx, uint64_t from_id, uint64_t to_id) {
    uint32_t new_count = 0;
    
    for (uint32_t i = 0; i < ctx->state_graph->edge_count; i++) {
        HotpatchEdge* edge = &ctx->state_graph->edges[i];
        
        if (!(edge->from_state_id == from_id && edge->to_state_id == to_id)) {
            if (new_count != i) {
                ctx->state_graph->edges[new_count] = *edge;
            }
            new_count++;
        }
    }
    
    ctx->state_graph->edge_count = new_count;
    return true;
}

HotpatchEdge* find_edge(OmnidirectionalHotpatch* ctx, uint64_t from_id, uint64_t to_id) {
    for (uint32_t i = 0; i < ctx->state_graph->edge_count; i++) {
        HotpatchEdge* edge = &ctx->state_graph->edges[i];
        if (edge->from_state_id == from_id && edge->to_state_id == to_id) {
            return edge;
        }
    }
    return NULL;
}

// ============================================================================
// VECTOR NAVIGATION
// ============================================================================

NavigationResult navigate_to_state(OmnidirectionalHotpatch* ctx, uint64_t target_state_id) {
    NavigationResult result;
    memset(&result, 0, sizeof(result));
    
    HotpatchState* current = get_state(ctx, ctx->current_state_id);
    HotpatchState* target = get_state(ctx, target_state_id);
    
    if (!current || !target) {
        result.success = false;
        snprintf(result.failure_reason, sizeof(result.failure_reason), "Invalid state ID");
        return result;
    }
    
    // Find path to target
    StatePath* path = find_path(ctx, ctx->current_state_id, target_state_id);
    if (!path || path->state_count == 0) {
        result.success = false;
        snprintf(result.failure_reason, sizeof(result.failure_reason), "No path found");
        if (path) free_path(path);
        return result;
    }
    
    // Navigate through path
    result.from_state_id = ctx->current_state_id;
    
    for (uint32_t i = 0; i < path->state_count; i++) {
        uint64_t next_state_id = path->state_ids[i];
        HotpatchEdge* edge = find_edge(ctx, ctx->current_state_id, next_state_id);
        
        if (edge) {
            ctx->current_state_id = next_state_id;
            ctx->total_navigations++;
            
            result.path_state_ids[i] = next_state_id;
            
            result.quality_change += edge->quality_delta;
            result.speed_change += edge->speed_delta;
            result.memory_change += edge->memory_delta;
        }
    }
    
    result.to_state_id = ctx->current_state_id;
    result.path_length = path->state_count;
    result.success = true;
    
    // Calculate actual direction
    calculate_direction_vector(ctx, result.from_state_id, result.to_state_id, 
                               result.actual_direction);
    result.distance_traveled = calculate_state_distance(ctx, result.from_state_id, 
                                                        result.to_state_id);
    
    free_path(path);
    
    return result;
}

NavigationResult navigate_to_vector(OmnidirectionalHotpatch* ctx, float target_vector[8]) {
    // Find nearest state to target vector
    uint64_t nearest_id = find_nearest_state(ctx, target_vector);
    
    if (nearest_id == 0) {
        NavigationResult result;
        memset(&result, 0, sizeof(result));
        result.success = false;
        snprintf(result.failure_reason, sizeof(result.failure_reason), 
                 "No state found near target vector");
        return result;
    }
    
    return navigate_to_state(ctx, nearest_id);
}

NavigationResult navigate_to_properties(OmnidirectionalHotpatch* ctx,
                                         float target_quality, float target_speed,
                                         uint64_t target_memory) {
    float target_vector[8] = {0};
    target_vector[AXIS_QUALITY] = target_quality;
    target_vector[AXIS_SPEED] = target_speed;
    target_vector[AXIS_MEMORY] = 1.0f - (target_memory / (float)(1024ULL * 1024 * 1024 * 100));
    target_vector[AXIS_STABILITY] = 0.5f; // Default
    
    return navigate_to_vector(ctx, target_vector);
}

NavigationResult navigate_direction(OmnidirectionalHotpatch* ctx,
                                     float direction[8], float magnitude) {
    NavigationResult result;
    memset(&result, 0, sizeof(result));
    
    // Calculate target state vector
    float target_vector[8];
    HotpatchState* current = get_state(ctx, ctx->current_state_id);
    
    if (!current) {
        result.success = false;
        snprintf(result.failure_reason, sizeof(result.failure_reason), 
                 "Current state not found");
        return result;
    }
    
    // Normalize direction
    float dir_norm = 0.0f;
    for (int i = 0; i < 8; i++) {
        dir_norm += direction[i] * direction[i];
    }
    dir_norm = sqrtf(dir_norm);
    
    if (dir_norm > 0.001f) {
        for (int i = 0; i < 8; i++) {
            direction[i] /= dir_norm;
        }
    }
    
    // Calculate target
    for (int i = 0; i < 8; i++) {
        target_vector[i] = current->state_vector[i] + direction[i] * magnitude;
        // Clamp to valid range
        if (target_vector[i] < 0.0f) target_vector[i] = 0.0f;
        if (target_vector[i] > 1.0f) target_vector[i] = 1.0f;
    }
    
    return navigate_to_vector(ctx, target_vector);
}

NavigationResult navigate_random(OmnidirectionalHotpatch* ctx, float exploration_factor) {
    float random_direction[8];
    
    // Generate random direction
    for (int i = 0; i < 8; i++) {
        random_direction[i] = ((float)rand() / RAND_MAX * 2.0f - 1.0f) * exploration_factor;
    }
    
    return navigate_direction(ctx, random_direction, exploration_factor);
}

// ============================================================================
// MULTI-DIRECTIONAL NAVIGATION
// ============================================================================

NavigationResult navigate_forward(OmnidirectionalHotpatch* ctx) {
    // Find states with better properties
    HotpatchState* current = get_state(ctx, ctx->current_state_id);
    if (!current) {
        NavigationResult result = {0};
        result.success = false;
        return result;
    }
    
    // Calculate "forward" direction (improving quality/speed, reducing memory)
    float forward_direction[8] = {0};
    forward_direction[AXIS_QUALITY] = 1.0f;  // Improve quality
    forward_direction[AXIS_SPEED] = 1.0f;    // Improve speed
    forward_direction[AXIS_MEMORY] = -1.0f;  // Reduce memory
    
    return navigate_direction(ctx, forward_direction, 0.5f);
}

NavigationResult navigate_backward(OmnidirectionalHotpatch* ctx) {
    // Navigate to previous state
    if (ctx->visited_count < 2) {
        NavigationResult result = {0};
        result.success = false;
        snprintf(result.failure_reason, sizeof(result.failure_reason),
                 "No previous state to navigate to");
        return result;
    }
    
    uint64_t prev_state_id = ctx->visited_states[ctx->visited_count - 2];
    return navigate_to_state(ctx, prev_state_id);
}

NavigationResult navigate_to_best(OmnidirectionalHotpatch* ctx) {
    // Find best state based on current weights
    uint64_t best_id = find_best_state(ctx, ctx->quality_weight, 
                                       ctx->speed_weight, ctx->memory_weight);
    
    if (best_id == 0) {
        NavigationResult result = {0};
        result.success = false;
        return result;
    }
    
    return navigate_to_state(ctx, best_id);
}

NavigationResult navigate_to_nearest_anchor(OmnidirectionalHotpatch* ctx) {
    HotpatchState* current = get_state(ctx, ctx->current_state_id);
    if (!current) {
        NavigationResult result = {0};
        result.success = false;
        return result;
    }
    
    // Find nearest anchor state
    uint64_t nearest_anchor = 0;
    float min_distance = FLT_MAX;
    
    for (uint32_t i = 0; i < ctx->state_graph->anchor_count; i++) {
        uint64_t anchor_id = ctx->state_graph->anchor_state_ids[i];
        float dist = calculate_state_distance(ctx, ctx->current_state_id, anchor_id);
        
        if (dist < min_distance) {
            min_distance = dist;
            nearest_anchor = anchor_id;
        }
    }
    
    if (nearest_anchor == 0) {
        NavigationResult result = {0};
        result.success = false;
        snprintf(result.failure_reason, sizeof(result.failure_reason),
                 "No anchor states found");
        return result;
    }
    
    return navigate_to_state(ctx, nearest_anchor);
}

NavigationResult navigate_away_from_worst(OmnidirectionalHotpatch* ctx) {
    // Find worst state
    uint64_t worst_id = 0;
    float worst_score = FLT_MAX;
    
    for (uint32_t i = 0; i < ctx->state_graph->state_count; i++) {
        HotpatchState* state = &ctx->state_graph->states[i];
        
        float score = state->quality_score * ctx->quality_weight +
                      state->speed_score * ctx->speed_weight -
                      (state->memory_usage / (float)(1024 * 1024 * 1024)) * ctx->memory_weight;
        
        if (score < worst_score) {
            worst_score = score;
            worst_id = state->state_id;
        }
    }
    
    if (worst_id == 0 || worst_id == ctx->current_state_id) {
        NavigationResult result = {0};
        result.success = true;
        return result;
    }
    
    // Navigate away from worst
    HotpatchState* current = get_state(ctx, ctx->current_state_id);
    HotpatchState* worst = get_state(ctx, worst_id);
    
    float away_direction[8];
    for (int i = 0; i < 8; i++) {
        away_direction[i] = current->state_vector[i] - worst->state_vector[i];
    }
    
    return navigate_direction(ctx, away_direction, 0.5f);
}

// ============================================================================
// PATH FINDING
// ============================================================================

StatePath* find_path(OmnidirectionalHotpatch* ctx, uint64_t from_id, uint64_t to_id) {
    // Simple BFS for now (would implement A* or Dijkstra for production)
    
    uint64_t* queue = (uint64_t*)calloc(ctx->state_graph->state_count, sizeof(uint64_t));
    uint64_t* parent = (uint64_t*)calloc(ctx->state_graph->max_state_id + 1, sizeof(uint64_t));
    bool* visited = (bool*)calloc(ctx->state_graph->max_state_id + 1, sizeof(bool));
    
    if (!queue || !parent || !visited) {
        free(queue);
        free(parent);
        free(visited);
        return NULL;
    }
    
    uint32_t queue_head = 0, queue_tail = 0;
    
    // Start BFS
    queue[queue_tail++] = from_id;
    visited[from_id] = true;
    parent[from_id] = 0; // No parent for start
    
    bool found = false;
    
    while (queue_head < queue_tail && !found) {
        uint64_t current_id = queue[queue_head++];
        
        // Check if we reached target
        if (current_id == to_id) {
            found = true;
            break;
        }
        
        // Explore neighbors
        for (uint32_t i = 0; i < ctx->state_graph->edge_count; i++) {
            HotpatchEdge* edge = &ctx->state_graph->edges[i];
            
            if (edge->from_state_id == current_id) {
                uint64_t neighbor_id = edge->to_state_id;
                
                if (!visited[neighbor_id]) {
                    visited[neighbor_id] = true;
                    parent[neighbor_id] = current_id;
                    queue[queue_tail++] = neighbor_id;
                }
            } else if (edge->bidirectional && edge->to_state_id == current_id) {
                uint64_t neighbor_id = edge->from_state_id;
                
                if (!visited[neighbor_id]) {
                    visited[neighbor_id] = true;
                    parent[neighbor_id] = current_id;
                    queue[queue_tail++] = neighbor_id;
                }
            }
        }
    }
    
    free(queue);
    
    StatePath* path = NULL;
    
    if (found) {
        // Reconstruct path
        uint64_t* reverse_path = (uint64_t*)calloc(ctx->state_graph->state_count, sizeof(uint64_t));
        uint32_t path_length = 0;
        
        uint64_t current = to_id;
        while (current != 0 && current != from_id) {
            reverse_path[path_length++] = current;
            current = parent[current];
        }
        
        // Allocate path
        path = (StatePath*)calloc(1, sizeof(StatePath));
        path->state_ids = (uint64_t*)calloc(path_length, sizeof(uint64_t));
        path->state_count = path_length;
        path->path_type = PATH_HEURISTIC;
        
        // Reverse path
        for (uint32_t i = 0; i < path_length; i++) {
            path->state_ids[i] = reverse_path[path_length - 1 - i];
        }
        
        free(reverse_path);
    }
    
    free(parent);
    free(visited);
    
    return path;
}

StatePath* find_optimal_path(OmnidirectionalHotpatch* ctx, NavigationVector* nav) {
    // A* search toward target vector
    // For now, use simple heuristic
    
    // Find states near target
    uint64_t best_state = find_nearest_state(ctx, nav->vector);
    
    if (best_state == 0) {
        return NULL;
    }
    
    return find_path(ctx, ctx->current_state_id, best_state);
}

StatePath* find_shortest_path(OmnidirectionalHotpatch* ctx, uint64_t from_id, uint64_t to_id) {
    // BFS finds shortest path by number of edges
    return find_path(ctx, from_id, to_id);
}

StatePath* find_cheapest_path(OmnidirectionalHotpatch* ctx, uint64_t from_id, uint64_t to_id) {
    // Dijkstra's algorithm for minimum cost path
    
    float* distance = (float*)calloc(ctx->state_graph->max_state_id + 1, sizeof(float));
    uint64_t* parent = (uint64_t*)calloc(ctx->state_graph->max_state_id + 1, sizeof(uint64_t));
    bool* visited = (bool*)calloc(ctx->state_graph->max_state_id + 1, sizeof(bool));
    
    if (!distance || !parent || !visited) {
        free(distance);
        free(parent);
        free(visited);
        return NULL;
    }
    
    // Initialize distances
    for (uint64_t i = 0; i <= ctx->state_graph->max_state_id; i++) {
        distance[i] = FLT_MAX;
    }
    distance[from_id] = 0.0f;
    
    // Simple implementation (would use priority queue in production)
    for (uint32_t iter = 0; iter < ctx->state_graph->state_count; iter++) {
        // Find minimum distance unvisited node
        float min_dist = FLT_MAX;
        uint64_t min_node = 0;
        
        for (uint32_t i = 0; i < ctx->state_graph->state_count; i++) {
            uint64_t node = ctx->state_graph->states[i].state_id;
            if (!visited[node] && distance[node] < min_dist) {
                min_dist = distance[node];
                min_node = node;
            }
        }
        
        if (min_node == 0) break;
        
        visited[min_node] = true;
        
        // Relax edges
        for (uint32_t i = 0; i < ctx->state_graph->edge_count; i++) {
            HotpatchEdge* edge = &ctx->state_graph->edges[i];
            
            if (edge->from_state_id == min_node) {
                uint64_t neighbor = edge->to_state_id;
                float new_dist = distance[min_node] + edge->transition_cost;
                
                if (new_dist < distance[neighbor]) {
                    distance[neighbor] = new_dist;
                    parent[neighbor] = min_node;
                }
            }
        }
    }
    
    // Reconstruct path
    StatePath* path = NULL;
    
    if (distance[to_id] < FLT_MAX) {
        uint64_t* reverse_path = (uint64_t*)calloc(ctx->state_graph->state_count, sizeof(uint64_t));
        uint32_t path_length = 0;
        
        uint64_t current = to_id;
        while (current != 0 && current != from_id) {
            reverse_path[path_length++] = current;
            current = parent[current];
        }
        
        path = (StatePath*)calloc(1, sizeof(StatePath));
        path->state_ids = (uint64_t*)calloc(path_length, sizeof(uint64_t));
        path->state_count = path_length;
        path->path_type = PATH_OPTIMAL;
        path->total_cost = distance[to_id];
        
        for (uint32_t i = 0; i < path_length; i++) {
            path->state_ids[i] = reverse_path[path_length - 1 - i];
        }
        
        free(reverse_path);
    }
    
    free(distance);
    free(parent);
    free(visited);
    
    return path;
}

void free_path(StatePath* path) {
    if (!path) return;
    
    free(path->state_ids);
    free(path);
}

// ============================================================================
// MERGE AND BLEND OPERATIONS
// ============================================================================

uint64_t merge_states(OmnidirectionalHotpatch* ctx, MergeConfig* config) {
    if (config->source_count < 2) return 0;
    
    // Calculate merged properties
    float merged_quality = 0.0f;
    float merged_speed = 0.0f;
    uint64_t merged_memory = 0;
    float merged_vector[8] = {0};
    
    float weight_sum = 0.0f;
    for (uint32_t i = 0; i < config->source_count; i++) {
        weight_sum += config->weights[i];
    }
    
    for (uint32_t i = 0; i < config->source_count; i++) {
        HotpatchState* state = get_state(ctx, config->source_state_ids[i]);
        if (!state) continue;
        
        float w = config->weights[i] / weight_sum;
        
        merged_quality += state->quality_score * w;
        merged_speed += state->speed_score * w;
        merged_memory += (uint64_t)(state->memory_usage * w);
        
        for (int j = 0; j < 8; j++) {
            merged_vector[j] += state->state_vector[j] * w;
        }
    }
    
    // Create merged state
    char description[256];
    snprintf(description, sizeof(description), "Merged state from %u sources",
             config->source_count);
    
    uint64_t merged_id = create_state(ctx, merged_quality, merged_speed, 
                                      merged_memory, description);
    
    if (merged_id == 0) return 0;
    
    // Add edges from all source states
    for (uint32_t i = 0; i < config->source_count; i++) {
        add_edge(ctx, config->source_state_ids[i], merged_id, HOTPATCH_ALL, true);
    }
    
    ctx->total_merges++;
    
    return merged_id;
}

uint64_t blend_states(OmnidirectionalHotpatch* ctx, uint64_t state1_id, uint64_t state2_id,
                      float blend_factor) {
    HotpatchState* state1 = get_state(ctx, state1_id);
    HotpatchState* state2 = get_state(ctx, state2_id);
    
    if (!state1 || !state2) return 0;
    
    // Linear blend
    float blended_quality = state1->quality_score * (1.0f - blend_factor) +
                           state2->quality_score * blend_factor;
    
    float blended_speed = state1->speed_score * (1.0f - blend_factor) +
                         state2->speed_score * blend_factor;
    
    uint64_t blended_memory = (uint64_t)(state1->memory_usage * (1.0f - blend_factor) +
                                        state2->memory_usage * blend_factor);
    
    // Create blended state
    char description[256];
    snprintf(description, sizeof(description), 
             "Blend of #%lu and #%lu (factor %.2f)", state1_id, state2_id, blend_factor);
    
    uint64_t blended_id = create_state(ctx, blended_quality, blended_speed,
                                        blended_memory, description);
    
    if (blended_id == 0) return 0;
    
    // Add edges
    add_edge(ctx, state1_id, blended_id, HOTPATCH_ALL, true);
    add_edge(ctx, state2_id, blended_id, HOTPATCH_ALL, true);
    
    return blended_id;
}

uint64_t interpolate_states(OmnidirectionalHotpatch* ctx, InterpolationConfig* config) {
    HotpatchState* from = get_state(ctx, config->from_state_id);
    HotpatchState* to = get_state(ctx, config->to_state_id);
    
    if (!from || !to) return 0;
    
    float t = config->t;
    
    // Apply interpolation mode
    switch (config->mode) {
        case INTERP_SMOOTH:
            t = t * t * (3.0f - 2.0f * t);
            break;
        case INTERP_EASE_IN:
            t = t * t;
            break;
        case INTERP_EASE_OUT:
            t = 1.0f - (1.0f - t) * (1.0f - t);
            break;
        case INTERP_EASE_IN_OUT:
            t = t < 0.5f ? 2.0f * t * t : 1.0f - powf(-2.0f * t + 2.0f, 2.0f) / 2.0f;
            break;
        case INTERP_SPRING:
            t = 1.0f - cosf(t * 3.14159f / 2.0f);
            break;
        case INTERP_EXPONENTIAL:
            t = t == 0.0f ? 0.0f : powf(2.0f, 10.0f * (t - 1.0f));
            break;
        default:
            break;
    }
    
    // Interpolate properties
    float interp_quality = from->quality_score * (1.0f - t) + to->quality_score * t;
    float interp_speed = from->speed_score * (1.0f - t) + to->speed_score * t;
    uint64_t interp_memory = (uint64_t)(from->memory_usage * (1.0f - t) + to->memory_usage * t);
    
    char description[256];
    snprintf(description, sizeof(description),
             "Interpolated state t=%.2f from #%lu to #%lu", t, 
             config->from_state_id, config->to_state_id);
    
    uint64_t interp_id = create_state(ctx, interp_quality, interp_speed,
                                       interp_memory, description);
    
    if (interp_id == 0) return 0;
    
    // Add edges
    add_edge(ctx, config->from_state_id, interp_id, HOTPATCH_ALL, true);
    add_edge(ctx, interp_id, config->to_state_id, HOTPATCH_ALL, true);
    
    config->interpolated_state_id = interp_id;
    for (int i = 0; i < 8; i++) {
        config->interpolated_vector[i] = from->state_vector[i] * (1.0f - t) + 
                                        to->state_vector[i] * t;
    }
    
    ctx->total_interpolations++;
    
    return interp_id;
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

float calculate_state_distance(OmnidirectionalHotpatch* ctx, uint64_t state1_id, 
                                uint64_t state2_id) {
    HotpatchState* state1 = get_state(ctx, state1_id);
    HotpatchState* state2 = get_state(ctx, state2_id);
    
    if (!state1 || !state2) return FLT_MAX;
    
    // Euclidean distance in state space
    float distance = 0.0f;
    for (int i = 0; i < 8; i++) {
        float diff = state1->state_vector[i] - state2->state_vector[i];
        distance += diff * diff;
    }
    
    return sqrtf(distance);
}

void calculate_direction_vector(OmnidirectionalHotpatch* ctx, uint64_t from_id,
                                uint64_t to_id, float direction[8]) {
    HotpatchState* from = get_state(ctx, from_id);
    HotpatchState* to = get_state(ctx, to_id);
    
    if (!from || !to) {
        memset(direction, 0, 8 * sizeof(float));
        return;
    }
    
    // Calculate direction
    for (int i = 0; i < 8; i++) {
        direction[i] = to->state_vector[i] - from->state_vector[i];
    }
    
    // Normalize
    float length = 0.0f;
    for (int i = 0; i < 8; i++) {
        length += direction[i] * direction[i];
    }
    length = sqrtf(length);
    
    if (length > 0.001f) {
        for (int i = 0; i < 8; i++) {
            direction[i] /= length;
        }
    }
}

uint64_t find_nearest_state(OmnidirectionalHotpatch* ctx, float target_vector[8]) {
    uint64_t nearest_id = 0;
    float min_distance = FLT_MAX;
    
    for (uint32_t i = 0; i < ctx->state_graph->state_count; i++) {
        HotpatchState* state = &ctx->state_graph->states[i];
        
        float distance = 0.0f;
        for (int j = 0; j < 8; j++) {
            float diff = state->state_vector[j] - target_vector[j];
            distance += diff * diff;
        }
        distance = sqrtf(distance);
        
        if (distance < min_distance) {
            min_distance = distance;
            nearest_id = state->state_id;
        }
    }
    
    return nearest_id;
}

uint64_t find_best_state(OmnidirectionalHotpatch* ctx, float quality_weight,
                         float speed_weight, float memory_weight) {
    uint64_t best_id = 0;
    float best_score = -FLT_MAX;
    
    for (uint32_t i = 0; i < ctx->state_graph->state_count; i++) {
        HotpatchState* state = &ctx->state_graph->states[i];
        
        float score = state->quality_score * quality_weight +
                      state->speed_score * speed_weight +
                      (1.0f - state->state_vector[AXIS_MEMORY]) * memory_weight;
        
        if (score > best_score) {
            best_score = score;
            best_id = state->state_id;
        }
    }
    
    return best_id;
}

bool can_transition(OmnidirectionalHotpatch* ctx, uint64_t from_id, uint64_t to_id) {
    // Check if edge exists
    if (find_edge(ctx, from_id, to_id)) return true;
    
    // Check if path exists
    StatePath* path = find_path(ctx, from_id, to_id);
    bool exists = (path != NULL);
    free_path(path);
    
    return exists;
}

// ============================================================================
// OMNIDIRECTIONAL HOTPATCH OPERATIONS
// ============================================================================

bool apply_omni_hotpatch(OmnidirectionalHotpatch* ctx, HotpatchOp ops,
                         NavigationVector* nav) {
    // Create checkpoint state
    uint64_t from_state = ctx->current_state_id;
    
    // Estimate resulting state vector
    HotpatchState* current = get_state(ctx, from_state);
    if (!current) return false;
    
    float new_vector[8];
    memcpy(new_vector, current->state_vector, sizeof(new_vector));
    
    // Estimate impact of operations
    if (ops & HOTPATCH_PRUNE_WEIGHTS) {
        new_vector[AXIS_SPARSITY] += 0.1f;
        new_vector[AXIS_MEMORY] += 0.05f;
        new_vector[AXIS_QUALITY] -= 0.02f;
        new_vector[AXIS_SPEED] += 0.05f;
    }
    
    if (ops & HOTPATCH_QUANTIZE) {
        new_vector[AXIS_PRECISION] -= 0.5f;
        new_vector[AXIS_MEMORY] += 0.2f;
        new_vector[AXIS_QUALITY] -= 0.01f;
    }
    
    if (ops & HOTPACK_KV) {
        new_vector[AXIS_MEMORY] += 0.1f;
    }
    
    if (ops & HOTPATCH_PRUNE_HEADS) {
        new_vector[AXIS_FEATURES] -= 0.1f;
        new_vector[AXIS_MEMORY] += 0.05f;
        new_vector[AXIS_QUALITY] -= 0.015f;
    }
    
    // Clamp values
    for (int i = 0; i < 8; i++) {
        if (new_vector[i] < 0.0f) new_vector[i] = 0.0f;
        if (new_vector[i] > 1.0f) new_vector[i] = 1.0f;
    }
    
    // Create new state
    char description[256];
    snprintf(description, sizeof(description), "Hotpatch ops: 0x%X", ops);
    
    uint64_t to_state = create_state(ctx, new_vector[AXIS_QUALITY], 
                                      new_vector[AXIS_SPEED],
                                      (uint64_t)((1.0f - new_vector[AXIS_MEMORY]) * 
                                                 100ULL * 1024 * 1024 * 1024),
                                      description);
    
    if (to_state == 0) return false;
    
    // Add edge
    add_edge(ctx, from_state, to_state, ops, true);
    
    // Navigate to new state
    NavigationResult result = navigate_to_state(ctx, to_state);
    
    return result.success;
}

bool unhotpatch(OmnidirectionalHotpatch* ctx) {
    // Navigate backward through state graph
    NavigationResult result = navigate_backward(ctx);
    return result.success;
}

bool rehotpatch(OmnidirectionalHotpatch* ctx, uint64_t target_state_id) {
    // Navigate to specific state
    NavigationResult result = navigate_to_state(ctx, target_state_id);
    return result.success;
}

bool cross_hotpatch(OmnidirectionalHotpatch* ctx, uint64_t state1_id,
                    uint64_t state2_id, float crossover_point) {
    // Create crossover between two states at specified point
    InterpolationConfig interp = {0};
    interp.from_state_id = state1_id;
    interp.to_state_id = state2_id;
    interp.t = crossover_point;
    interp.mode = INTERP_SMOOTH;
    
    uint64_t crossed_id = interpolate_states(ctx, &interp);
    
    if (crossed_id == 0) return false;
    
    return rehotpatch(ctx, crossed_id);
}

bool parallel_hotpatch(OmnidirectionalHotpatch* ctx, HotpatchOp ops1,
                       HotpatchOp ops2, uint32_t num_paths) {
    // Explore multiple hotpatch paths in parallel
    // Create states for each path and find best
    
    uint64_t* path_results = (uint64_t*)calloc(num_paths, sizeof(uint64_t));
    
    for (uint32_t i = 0; i < num_paths; i++) {
        // Apply with slightly different parameters
        float variation = ((float)rand() / RAND_MAX) * 0.2f - 0.1f; // +/- 10%
        
        NavigationVector nav = {0};
        nav.vector[AXIS_QUALITY] = 1.0f + variation;
        nav.vector[AXIS_SPEED] = 1.0f + variation;
        
        HotpatchOp ops = (i % 2 == 0) ? ops1 : ops2;
        
        if (apply_omni_hotpatch(ctx, ops, &nav)) {
            path_results[i] = ctx->current_state_id;
        }
    }
    
    // Find best result
    uint64_t best_id = find_best_state(ctx, ctx->quality_weight,
                                       ctx->speed_weight, ctx->memory_weight);
    
    if (best_id != 0) {
        rehotpatch(ctx, best_id);
    }
    
    free(path_results);
    return true;
}

// ============================================================================
// STATE SPACE EXPLORATION
// ============================================================================

void explore_state_space(OmnidirectionalHotpatch* ctx, uint32_t iterations) {
    for (uint32_t i = 0; i < iterations; i++) {
        // Random exploration
        NavigationResult result = navigate_random(ctx, 0.3f);
        
        if (!result.success) {
            // Try different direction
            float alt_direction[8] = {0};
            alt_direction[rand() % 8] = (rand() % 2 == 0) ? 1.0f : -1.0f;
            navigate_direction(ctx, alt_direction, 0.5f);
        }
        
        // Occasionally try to reach better states
        if (i % 10 == 0) {
            navigate_to_best(ctx);
        }
    }
}

uint64_t* find_pareto_frontier(OmnidirectionalHotpatch* ctx, uint32_t* count) {
    // Find Pareto-optimal states (not dominated by any other state)
    
    bool* is_pareto = (bool*)calloc(ctx->state_graph->state_count, sizeof(bool));
    
    for (uint32_t i = 0; i < ctx->state_graph->state_count; i++) {
        is_pareto[i] = true;
    }
    
    // Compare each pair
    for (uint32_t i = 0; i < ctx->state_graph->state_count; i++) {
        if (!is_pareto[i]) continue;
        
        HotpatchState* state_i = &ctx->state_graph->states[i];
        
        for (uint32_t j = 0; j < ctx->state_graph->state_count; j++) {
            if (i == j || !is_pareto[j]) continue;
            
            HotpatchState* state_j = &ctx->state_graph->states[j];
            
            // Check if j dominates i
            bool dominates = true;
            bool strictly_better = false;
            
            // Quality: higher is better
            if (state_j->quality_score < state_i->quality_score) dominates = false;
            if (state_j->quality_score > state_i->quality_score) strictly_better = true;
            
            // Speed: higher is better
            if (state_j->speed_score < state_i->speed_score) dominates = false;
            if (state_j->speed_score > state_i->speed_score) strictly_better = true;
            
            // Memory: lower is better
            if (state_j->memory_usage > state_i->memory_usage) dominates = false;
            if (state_j->memory_usage < state_i->memory_usage) strictly_better = true;
            
            if (dominates && strictly_better) {
                is_pareto[i] = false;
                break;
            }
        }
    }
    
    // Count Pareto-optimal states
    *count = 0;
    for (uint32_t i = 0; i < ctx->state_graph->state_count; i++) {
        if (is_pareto[i]) (*count)++;
    }
    
    // Collect IDs
    uint64_t* pareto_ids = (uint64_t*)calloc(*count, sizeof(uint64_t));
    uint32_t idx = 0;
    
    for (uint32_t i = 0; i < ctx->state_graph->state_count; i++) {
        if (is_pareto[i]) {
            pareto_ids[idx++] = ctx->state_graph->states[i].state_id;
        }
    }
    
    free(is_pareto);
    
    return pareto_ids;
}

void prune_state_space(OmnidirectionalHotpatch* ctx, uint32_t keep_count) {
    if (ctx->state_graph->state_count <= keep_count) return;
    
    // Find Pareto frontier
    uint32_t pareto_count;
    uint64_t* pareto_ids = find_pareto_frontier(ctx, &pareto_count);
    
    // Mark states to keep
    bool* keep = (bool*)calloc(ctx->state_graph->state_count, sizeof(bool));
    
    // Always keep Pareto-optimal states
    for (uint32_t i = 0; i < pareto_count; i++) {
        HotpatchState* state = get_state(ctx, pareto_ids[i]);
        if (state) {
            uint32_t idx = (uint32_t)(state - ctx->state_graph->states);
            keep[idx] = true;
        }
    }
    
    // Keep current state
    HotpatchState* current = get_state(ctx, ctx->current_state_id);
    if (current) {
        uint32_t idx = (uint32_t)(current - ctx->state_graph->states);
        keep[idx] = true;
    }
    
    // Keep anchor states
    for (uint32_t i = 0; i < ctx->state_graph->anchor_count; i++) {
        HotpatchState* state = get_state(ctx, ctx->state_graph->anchor_state_ids[i]);
        if (state) {
            uint32_t idx = (uint32_t)(state - ctx->state_graph->states);
            keep[idx] = true;
        }
    }
    
    // Count how many we're keeping
    uint32_t keeping = 0;
    for (uint32_t i = 0; i < ctx->state_graph->state_count; i++) {
        if (keep[i]) keeping++;
    }
    
    // If still too many, remove lowest-value states
    if (keeping > keep_count) {
        // Score remaining states
        float* scores = (float*)calloc(ctx->state_graph->state_count, sizeof(float));
        
        for (uint32_t i = 0; i < ctx->state_graph->state_count; i++) {
            if (!keep[i]) {
                HotpatchState* state = &ctx->state_graph->states[i];
                scores[i] = state->quality_score * 0.4f +
                           state->speed_score * 0.3f +
                           (1.0f - state->state_vector[AXIS_MEMORY]) * 0.3f;
            }
        }
        
        // Keep highest-scoring states
        uint32_t need_to_remove = keeping - keep_count;
        for (uint32_t r = 0; r < need_to_remove; r++) {
            float min_score = FLT_MAX;
            uint32_t min_idx = 0;
            
            for (uint32_t i = 0; i < ctx->state_graph->state_count; i++) {
                if (keep[i] && scores[i] < min_score) {
                    min_score = scores[i];
                    min_idx = i;
                }
            }
            
            keep[min_idx] = false;
        }
        
        free(scores);
    }
    
    // Actually remove states
    uint32_t write_idx = 0;
    for (uint32_t i = 0; i < ctx->state_graph->state_count; i++) {
        if (keep[i]) {
            if (write_idx != i) {
                ctx->state_graph->states[write_idx] = ctx->state_graph->states[i];
            }
            write_idx++;
        } else {
            // Free delta data
            if (ctx->state_graph->states[i].delta_data) {
                free(ctx->state_graph->states[i].delta_data);
            }
        }
    }
    
    ctx->state_graph->state_count = write_idx;
    
    free(keep);
    free(pareto_ids);
}

// ============================================================================
// STATISTICS AND REPORTING
// ============================================================================

void get_state_statistics(OmnidirectionalHotpatch* ctx, uint64_t state_id,
                          float* quality, float* speed, uint64_t* memory) {
    HotpatchState* state = get_state(ctx, state_id);
    if (!state) {
        *quality = 0;
        *speed = 0;
        *memory = 0;
        return;
    }
    
    *quality = state->quality_score;
    *speed = state->speed_score;
    *memory = state->memory_usage;
}

void generate_state_report(OmnidirectionalHotpatch* ctx, const char* output_path) {
    FILE* f = fopen(output_path, "w");
    if (!f) {
        return;
    }
    
    fprintf(f, "# Omnidirectional Hotpatch State Report\n\n");
    
    fprintf(f, "## State Graph Statistics\n");
    fprintf(f, "- Total States: %u\n", ctx->state_graph->state_count);
    fprintf(f, "- Total Edges: %u\n", ctx->state_graph->edge_count);
    fprintf(f, "- Anchor States: %u\n", ctx->state_graph->anchor_count);
    fprintf(f, "- Current State: #%lu\n\n", ctx->current_state_id);
    
    fprintf(f, "## States\n\n");
    fprintf(f, "| ID | Quality | Speed | Memory | Sparsity | Description |\n");
    fprintf(f, "|----|---------|-------|--------|----------|-------------|\n");
    
    for (uint32_t i = 0; i < ctx->state_graph->state_count; i++) {
        HotpatchState* state = &ctx->state_graph->states[i];
        fprintf(f, "| %lu | %.4f | %.4f | %lu MB | %.2f | %s |\n",
                state->state_id, state->quality_score, state->speed_score,
                state->memory_usage / (1024 * 1024), state->state_vector[AXIS_SPARSITY],
                state->description);
    }
    
    fprintf(f, "\n## Edges\n\n");
    fprintf(f, "| From | To | Operations | Quality Δ | Speed Δ | Memory Δ | Bidirectional |\n");
    fprintf(f, "|------|-----|------------|-----------|---------|----------|---------------|\n");
    
    for (uint32_t i = 0; i < ctx->state_graph->edge_count; i++) {
        HotpatchEdge* edge = &ctx->state_graph->edges[i];
        fprintf(f, "| %lu | %lu | 0x%X | %.4f | %.4f | %ld | %s |\n",
                edge->from_state_id, edge->to_state_id, edge->operations,
                edge->quality_delta, edge->speed_delta, edge->memory_delta,
                edge->bidirectional ? "Yes" : "No");
    }
    
    fprintf(f, "\n## Navigation Statistics\n");
    fprintf(f, "- Total Navigations: %lu\n", ctx->total_navigations);
    fprintf(f, "- Total Merges: %lu\n", ctx->total_merges);
    fprintf(f, "- Total Interpolations: %lu\n", ctx->total_interpolations);
    
    fclose(f);
}

void visualize_state_space(OmnidirectionalHotpatch* ctx, const char* output_path) {
    // Generate GraphViz DOT file for visualization
    FILE* f = fopen(output_path, "w");
    if (!f) {
        return;
    }
    
    fprintf(f, "digraph StateSpace {\n");
    fprintf(f, "  rankdir=LR;\n");
    fprintf(f, "  node [shape=box];\n\n");
    
    // Nodes (states)
    for (uint32_t i = 0; i < ctx->state_graph->state_count; i++) {
        HotpatchState* state = &ctx->state_graph->states[i];
        
        // Color based on quality
        float quality = state->quality_score;
        int r = (int)((1.0f - quality) * 255);
        int g = (int)(quality * 255);
        int b = 100;
        
        fprintf(f, "  S%lu [label=\"%s\\nQ:%.2f S:%.2f M:%luMB\" fillcolor=\"#%02X%02X%02X\" style=filled];\n",
                state->state_id, state->description,
                state->quality_score, state->speed_score,
                state->memory_usage / (1024 * 1024),
                r, g, b);
    }
    
    fprintf(f, "\n");
    
    // Edges
    for (uint32_t i = 0; i < ctx->state_graph->edge_count; i++) {
        HotpatchEdge* edge = &ctx->state_graph->edges[i];
        
        if (!edge->bidirectional) {
            fprintf(f, "  S%lu -> S%lu [label=\"%.2f\"];\n",
                    edge->from_state_id, edge->to_state_id, edge->quality_delta);
        }
    }
    
    // Mark current state
    fprintf(f, "\n  S%lu [penwidth=3];\n", ctx->current_state_id);
    
    fprintf(f, "}\n");
    fclose(f);
}
