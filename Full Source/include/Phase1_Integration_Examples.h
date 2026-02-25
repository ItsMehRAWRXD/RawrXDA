#pragma once

/*
================================================================================
 PHASE1_INTEGRATION_EXAMPLES.H
 
 This file demonstrates how Phases 2-5 integrate with Phase 1 Foundation.
 Use these patterns as templates for your implementations.
================================================================================
*/

#include "Phase1_Foundation.h"

namespace Phase2_ModelLoader {
/*================================================================================
 PHASE 2: MODEL LOADER - Uses Phase 1 for Memory Allocation
================================================================================*/

class ModelLoader {
public:
    ModelLoader() {
        // Initialize Phase 1 during loader construction
        m_phase1 = Phase1::Foundation::Initialize();
        if (!m_phase1) {
            throw std::runtime_error("Failed to initialize Phase 1");
        }
        
        // Cache CPU capabilities for optimization decisions
        m_has_avx512 = PHASE1_HAS_AVX512();
        m_has_avx2 = PHASE1_HAS_AVX2();
        m_physical_cores = PHASE1_CORES();
    }
    
    // Load model from disk
    struct Model {
        float* weights;
        float* biases;
        uint64_t weight_size;
        uint64_t bias_size;
        uint32_t layer_count;
        bool uses_avx512;
    };
    
    Model* LoadModel(const char* model_path) {
        // Allocate model metadata
        Model* model = (Model*)PHASE1_MALLOC(sizeof(Model));
        
        // Determine optimal kernel
        model->uses_avx512 = m_has_avx512;
        
        // Load weights from disk
        uint64_t weights_size = GetModelWeightSize(model_path);
        
        // Allocate weights on NUMA node 0 for now
        // (Phase 5 orchestrator will do smarter NUMA placement)
        model->weights = (float*)PHASE1_NUMA_MALLOC(0, weights_size, 64);
        if (!model->weights) {
            // Fallback to system arena
            model->weights = (float*)PHASE1_MALLOC(weights_size);
        }
        
        // Read weights from disk
        ReadWeightsFromDisk(model_path, model->weights, weights_size);
        
        // Allocate biases
        uint64_t bias_size = GetModelBiasSize(model_path);
        model->biases = (float*)PHASE1_MALLOC(bias_size);
        ReadBiasesFromDisk(model_path, model->biases, bias_size);
        
        model->weight_size = weights_size;
        model->bias_size = bias_size;
        
        return model;
    }
    
    // Query Phase 1 for resource availability
    void PrintSystemInfo() {
        printf("=== Phase 1 System Information ===\n");
        printf("CPU: %s\n", PHASE1().GetCPUCapabilities().brand_string);
        printf("Physical Cores: %d\n", PHASE1_CORES());
        printf("Logical Threads: %d\n", PHASE1_THREADS());
        printf("NUMA Nodes: %d\n", PHASE1().GetNUMANodeCount());
        printf("AVX-512: %s\n", PHASE1_HAS_AVX512() ? "Yes" : "No");
        printf("AVX2: %s\n", PHASE1_HAS_AVX2() ? "Yes" : "No");
        printf("TSC Frequency: %llu Hz\n", PHASE1().GetTSCFrequency());
    }
    
private:
    Phase1::PHASE1_CONTEXT* m_phase1;
    bool m_has_avx512;
    bool m_has_avx2;
    uint32_t m_physical_cores;
    
    uint64_t GetModelWeightSize(const char* path) { /* ... */ return 0; }
    uint64_t GetModelBiasSize(const char* path) { /* ... */ return 0; }
    void ReadWeightsFromDisk(const char* path, float* buffer, uint64_t size) { /* ... */ }
    void ReadBiasesFromDisk(const char* path, float* buffer, uint64_t size) { /* ... */ }
};

}  // namespace Phase2_ModelLoader

