#include "large_model_optimizer.hpp"
#include <QFileInfo>
#include <cmath>

LargeModelOptimizer::LargeModelOptimizer(QObject* parent) : QObject(parent) {}

LargeModelOptimizer::ModelAnalysis LargeModelOptimizer::analyzeLargeModel(const std::string& model_path) {
    ModelAnalysis analysis;
    analysis.model_path = model_path;
    
    QFileInfo info(QString::fromStdString(model_path));
    analysis.total_size_bytes = info.size();
    
    // Estimate parameters based on size (rough approximation for GGUF)
    // Assuming average 6 bits per parameter for mixed quantization models
    analysis.parameter_count = static_cast<size_t>(analysis.total_size_bytes * 8.0 / 6.0);
    
    analysis.estimated_memory_4bit = calculate4BitMemory(analysis.total_size_bytes);
    analysis.estimated_memory_3bit = calculate3BitMemory(analysis.total_size_bytes);
    analysis.estimated_memory_2bit = calculate2BitMemory(analysis.total_size_bytes);
    
    analysis.supports_streaming = true;
    analysis.supports_quantization = true;
    
    analysis.layer_sizes = analyzeLayerDistribution(model_path);
    analysis.critical_layers = identifyCriticalLayers(analysis.layer_sizes);
    analysis.optional_layers = identifyOptionalLayers(analysis.layer_sizes);
    
    return analysis;
}

LargeModelOptimizer::OptimizationPlan LargeModelOptimizer::createOptimizationPlan(const std::string& model_path, 
                                                                                 size_t available_memory) {
    OptimizationPlan plan;
    plan.original_model_path = model_path;
    
    QFileInfo info(QString::fromStdString(model_path));
    plan.original_size = info.size();
    
    // Determine best strategy
    if (plan.original_size <= available_memory) {
        plan.optimized_size = plan.original_size;
        plan.memory_reduction = 0.0;
        plan.recommended_strategy = "Direct Load";
        plan.requires_streaming = false;
        plan.requires_quantization = false;
    } else {
        // Try streaming first
        if (plan.original_size <= available_memory * 1.5) {
            plan.optimized_size = plan.original_size; // Virtual size
            plan.memory_reduction = 0.0;
            plan.recommended_strategy = "Streaming";
            plan.requires_streaming = true;
            plan.requires_quantization = false;
            plan.applied_techniques.push_back("Streaming");
        } else {
            // Need quantization
            size_t size_4bit = calculate4BitMemory(plan.original_size);
            if (size_4bit <= available_memory) {
                plan.optimized_size = size_4bit;
                plan.memory_reduction = 1.0 - (double)size_4bit / plan.original_size;
                plan.recommended_strategy = "4-bit Quantization";
                plan.requires_streaming = false;
                plan.requires_quantization = true;
                plan.applied_techniques.push_back("Q4_K_M Quantization");
            } else {
                // Quantization + Streaming
                plan.optimized_size = size_4bit;
                plan.memory_reduction = 1.0 - (double)size_4bit / plan.original_size;
                plan.recommended_strategy = "4-bit Quantization + Streaming";
                plan.requires_streaming = true;
                plan.requires_quantization = true;
                plan.applied_techniques.push_back("Q4_K_M Quantization");
                plan.applied_techniques.push_back("Streaming");
            }
        }
    }
    
    return plan;
}

LargeModelOptimizer::ModelRecommendation LargeModelOptimizer::recommendModelConfiguration(const std::string& model_path, 
                                                                                       size_t available_memory) {
    ModelRecommendation rec;
    rec.model_id = QFileInfo(QString::fromStdString(model_path)).fileName().toStdString();
    
    QFileInfo info(QString::fromStdString(model_path));
    size_t size = info.size();
    
    rec.memory_requirement_4bit = calculate4BitMemory(size);
    rec.memory_requirement_3bit = calculate3BitMemory(size);
    rec.memory_requirement_2bit = calculate2BitMemory(size);
    
    rec.can_run_full = size <= available_memory;
    rec.can_run_4bit = rec.memory_requirement_4bit <= available_memory;
    rec.can_run_3bit = rec.memory_requirement_3bit <= available_memory;
    rec.can_run_2bit = rec.memory_requirement_2bit <= available_memory;
    
    if (rec.can_run_full) {
        rec.recommended_precision = "Original";
        rec.expected_quality_loss = 0.0;
    } else if (rec.can_run_4bit) {
        rec.recommended_precision = "Q4_K_M";
        rec.expected_quality_loss = 0.05;
    } else if (rec.can_run_3bit) {
        rec.recommended_precision = "Q3_K_M";
        rec.expected_quality_loss = 0.12;
    } else if (rec.can_run_2bit) {
        rec.recommended_precision = "Q2_K";
        rec.expected_quality_loss = 0.25;
    } else {
        rec.recommended_precision = "Q2_K + Streaming";
        rec.expected_quality_loss = 0.25;
    }
    
    return rec;
}

size_t LargeModelOptimizer::estimateParameterCount(const std::string& model_path) {
    QFileInfo info(QString::fromStdString(model_path));
    return static_cast<size_t>(info.size() * 8.0 / 16.0); // Assume FP16
}

std::map<std::string, size_t> LargeModelOptimizer::analyzeLayerDistribution(const std::string& model_path) {
    // Placeholder
    return {};
}

std::vector<std::string> LargeModelOptimizer::identifyCriticalLayers(const std::map<std::string, size_t>& layer_sizes) {
    return {"token_embd", "output", "norm"};
}

std::vector<std::string> LargeModelOptimizer::identifyOptionalLayers(const std::map<std::string, size_t>& layer_sizes) {
    return {};
}

size_t LargeModelOptimizer::calculate4BitMemory(size_t original_size) {
    return original_size / 3; // Approx compression from FP16
}

size_t LargeModelOptimizer::calculate3BitMemory(size_t original_size) {
    return original_size / 4;
}

size_t LargeModelOptimizer::calculate2BitMemory(size_t original_size) {
    return original_size / 6;
}

double LargeModelOptimizer::estimateQualityLoss(int quantization_bits) {
    if (quantization_bits >= 16) return 0.0;
    if (quantization_bits >= 8) return 0.01;
    if (quantization_bits >= 4) return 0.05;
    if (quantization_bits >= 3) return 0.15;
    return 0.30;
}

std::string LargeModelOptimizer::recommendStreamingStrategy(size_t model_size, size_t available_memory) {
    if (model_size <= available_memory) return "None";
    return "Adaptive";
}

std::string LargeModelOptimizer::recommendQuantizationLevel(size_t model_size, size_t available_memory) {
    if (model_size <= available_memory) return "None";
    return "Q4_K_M";
}

bool LargeModelOptimizer::canRunWithStreaming(size_t model_size, size_t available_memory) {
    return model_size <= available_memory * 2; // Can stream up to 2x memory
}

bool LargeModelOptimizer::canRunWithQuantization(size_t model_size, size_t available_memory, int bits) {
    double compression = 16.0 / bits;
    return (model_size / compression) <= available_memory;
}
