// model_registry.cpp — In-memory implementation for ModelRegistry (Win32, no Qt)
// Persistent SQLite-backed implementation can replace this later.

#include "../include/model_registry.h"
#include <algorithm>
#include <chrono>

ModelRegistry::ModelRegistry(void* parent) : m_parent(parent) {}

ModelRegistry::~ModelRegistry() = default;

void ModelRegistry::initialize() {
    m_models.clear();
    m_selectedModelId = 0;
    m_dbHandle = nullptr;
}

bool ModelRegistry::registerModel(const ModelVersion& version) {
    ModelVersion v = version;
    int nextId = 1;
    for (const auto& m : m_models)
        if (m.id >= nextId) nextId = m.id + 1;
    v.id = nextId;
    if (m_models.empty())
        v.isActive = true;
    else
        v.isActive = false;
    if (v.createdAt == 0) {
        v.createdAt = static_cast<int64_t>(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    }
    m_models.push_back(v);
    if (m_updatedCb) m_updatedCb(m_updatedCtx);
    return true;
}

std::vector<ModelVersion> ModelRegistry::getAllModels() const {
    return m_models;
}

ModelVersion ModelRegistry::getModel(int id) const {
    auto it = std::find_if(m_models.begin(), m_models.end(),
        [id](const ModelVersion& m) { return m.id == id; });
    if (it != m_models.end()) return *it;
    return {};
}

bool ModelRegistry::deleteModel(int id) {
    auto it = std::find_if(m_models.begin(), m_models.end(),
        [id](const ModelVersion& m) { return m.id == id; });
    if (it == m_models.end()) return false;
    bool wasActive = it->isActive;
    m_models.erase(it);
    if (wasActive && !m_models.empty()) {
        m_models.front().isActive = true;
        m_selectedModelId = m_models.front().id;
    } else if (m_models.empty()) {
        m_selectedModelId = 0;
    }
    if (m_deletedCb) m_deletedCb(m_deletedCtx, id);
    if (m_updatedCb) m_updatedCb(m_updatedCtx);
    return true;
}

bool ModelRegistry::setActiveModel(int id) {
    auto it = std::find_if(m_models.begin(), m_models.end(),
        [id](const ModelVersion& m) { return m.id == id; });
    if (it == m_models.end()) return false;
    for (auto& m : m_models) m.isActive = false;
    it->isActive = true;
    m_selectedModelId = id;
    if (m_selectedCb && !it->path.empty())
        m_selectedCb(m_selectedCtx, it->path.c_str());
    if (m_updatedCb) m_updatedCb(m_updatedCtx);
    return true;
}

ModelVersion ModelRegistry::getActiveModel() const {
    auto it = std::find_if(m_models.begin(), m_models.end(),
        [](const ModelVersion& m) { return m.isActive; });
    if (it != m_models.end()) return *it;
    return {};
}

void ModelRegistry::setSelectedCallback(ModelSelectedCallback cb, void* ctx) {
    m_selectedCb = cb;
    m_selectedCtx = ctx;
}

void ModelRegistry::setUpdatedCallback(ModelUpdatedCallback cb, void* ctx) {
    m_updatedCb = cb;
    m_updatedCtx = ctx;
}

void ModelRegistry::setDeletedCallback(ModelDeletedCallback cb, void* ctx) {
    m_deletedCb = cb;
    m_deletedCtx = ctx;
}

void ModelRegistry::setShowCallback(ShowCallback cb, void* ctx) {
    m_showCb = cb;
    m_showCtx = ctx;
}

void ModelRegistry::show() {
    if (m_showCb) m_showCb(m_showCtx);
}