namespace Phase3_AgentKernel {
/*================================================================================
 PHASE 3: AGENT KERNEL - Uses Phase 1 for Timing and Threading
================================================================================*/

class AgentKernel {
public:
    struct AgentState {
        uint64_t id;
        float* context_vector;
        uint32_t context_size;
        uint64_t reasoning_cycles;
        uint64_t decision_time_us;
    };
    
    AgentKernel() {
        m_phase1 = Phase1::Foundation::Initialize();
        m_thread_count = PHASE1_THREADS();
        m_agent_states = (AgentState*)PHASE1_MALLOC(
            sizeof(AgentState) * m_thread_count
        );
        
        // Initialize per-thread agent states
        for (uint32_t i = 0; i < m_thread_count; i++) {
            m_agent_states[i].id = i;
            m_agent_states[i].context_size = 1024;
            m_agent_states[i].context_vector = (float*)PHASE1_MALLOC(
                sizeof(float) * m_agent_states[i].context_size
            );
            m_agent_states[i].reasoning_cycles = 0;
            m_agent_states[i].decision_time_us = 0;
        }
    }
    
    // Agent reasoning loop
    void ReasoningStep(uint32_t agent_id) {
        AgentState& state = m_agent_states[agent_id];
        
        // Timing for adaptive scheduling
        uint64_t reasoning_start = PHASE1_CYCLES();
        uint64_t time_start_us = PHASE1_MICROS();
        
        // Perform reasoning
        std::vector<float> decision = PerformReasoning(state.context_vector);
        
        // Record timing for load balancing
        state.reasoning_cycles = PHASE1_CYCLES() - reasoning_start;
        state.decision_time_us = PHASE1_MICROS() - time_start_us;
        
        // Phase 4 will use this timing for load balancing
        ReportAgentTiming(agent_id, state.decision_time_us);
    }
    
    // Batch reasoning with timing
    void BatchReasoningStep(uint32_t batch_size) {
        uint64_t batch_start_us = PHASE1_MICROS();
        
        for (uint32_t i = 0; i < batch_size; i++) {
            ReasoningStep(i);
        }
        
        uint64_t batch_elapsed_us = PHASE1_MICROS() - batch_start_us;
        printf("Batch reasoning: %.2f ms\n", batch_elapsed_us / 1000.0);
    }
    
    // Query agent timing for Phase 4 load balancing
    uint64_t GetAgentDecisionTime(uint32_t agent_id) {
        return m_agent_states[agent_id].decision_time_us;
    }
    
    const AgentState* GetAgentState(uint32_t agent_id) {
        return &m_agent_states[agent_id];
    }
    
private:
    Phase1::PHASE1_CONTEXT* m_phase1;
    uint32_t m_thread_count;
    AgentState* m_agent_states;
    
    std::vector<float> PerformReasoning(float* context) { /* ... */ return {}; }
    void ReportAgentTiming(uint32_t agent_id, uint64_t time_us) { /* ... */ }
};

}  // namespace Phase3_AgentKernel

namespace Phase4_SwarmInference {
/*================================================================================
 PHASE 4: SWARM INFERENCE - Uses Phase 1 for Memory, Timing, and Load Balancing
================================================================================*/

class SwarmInferenceEngine {
public:
    struct ModelInstance {
        uint32_t model_id;
        float* weights;
        uint64_t weight_size;
        uint32_t numa_node;
        uint64_t last_inference_time_us;
        uint32_t inference_count;
    };
    
    SwarmInferenceEngine(uint32_t num_models) {
        m_phase1 = Phase1::Foundation::Initialize();
        m_num_models = num_models;
        m_numa_nodes = PHASE1().GetNUMANodeCount();
        m_physical_cores = PHASE1_CORES();
        
        // Allocate models
        m_models = (ModelInstance*)PHASE1_MALLOC(sizeof(ModelInstance) * num_models);
        
        // Allocate inference queues per NUMA node
        m_inference_queues = (InferenceQueue*)PHASE1_MALLOC(
            sizeof(InferenceQueue) * m_numa_nodes
        );
        
        printf("Swarm initialized: %d models, %d NUMA nodes, %d cores\n",
               num_models, m_numa_nodes, m_physical_cores);
    }
    
