#include "lazy_model_loader.hpp"
#include <QDebug>
#include <QFileInfo>

LazyModelLoader::LazyModelLoader(QObject* parent)
    : QObject(parent),
      optimization_timer(new QTimer(this)),
      prefetch_timer(new QTimer(this)) {
    
    connect(optimization_timer, &QTimer::timeout, this, &LazyModelLoader::optimizeLoadingStrategy);
    connect(prefetch_timer, &QTimer::timeout, this, &LazyModelLoader::prefetchCriticalLayers);
    
    optimization_timer->start(10000); // Optimize every 10 seconds
}

LazyModelLoader::~LazyModelLoader() {
    shutdown();
}

bool LazyModelLoader::initialize(StreamingGGUFMemoryManager* manager) {
    if (!manager) return false;
    
    memory_manager = manager;
    initialized = true;
    
    connect(memory_manager, SIGNAL(memoryPressureDetected(MemoryPressure, size_t, size_t)),
            this, SLOT(onMemoryPressure(int, size_t, size_t)));
            
    return true;
}

void LazyModelLoader::shutdown() {
    initialized = false;
    registered_models.clear();
    model_paths.clear();
}

bool LazyModelLoader::registerModel(const std::string& model_path, const std::string& model_id) {
    if (!initialized) return false;
    
    auto info = std::make_unique<ModelInfo>();
    info->model_path = model_path;
    info->model_id = model_id;
    info->total_size = estimateModelSize(model_path);
    info->loaded_size = 0;
    info->fully_loaded = false;
    
    identifyCriticalTensors(*info);
    identifyLayerTensors(*info);
    
    registered_models[model_id] = std::move(info);
    model_paths[model_id] = model_path;
    
    return true;
}

bool LazyModelLoader::loadModelLazy(const std::string& model_id) {
    if (!initialized || registered_models.find(model_id) == registered_models.end()) {
        return false;
    }
    
    emit modelLoadStarted(QString::fromStdString(model_id));
    
    // Initialize streaming in memory manager
    if (!memory_manager->streamModel(model_paths[model_id], model_id)) {
        return false;
    }
    
    // Apply loading strategy
    LoadingStrategy strategy = determineOptimalStrategy(model_id);
    applyLoadingStrategy(model_id, strategy);
    
    emit modelLoadCompleted(QString::fromStdString(model_id));
    return true;
}

bool LazyModelLoader::unloadModelLazy(const std::string& model_id) {
    if (!initialized) return false;
    
    memory_manager->unloadStreamedModel(model_id);
    
    auto it = registered_models.find(model_id);
    if (it != registered_models.end()) {
        it->second->loaded_size = 0;
        it->second->fully_loaded = false;
        it->second->tensor_loaded.clear();
    }
    
    return true;
}

bool LazyModelLoader::isModelLoaded(const std::string& model_id) const {
    return memory_manager->isModelStreamed(model_id);
}

std::vector<float> LazyModelLoader::getTensorLazy(const std::string& model_id, 
                                                 const std::string& tensor_name,
                                                 size_t offset, size_t count) {
    if (!initialized) return {};
    
    emit tensorLoadRequested(QString::fromStdString(model_id), QString::fromStdString(tensor_name));
    
    return memory_manager->accessTensor(model_id, tensor_name, offset, count);
}

size_t LazyModelLoader::estimateModelSize(const std::string& model_path) {
    QFileInfo info(QString::fromStdString(model_path));
    return info.size();
}

bool LazyModelLoader::canFitInMemory(const std::string& model_path, size_t available_memory) const {
    QFileInfo info(QString::fromStdString(model_path));
    return static_cast<size_t>(info.size()) <= available_memory;
}

std::string LazyModelLoader::suggestOptimalQuantization(const std::string& model_path, size_t target_memory) const {
    // Placeholder logic
    size_t size = const_cast<LazyModelLoader*>(this)->estimateModelSize(model_path);
    if (size <= target_memory) return "F16";
    if (size / 2 <= target_memory) return "Q8_0";
    if (size / 4 <= target_memory) return "Q4_0";
    return "Q2_K";
}

void LazyModelLoader::onMemoryPressure(int level, size_t current_usage, size_t budget) {
    if (level >= 2) { // HIGH or CRITICAL
        // Switch to more aggressive lazy loading
        setLoadingStrategy(LoadingStrategy::FULL_LAZY);
        setPrefetchCriticalLayers(false);
    }
}

void LazyModelLoader::optimizeLoadingStrategy() {
    // Periodically re-evaluate strategy based on usage
    for (const auto& [id, info] : registered_models) {
        if (isModelLoaded(id)) {
            LoadingStrategy new_strategy = determineOptimalStrategy(id);
            if (new_strategy != loading_strategy) {
                applyLoadingStrategy(id, new_strategy);
            }
        }
    }
}

void LazyModelLoader::prefetchCriticalLayers() {
    if (!prefetch_critical_layers) return;
    
    for (const auto& [id, info] : registered_models) {
        if (isModelLoaded(id)) {
            loadCriticalTensors(id);
        }
    }
}

// Helper implementations
void LazyModelLoader::identifyCriticalTensors(ModelInfo& model_info) {
    // Identify embeddings, output weights, etc.
    model_info.critical_tensors.push_back("token_embd.weight");
    model_info.critical_tensors.push_back("output.weight");
    model_info.critical_tensors.push_back("output_norm.weight");
}

void LazyModelLoader::identifyLayerTensors(ModelInfo& model_info) {
    // Identify layer structure
    // This would parse the GGUF structure in a real implementation
}

size_t LazyModelLoader::calculateTensorSize(const std::string& model_path, const std::string& tensor_name) {
    return 0; // Placeholder
}

std::vector<std::string> LazyModelLoader::getTensorsForLayer(const std::string& model_id, size_t layer_index) {
    return {}; // Placeholder
}

LazyModelLoader::LoadingStrategy LazyModelLoader::determineOptimalStrategy(const std::string& model_id) {
    return LoadingStrategy::ADAPTIVE;
}

bool LazyModelLoader::applyLoadingStrategy(const std::string& model_id, LoadingStrategy strategy) {
    loading_strategy = strategy;
    emit loadingStrategyApplied("Adaptive", 0.0);
    return true;
}

void LazyModelLoader::optimizeMemoryLayout(const std::string& model_id) {
    // Reorder blocks based on access patterns
}

bool LazyModelLoader::loadCriticalTensors(const std::string& model_id) {
    auto it = registered_models.find(model_id);
    if (it == registered_models.end()) return false;
    
    for (const auto& tensor : it->second->critical_tensors) {
        memory_manager->accessTensor(model_id, tensor);
    }
    return true;
}

bool LazyModelLoader::loadLayerTensors(const std::string& model_id, size_t layer_index) {
    return true;
}

bool LazyModelLoader::loadTensorOnDemand(const std::string& model_id, const std::string& tensor_name) {
    memory_manager->accessTensor(model_id, tensor_name);
    return true;
}

std::string LazyModelLoader::makeModelKey(const std::string& model_path) const {
    return model_path;
}

bool LazyModelLoader::isCriticalTensor(const std::string& tensor_name) const {
    return tensor_name.find("embd") != std::string::npos || tensor_name.find("output") != std::string::npos;
}

bool LazyModelLoader::isLayerTensor(const std::string& tensor_name) const {
    return tensor_name.find("blk") != std::string::npos;
}

size_t LazyModelLoader::getLayerIndex(const std::string& tensor_name) const {
    return 0;
}
