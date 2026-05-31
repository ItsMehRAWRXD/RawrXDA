// ============================================================================
// OMNIDIRECTIONAL HOTPATCH SYSTEM
// State Graph, Vector Navigation, Multi-Path Hotpatching
// ============================================================================

#ifndef OMNIDIRECTIONAL_HOTPATCH_H
#define OMNIDIRECTIONAL_HOTPATCH_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// STATE GRAPH - Hotpatch states as nodes in a graph, not linear history
// ============================================================================

// Hotpatch operations bitmask
typedef enum {
    HOTPATCH_NONE = 0,
    HOTPATCH_PRUNE_WEIGHTS = 1 << 0,
    HOTPATCH_QUANTIZE = 1 << 1,
    HOTPACK_KV = 1 << 2,
    HOTPATCH_PRUNE_HEADS = 1 << 3,
    HOTPATCH_PRUNE_LAYERS = 1 << 4,
    HOTPATCH_FUSE_OPS = 1 << 5,
    HOTPATCH_COMPRESS = 1 << 6,
    HOTPATCH_OFFLOAD = 1 << 7,
    HOTPATCH_RECOMPUTE = 1 << 8,
    HOTPATCH_ALL = 0xFFFFFFFF
} HotpatchOp;

// State node in the graph
typedef struct HotpatchState {
    uint64_t state_id;
    uint64_t timestamp;
    
    // State properties (can be used as navigation targets)
    float quality_score;
    float speed_score;
    uint64_t memory_usage;
    uint64_t compute_usage;
    
    // State vector in multi-dimensional space
    // Dimensions: quality, speed, memory, compute, sparsity, precision, features, stability
    float state_vector[8];
    
    // Parent states (for rollback/merge)
    uint64_t parent_states[4];
    uint32_t parent_count;
    
    // Child states (for forward navigation)
    uint64_t child_states[16];
    uint32_t child_count;
    
    // Delta from nearest anchor state
    void* delta_data;
    size_t delta_size;
    uint64_t anchor_state_id;
    
    // Operations to reach this state from anchor
    HotpatchOp operations;
    
    // State metadata
    char description[256];
    bool is_anchor;        // Can be used as anchor for other deltas
    bool is_stable;        // Passed quality checks
    float transition_cost; // Cost to transition to this state
    
} HotpatchState;

// Edge between states (transition)
typedef struct HotpatchEdge {
    uint64_t from_state_id;
    uint64_t to_state_id;
    
    // Transition properties
    HotpatchOp operations;
    float quality_delta;
    float speed_delta;
    int64_t memory_delta;
    float transition_cost;    // Time/compute cost
    
    // Direction in state space
    float direction_vector[8];  // Normalized direction
    
    // Bidirectional flag
    bool bidirectional;
    bool reversible;
    
    // Cached transition data
    void* forward_delta;
    void* reverse_delta;
    size_t forward_size;
    size_t reverse_size;
    
} HotpatchEdge;

// State space graph
typedef struct {
    HotpatchState* states;
    uint32_t state_count;
    uint32_t state_capacity;
    
    HotpatchEdge* edges;
    uint32_t edge_count;
    uint32_t edge_capacity;
    
    // State indices for fast lookup
    HotpatchState** state_index;   // Indexed by state_id
    uint64_t max_state_id;
    
    // Anchor states (key states used as delta bases)
    uint64_t* anchor_state_ids;
    uint32_t anchor_count;
    
    // Current position
    uint64_t current_state_id;
    
    // Statistics
    uint32_t total_transitions;
    uint32_t successful_transitions;
    uint32_t failed_transitions;
    
} StateGraph;

// ============================================================================
// VECTOR NAVIGATION - Navigate state space in any direction
// ============================================================================

// Direction in state space
typedef struct {
    float vector[8];          // Direction vector
    float magnitude;          // How far to go
    float priority[8];        // Priority for each dimension
    
    // Target properties
    float target_quality;
    float target_speed;
    uint64_t target_memory;
    uint64_t target_compute;
    
    // Constraints
    float min_quality;
    float min_speed;
    uint64_t max_memory;
    float max_transition_cost;
    
} NavigationVector;

// Navigation result
typedef struct {
    uint64_t from_state_id;
    uint64_t to_state_id;
    float actual_direction[8];
    float distance_traveled;
    float quality_change;
    float speed_change;
    int64_t memory_change;
    uint32_t path_length;        // Number of intermediate states
    uint64_t path_state_ids[32];  // Path through state graph
    
    bool success;
    char failure_reason[256];
    
} NavigationResult;

// Multi-dimensional state space axes
typedef enum {
    AXIS_QUALITY = 0,       // Higher is better
    AXIS_SPEED = 1,         // Higher is better (tokens/sec)
    AXIS_MEMORY = 2,        // Lower is better (bytes)
    AXIS_COMPUTE = 3,       // Lower is better
    AXIS_SPARSITY = 4,      // Model sparsity
    AXIS_PRECISION = 5,     // Quantization level (higher = more precision)
    AXIS_FEATURES = 6,      // Number of active features/heads
    AXIS_STABILITY = 7      // State stability/confidence
    
} StateAxis;