    // Load model onto NUMA node
    void LoadModel(uint32_t model_id, const char* model_path, uint32_t numa_node) {
        ModelInstance& model = m_models[model_id];
        model.model_id = model_id;
        model.numa_node = numa_node % m_numa_nodes;
        
        // Allocate weights on target NUMA node
        uint64_t weight_size = 100 * 1024 * 1024;  // 100MB example
        model.weights = (float*)PHASE1_NUMA_MALLOC(model.numa_node, weight_size);
        if (!model.weights) {
            printf("[WARNING] Failed to allocate NUMA memory, using system arena\n");
            model.weights = (float*)PHASE1_MALLOC(weight_size);
        }
        
        model.weight_size = weight_size;
        model.inference_count = 0;
        model.last_inference_time_us = 0;
        
        // Load weights from disk
        LoadModelWeights(model_path, model.weights, weight_size);
    }
    
    // Single model inference with timing
    InferenceResult Infer(uint32_t model_id, const float* input, uint32_t input_size) {
        ModelInstance& model = m_models[model_id];
        
        // Measure inference time for load balancing
        uint64_t infer_start_us = PHASE1_MICROS();
        
        // Execute inference
        InferenceResult result = ExecuteInference(
            model.weights,
            input,
            input_size
        );
        
        // Record timing
        uint64_t infer_time_us = PHASE1_MICROS() - infer_start_us;
        model.last_inference_time_us = infer_time_us;
        model.inference_count++;
        
        return result;
    }
    
    // Dynamic load balancing based on Phase 1 timing data
    uint32_t SelectFastestModel() {
        uint32_t fastest_model = 0;
        uint64_t fastest_time = UINT64_MAX;
        
        for (uint32_t i = 0; i < m_num_models; i++) {
            if (m_models[i].last_inference_time_us < fastest_time) {
                fastest_time = m_models[i].last_inference_time_us;
                fastest_model = i;
            }
        }
        
        return fastest_model;
    }
    
    // Distribute inference to NUMA nodes
    void EnqueueInference(uint32_t model_id, const InferenceTask& task) {
        uint32_t numa_node = m_models[model_id].numa_node;
        m_inference_queues[numa_node].Enqueue(task);
    }
    
    // Performance metrics using Phase 1 timing
    void PrintPerformanceMetrics() {
        printf("=== Swarm Inference Performance Metrics ===\n");
        for (uint32_t i = 0; i < m_num_models; i++) {
            printf("Model %d: %llu us/inference (%u inferences)\n",
                   i,
                   m_models[i].last_inference_time_us,
                   m_models[i].inference_count);
        }
    }
    
private:
    Phase1::PHASE1_CONTEXT* m_phase1;
    uint32_t m_num_models;
    uint32_t m_numa_nodes;
    uint32_t m_physical_cores;
    ModelInstance* m_models;
    
    struct InferenceQueue {
        void Enqueue(const InferenceTask& task) { /* ... */ }
    };
    InferenceQueue* m_inference_queues;
    
    struct InferenceTask {
        uint32_t model_id;
        float* input;
        uint32_t input_size;
    };
    
    struct InferenceResult {
        float* output;
        uint32_t output_size;
    };
    
    void LoadModelWeights(const char* path, float* buffer, uint64_t size) { /* ... */ }
    InferenceResult ExecuteInference(float* weights, const float* input, uint32_t size) { /* ... */ return {}; }
};

}  // namespace Phase4_SwarmInference

namespace Phase5_Orchestrator {
/*================================================================================
 PHASE 5: ORCHESTRATOR - Uses Phase 1 for Timing and Resource Coordination
================================================================================*/

class SwarmOrchestrator {
public:
    struct TaskMetrics {
        uint64_t task_id;
        uint32_t assigned_model;
        uint32_t target_numa_node;
        uint64_t queue_time_us;
        uint64_t execution_time_us;
        uint64_t total_time_us;
    };
    
