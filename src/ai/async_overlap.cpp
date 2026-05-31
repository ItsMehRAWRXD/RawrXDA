// async_overlap.cpp - Implementation of async overlap perfection
// Part of the Copilot-like inference pipeline.

#include "async_overlap.h"

namespace RawrXD {

AsyncOverlap::AsyncOverlap(PersistentGPULoop* gpu_loop)
    : gpu_loop_(gpu_loop)
{
    stats_ = {};
}

AsyncOverlap::~AsyncOverlap() {
    Stop();
}

bool AsyncOverlap::Initialize() {
    // Initialize queues
    while (!cpu_queue_.empty()) cpu_queue_.pop();
    while (!gpu_queue_.empty()) gpu_queue_.pop();
    while (!ui_queue_.empty()) ui_queue_.pop();
    while (!complete_queue_.empty()) complete_queue_.pop();
    
    return true;
}

void AsyncOverlap::Start() {
    if (running_.load()) {
        return;
    }
    
    running_.store(true);
    stop_flag_.store(false);
    
    pipeline_start_ = std::chrono::steady_clock::now();
    
    // Start all threads
    cpu_thread_ = std::thread(&AsyncOverlap::CPUPrepThread, this);
    gpu_thread_ = std::thread(&AsyncOverlap::GPUExecThread, this);
    ui_thread_ = std::thread(&AsyncOverlap::UIRenderThread, this);
    coordinator_thread_ = std::thread(&AsyncOverlap::CoordinatorThread, this);
}

void AsyncOverlap::Stop() {
    if (!running_.load()) {
        return;
    }
    
    stop_flag_.store(true);
    running_.store(false);
    
    // Notify all condition variables
    cpu_cv_.notify_all();
    gpu_cv_.notify_all();
    ui_cv_.notify_all();
    complete_cv_.notify_all();
    
    // Wait for all threads
    if (cpu_thread_.joinable()) cpu_thread_.join();
    if (gpu_thread_.joinable()) gpu_thread_.join();
    if (ui_thread_.joinable()) ui_thread_.join();
    if (coordinator_thread_.joinable()) coordinator_thread_.join();
}

void AsyncOverlap::SubmitWork(const AsyncWorkItem& work) {
    std::lock_guard<std::mutex> lock(cpu_mutex_);
    cpu_queue_.push(work);
    cpu_cv_.notify_one();
}

void AsyncOverlap::ProcessLoop() {
    // Main processing loop
    // This is called by the coordinator thread
    
    while (!stop_flag_.load()) {
        // Check for completed work
        AsyncWorkItem work;
        bool has_work = false;
        
        {
            std::lock_guard<std::mutex> lock(complete_mutex_);
            if (!complete_queue_.empty()) {
                work = complete_queue_.front();
                complete_queue_.pop();
                has_work = true;
            }
        }
        
        if (has_work) {
            // Update statistics
            {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                stats_.total_tokens++;
                
                // Calculate stage latencies
                auto cpu_latency = std::chrono::duration_cast<std::chrono::microseconds>(
                    work.cpu_end - work.cpu_start);
                auto gpu_latency = std::chrono::duration_cast<std::chrono::microseconds>(
                    work.gpu_end - work.gpu_start);
                auto ui_latency = std::chrono::duration_cast<std::chrono::microseconds>(
                    work.ui_end - work.ui_start);
                
                stats_.avg_cpu_latency = (stats_.avg_cpu_latency * (stats_.total_tokens - 1) + 
                                         cpu_latency) / stats_.total_tokens;
                stats_.avg_gpu_latency = (stats_.avg_gpu_latency * (stats_.total_tokens - 1) + 
                                         gpu_latency) / stats_.total_tokens;
                stats_.avg_ui_latency = (stats_.avg_ui_latency * (stats_.total_tokens - 1) + 
                                        ui_latency) / stats_.total_tokens;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
}

void AsyncOverlap::CPUPrepThread() {
    while (!stop_flag_.load()) {
        AsyncWorkItem work;
        bool has_work = false;
        
        {
            std::unique_lock<std::mutex> lock(cpu_mutex_);
            
            bool has_item = cpu_cv_.wait_for(lock, std::chrono::milliseconds(1), [this]() {
                return !cpu_queue_.empty() || stop_flag_.load();
            });
            
            if (!has_item || cpu_queue_.empty()) {
                continue;
            }
            
            work = cpu_queue_.front();
            cpu_queue_.pop();
            has_work = true;
        }
        
        if (!has_work) {
            continue;
        }
        
        // CPU preparation
        work.cpu_start = std::chrono::steady_clock::now();
        
        if (cpu_prep_callback_) {
            cpu_prep_callback_(work);
        }
        
        work.cpu_end = std::chrono::steady_clock::now();
        work.cpu_done.store(true);
        
        // Move to GPU queue
        {
            std::lock_guard<std::mutex> lock(gpu_mutex_);
            gpu_queue_.push(work);
        }
        gpu_cv_.notify_one();
    }
}

void AsyncOverlap::GPUExecThread() {
    while (!stop_flag_.load()) {
        AsyncWorkItem work;
        bool has_work = false;
        
        {
            std::unique_lock<std::mutex> lock(gpu_mutex_);
            
            bool has_item = gpu_cv_.wait_for(lock, std::chrono::milliseconds(1), [this]() {
                return !gpu_queue_.empty() || stop_flag_.load();
            });
            
            if (!has_item || gpu_queue_.empty()) {
                continue;
            }
            
            work = gpu_queue_.front();
            gpu_queue_.pop();
            has_work = true;
        }
        
        if (!has_work) {
            continue;
        }
        
        // GPU execution
        work.gpu_start = std::chrono::steady_clock::now();
        
        if (gpu_exec_callback_) {
            gpu_exec_callback_(work);
        }
        
        work.gpu_end = std::chrono::steady_clock::now();
        work.gpu_done.store(true);
        
        // Move to UI queue
        {
            std::lock_guard<std::mutex> lock(ui_mutex_);
            ui_queue_.push(work);
        }
        ui_cv_.notify_one();
    }
}

void AsyncOverlap::UIRenderThread() {
    while (!stop_flag_.load()) {
        AsyncWorkItem work;
        bool has_work = false;
        
        {
            std::unique_lock<std::mutex> lock(ui_mutex_);
            
            bool has_item = ui_cv_.wait_for(lock, std::chrono::milliseconds(1), [this]() {
                return !ui_queue_.empty() || stop_flag_.load();
            });
            
            if (!has_item || ui_queue_.empty()) {
                continue;
            }
            
            work = ui_queue_.front();
            ui_queue_.pop();
            has_work = true;
        }
        
        if (!has_work) {
            continue;
        }
        
        // UI rendering
        work.ui_start = std::chrono::steady_clock::now();
        
        if (ui_render_callback_) {
            ui_render_callback_(work);
        }
        
        work.ui_end = std::chrono::steady_clock::now();
        work.ui_done.store(true);
        
        // Move to complete queue
        {
            std::lock_guard<std::mutex> lock(complete_mutex_);
            complete_queue_.push(work);
        }
        complete_cv_.notify_one();
    }
}

void AsyncOverlap::CoordinatorThread() {
    while (!stop_flag_.load()) {
        // Monitor pipeline health
        // Check for stalls, bubbles, etc.
        
        // Calculate overlap metrics
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            
            // Check if all stages are active
            bool cpu_active = false;
            bool gpu_active = false;
            bool ui_active = false;
            
            {
                std::lock_guard<std::mutex> cpu_lock(cpu_mutex_);
                cpu_active = !cpu_queue_.empty();
            }
            
            {
                std::lock_guard<std::mutex> gpu_lock(gpu_mutex_);
                gpu_active = !gpu_queue_.empty();
            }
            
            {
                std::lock_guard<std::mutex> ui_lock(ui_mutex_);
                ui_active = !ui_queue_.empty();
            }
            
            // Calculate utilization
            if (stats_.total_tokens > 0) {
                stats_.cpu_utilization = cpu_active ? 1.0f : 0.0f;
                stats_.gpu_utilization = gpu_active ? 1.0f : 0.0f;
                stats_.ui_utilization = ui_active ? 1.0f : 0.0f;
                
                // Calculate overlap
                if (cpu_active && gpu_active && ui_active) {
                    stats_.overlap_percentage = 1.0f;
                } else if ((cpu_active && gpu_active) || 
                          (cpu_active && ui_active) || 
                          (gpu_active && ui_active)) {
                    stats_.overlap_percentage = 0.67f;
                } else if (cpu_active || gpu_active || ui_active) {
                    stats_.overlap_percentage = 0.33f;
                } else {
                    stats_.overlap_percentage = 0.0f;
                }
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

} // namespace RawrXD