// Navigation strategies
typedef enum {
    NAV_GREEDY,              // Best immediate improvement
    NAV_BALANCED,            // Balance all objectives
    NAV_OPTIMAL,            // Find optimal path through graph
    NAV_EXPLORATORY,        // Explore state space
    NAV_CONSERVATIVE,       // Minimize quality loss
    NAV_AGGRESSIVE,         // Maximize gains
    NAV_MINIMAL_TRANSITION, // Fewest state transitions
    NAV_LEAST_COST,         // Lowest transition cost
    
} NavigationStrategy;

// ============================================================================
// MERGE AND BLEND - Combine multiple hotpatch states
// ============================================================================

// Merge strategy
typedef enum {
    MERGE_WEIGHTED_AVERAGE,   // Weighted average of properties
    MERGE_BEST_QUALITY,       // Take best quality elements
    MERGE_BEST_SPEED,         // Take best speed elements
    MERGE_INTERSECTION,       // Only keep common elements
    MERGE_UNION,              // Keep all elements
    MERGE_INTERPOLATE,        // Interpolate between states
    
} MergeStrategy;

// Conflict resolution
typedef enum {
    CONFLICT_KEEP_FIRST,
    CONFLICT_KEEP_LAST,
    CONFLICT_AVERAGE,
    CONFLICT_KEEP_BEST,
    CONFLICT_KEEP_SMALLEST,
    CONFLICT_KEEP_LARGEST,
    
} ConflictResolution;

// State merge configuration
typedef struct {
    uint64_t* source_state_ids;
    uint32_t source_count;
    
    // Merge weights
    float* weights;          // Weight for each source state
    
    // Merge strategy
    MergeStrategy strategy;
    
    // Conflict resolution
    ConflictResolution conflict_mode;
    
    // Output constraints
    float min_quality;
    uint64_t max_memory;
    
} MergeConfig;

// ============================================================================
// STATE INTERPOLATION - Morph between states
// ============================================================================

// Interpolation mode
typedef enum {
    INTERP_LINEAR,          // Linear interpolation
    INTERP_SMOOTH,          // Smoothstep interpolation
    INTERP_EASE_IN,         // Ease-in interpolation
    INTERP_EASE_OUT,        // Ease-out interpolation
    INTERP_EASE_IN_OUT,     // Ease-in-out interpolation
    INTERP_SPRING,          // Spring-like interpolation
    INTERP_EXPONENTIAL,     // Exponential interpolation
    INTERP_BEZIER,          // Bezier curve interpolation
    
} InterpolateMode;

// Interpolation between two states
typedef struct {
    uint64_t from_state_id;
    uint64_t to_state_id;
    
    float t;              // Interpolation parameter [0, 1]
    float step_size;      // Step size for discrete interpolation
    
    // Interpolation mode
    InterpolateMode mode;
    
    // Per-axis interpolation
    float axis_weights[8];
    
    // Output
    uint64_t interpolated_state_id;
    float interpolated_vector[8];
    
} InterpolationConfig;

// ============================================================================
// PATH FINDING - Find optimal path through state space
// ============================================================================

// Path type
typedef enum {
    PATH_DIRECT,           // Direct edge between states
    PATH_OPTIMAL,          // Optimal path found
    PATH_HEURISTIC,        // Heuristic path
    PATH_EXPLORATORY,      // Exploratory path
    
} PathType;

// Path in state graph
typedef struct {
    uint64_t* state_ids;
    uint32_t state_count;
    
    // Path properties
    float total_quality_delta;
    float total_speed_delta;
    int64_t total_memory_delta;
    float total_cost;
    
    // Path type
    PathType path_type;
    
} StatePath;

// ============================================================================
// OMNIDIRECTIONAL HOTPATCH CONTEXT
// ============================================================================

typedef struct OmnidirectionalHotpatch {
    // State graph
    StateGraph* state_graph;
    
    // Current state
    uint64_t current_state_id;
    float current_state_vector[8];
    
    // Navigation state
    NavigationVector pending_navigation;
    NavigationStrategy current_strategy;
    
    // State history (for non-linear navigation)
    uint64_t* visited_states;
    uint32_t visited_count;
    uint32_t visited_capacity;
    
    // Statistics
    uint64_t total_navigations;
    uint64_t total_merges;
    uint64_t total_interpolations;
    uint64_t total_path_finds;
    
    // Configuration
    float max_state_distance;      // Max distance between states
    uint32_t max_path_length;       // Max path length
    float quality_weight;
    float speed_weight;
    float memory_weight;
    
} OmnidirectionalHotpatch;

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

// Initialization
OmnidirectionalHotpatch* omni_hotpatch_create(void);
void omni_hotpatch_destroy(OmnidirectionalHotpatch* ctx);