    SwarmOrchestrator() {
        m_phase1 = Phase1::Foundation::Initialize();
        m_total_capacity = PHASE1_THREADS() * 100;  // 100 tasks per thread max
        m_current_load = 0;
        
        // Allocate task metrics buffer
        m_task_metrics = (TaskMetrics*)PHASE1_MALLOC(
            sizeof(TaskMetrics) * 10000  // Track last 10000 tasks
        );
        m_metrics_index = 0;
        
        printf("Orchestrator initialized: capacity=%d tasks/frame\n", m_total_capacity);
    }
    
    // Main orchestration loop
    void OrchestrationFrame() {
        uint64_t frame_start_us = PHASE1_MICROS();
        
        // Get pending tasks
        std::vector<Task> pending_tasks = GetPendingTasks();
        
        // Assign tasks to models/NUMA nodes with load balancing
        for (const auto& task : pending_tasks) {
            uint32_t numa_node = SelectOptimalNUMANode(task);
            uint32_t model_id = SelectOptimalModel(numa_node);
            
            // Enqueue on selected model
            EnqueueTaskOnModel(task, model_id, numa_node);
            m_current_load++;
        }
        
        // Process executing tasks
        ProcessExecutingTasks();
        
        // Record frame metrics
        uint64_t frame_elapsed_us = PHASE1_MICROS() - frame_start_us;
        double frame_time_ms = frame_elapsed_us / 1000.0;
        
        printf("Frame: %.2f ms, Load: %d/%d, Throughput: %.0f tasks/sec\n",
               frame_time_ms,
               m_current_load,
               m_total_capacity,
               pending_tasks.size() / (frame_elapsed_us / 1000000.0));
    }
    
    // Dynamic load balancing using Phase 1 timing data
    uint32_t SelectOptimalNUMANode(const Task& task) {
        uint32_t numa_nodes = PHASE1().GetNUMANodeCount();
        
        // For now, round-robin
        static uint32_t next_node = 0;
        uint32_t selected = next_node;
        next_node = (next_node + 1) % numa_nodes;
        
        return selected;
    }
    
    uint32_t SelectOptimalModel(uint32_t numa_node) {
        // Would query Phase 4 for model timings
        // Select fastest model on this NUMA node
        return 0;
    }
    
    // Collect performance metrics
    void PrintOrchestrationMetrics() {
        printf("=== Orchestration Metrics ===\n");
        
        uint64_t total_execution_us = 0;
        uint64_t total_queue_us = 0;
        
        for (uint32_t i = 0; i < m_metrics_index; i++) {
            total_execution_us += m_task_metrics[i].execution_time_us;
            total_queue_us += m_task_metrics[i].queue_time_us;
        }
        
        double avg_execution_ms = (total_execution_us / m_metrics_index) / 1000.0;
        double avg_queue_ms = (total_queue_us / m_metrics_index) / 1000.0;
        
        printf("Average execution time: %.2f ms\n", avg_execution_ms);
        printf("Average queue time: %.2f ms\n", avg_queue_ms);
        printf("Total tasks processed: %d\n", m_metrics_index);
    }
    
private:
    Phase1::PHASE1_CONTEXT* m_phase1;
    uint32_t m_total_capacity;
    uint32_t m_current_load;
    TaskMetrics* m_task_metrics;
    uint32_t m_metrics_index;
    
    struct Task {
        uint64_t task_id;
        float* input;
        uint32_t input_size;
    };
    
    std::vector<Task> GetPendingTasks() { /* ... */ return {}; }
    void ProcessExecutingTasks() { /* ... */ }
    void EnqueueTaskOnModel(const Task& task, uint32_t model_id, uint32_t numa_node) { /* ... */ }
};

}  // namespace Phase5_Orchestrator