// State management
uint64_t create_state(OmnidirectionalHotpatch* ctx, float quality, float speed, 
                      uint64_t memory, const char* description);
bool delete_state(OmnidirectionalHotpatch* ctx, uint64_t state_id);
HotpatchState* get_state(OmnidirectionalHotpatch* ctx, uint64_t state_id);

// State graph operations
bool add_edge(OmnidirectionalHotpatch* ctx, uint64_t from_id, uint64_t to_id, 
              HotpatchOp ops, bool bidirectional);
bool remove_edge(OmnidirectionalHotpatch* ctx, uint64_t from_id, uint64_t to_id);
HotpatchEdge* find_edge(OmnidirectionalHotpatch* ctx, uint64_t from_id, uint64_t to_id);

// Vector navigation (non-directional hotpatching)
NavigationResult navigate_to_state(OmnidirectionalHotpatch* ctx, uint64_t target_state_id);
NavigationResult navigate_to_vector(OmnidirectionalHotpatch* ctx, float target_vector[8]);
NavigationResult navigate_to_properties(OmnidirectionalHotpatch* ctx, 
                                         float target_quality, float target_speed,
                                         uint64_t target_memory);
NavigationResult navigate_direction(OmnidirectionalHotpatch* ctx, 
                                     float direction[8], float magnitude);
NavigationResult navigate_random(OmnidirectionalHotpatch* ctx, float exploration_factor);

// Multi-directional navigation
NavigationResult navigate_forward(OmnidirectionalHotpatch* ctx);
NavigationResult navigate_backward(OmnidirectionalHotpatch* ctx);
NavigationResult navigate_to_best(OmnidirectionalHotpatch* ctx);
NavigationResult navigate_to_nearest_anchor(OmnidirectionalHotpatch* ctx);
NavigationResult navigate_away_from_worst(OmnidirectionalHotpatch* ctx);

// Path finding
StatePath* find_path(OmnidirectionalHotpatch* ctx, uint64_t from_id, uint64_t to_id);
StatePath* find_optimal_path(OmnidirectionalHotpatch* ctx, NavigationVector* nav);
StatePath* find_shortest_path(OmnidirectionalHotpatch* ctx, uint64_t from_id, uint64_t to_id);
StatePath* find_cheapest_path(OmnidirectionalHotpatch* ctx, uint64_t from_id, uint64_t to_id);
void free_path(StatePath* path);

// Merge and blend operations
uint64_t merge_states(OmnidirectionalHotpatch* ctx, MergeConfig* config);
uint64_t blend_states(OmnidirectionalHotpatch* ctx, uint64_t state1_id, uint64_t state2_id, 
                      float blend_factor);
uint64_t interpolate_states(OmnidirectionalHotpatch* ctx, InterpolationConfig* config);

// Utility functions
float calculate_state_distance(OmnidirectionalHotpatch* ctx, uint64_t state1_id, uint64_t state2_id);
void calculate_direction_vector(OmnidirectionalHotpatch* ctx, uint64_t from_id, 
                                uint64_t to_id, float direction[8]);
uint64_t find_nearest_state(OmnidirectionalHotpatch* ctx, float target_vector[8]);
uint64_t find_best_state(OmnidirectionalHotpatch* ctx, float quality_weight, 
                         float speed_weight, float memory_weight);
bool can_transition(OmnidirectionalHotpatch* ctx, uint64_t from_id, uint64_t to_id);

// Hotpatch operations (omnidirectional)
bool apply_omni_hotpatch(OmnidirectionalHotpatch* ctx, HotpatchOp ops, 
                         NavigationVector* nav);
bool unhotpatch(OmnidirectionalHotpatch* ctx);
bool rehotpatch(OmnidirectionalHotpatch* ctx, uint64_t target_state_id);
bool cross_hotpatch(OmnidirectionalHotpatch* ctx, uint64_t state1_id, 
                    uint64_t state2_id, float crossover_point);
bool parallel_hotpatch(OmnidirectionalHotpatch* ctx, HotpatchOp ops1, 
                       HotpatchOp ops2, uint32_t num_paths);

// State space exploration
void explore_state_space(OmnidirectionalHotpatch* ctx, uint32_t iterations);
uint64_t* find_pareto_frontier(OmnidirectionalHotpatch* ctx, uint32_t* count);
void prune_state_space(OmnidirectionalHotpatch* ctx, uint32_t keep_count);

// Statistics and reporting
void get_state_statistics(OmnidirectionalHotpatch* ctx, uint64_t state_id, 
                          float* quality, float* speed, uint64_t* memory);
void generate_state_report(OmnidirectionalHotpatch* ctx, const char* output_path);
void visualize_state_space(OmnidirectionalHotpatch* ctx, const char* output_path);

#ifdef __cplusplus
}
#endif

#endif // OMNIDIRECTIONAL_HOTPATCH_H
