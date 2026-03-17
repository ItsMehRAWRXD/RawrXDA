#include "gpu_ray_tracing.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <chrono>

namespace RawrXD {
namespace GPU {

// ============================================================================
// BVHBuilder Implementation
// ============================================================================

BVHBuilder::BVHBuilder(uint32_t device_id) : device_id_(device_id) {
    // Initialize with a root node
    BVHNode root;
    root.is_leaf = true;
    nodes_.push_back(root);
}

BVHBuilder::~BVHBuilder() {
    QMutexLocker lock(&mutex_);
    nodes_.clear();
    triangles_.clear();
    triangle_indices_.clear();
}

uint32_t BVHBuilder::add_geometry(const std::vector<Triangle>& triangles) {
    QMutexLocker lock(&mutex_);

    uint32_t start_idx = triangles_.size();

    for (const auto& tri : triangles) {
        triangles_.push_back(tri);
        triangle_indices_.push_back(static_cast<uint32_t>(triangles_.size() - 1));
    }

    return start_idx;
}

bool BVHBuilder::build() {
    QMutexLocker lock(&mutex_);

    if (triangles_.empty()) {
        return false;
    }

    // Clear existing nodes except root
    nodes_.clear();

    // Build BVH recursively
    build_recursive(0, triangles_.size(), 0);

    return true;
}

uint32_t BVHBuilder::build_recursive(uint32_t start, uint32_t end, uint32_t depth) {
    uint32_t node_idx = nodes_.size();

    BVHNode node;
    node.first_primitive = start;
    node.primitive_count = end - start;

    // Calculate bounds
    for (uint32_t i = start; i < end; ++i) {
        node.bounds.expand(triangles_[triangle_indices_[i]].get_bounds());
    }

    // Leaf node if few primitives
    const uint32_t max_primitives_per_leaf = 4;
    if (node.primitive_count <= max_primitives_per_leaf) {
        node.is_leaf = true;
        nodes_.push_back(node);
        return node_idx;
    }

    // Find best split using SAH
    uint32_t split = calculate_sah_split(start, end);

    if (split == start || split == end) {
        // No good split found, make leaf
        node.is_leaf = true;
        nodes_.push_back(node);
        return node_idx;
    }

    // Build children
    node.is_leaf = false;
    node.left_child = build_recursive(start, split, depth + 1);
    node.right_child = build_recursive(split, end, depth + 1);

    nodes_.push_back(node);
    return node_idx;
}

uint32_t BVHBuilder::calculate_sah_split(uint32_t start, uint32_t end) {
    if (end - start <= 1) {
        return start;
    }

    AABB full_bounds;
    for (uint32_t i = start; i < end; ++i) {
        full_bounds.expand(triangles_[triangle_indices_[i]].get_bounds());
    }

    float best_cost = 1e10f;
    uint32_t best_split = start;

    // Try splits along longest axis
    glm::vec3 size = full_bounds.max - full_bounds.min;
    int axis = 0;
    if (size.y > size.x) axis = 1;
    if (size.z > size[axis]) axis = 2;

    // Try different split positions
    for (uint32_t i = start + 1; i < end; ++i) {
        AABB left_bounds, right_bounds;

        for (uint32_t j = start; j < i; ++j) {
            left_bounds.expand(triangles_[triangle_indices_[j]].get_bounds());
        }

        for (uint32_t j = i; j < end; ++j) {
            right_bounds.expand(triangles_[triangle_indices_[j]].get_bounds());
        }

        float cost = (i - start) * left_bounds.surface_area() +
                    (end - i) * right_bounds.surface_area();

        if (cost < best_cost) {
            best_cost = cost;
            best_split = i;
        }
    }

    return best_split;
}

BVHBuilder::BVHStats BVHBuilder::get_statistics() const {
    QMutexLocker lock(&mutex_);

    BVHStats stats;
    stats.node_count = nodes_.size();
    stats.triangle_count = triangles_.size();

    // Calculate depth statistics (simplified)
    stats.max_depth = static_cast<uint32_t>(std::log2(nodes_.size()) + 1);
    stats.average_depth = stats.max_depth / 2.0f;

    return stats;
}

// ============================================================================
// RayIntersectionEngine Implementation
// ============================================================================

RayIntersectionEngine::RayIntersectionEngine(uint32_t device_id) : device_id_(device_id) {}

RayIntersectionEngine::~RayIntersectionEngine() {
    // Cleanup GPU resources would happen here
}

bool RayIntersectionEngine::initialize(const BVHBuilder& bvh) {
    QMutexLocker lock(&mutex_);

    bvh_ = std::make_unique<BVHBuilder>(device_id_);
    bvh_->build();

    return true;
}

bool RayIntersectionEngine::upload_bvh_to_gpu(VkDevice device, const BVHBuilder& bvh) {
    // In a real implementation, would upload BVH and triangle data to GPU buffers
    return true;
}

bool RayIntersectionEngine::intersect(const Ray& ray, RayHit& hit) {
    QMutexLocker lock(&mutex_);

    if (!bvh_) {
        return false;
    }

    // Use GPU if available, otherwise fall back to CPU
    return cpu_intersect(ray, hit);
}

bool RayIntersectionEngine::intersect_batch(const std::vector<Ray>& rays,
                                           std::vector<RayHit>& hits) {
    QMutexLocker lock(&mutex_);

    hits.clear();
    hits.reserve(rays.size());

    for (const auto& ray : rays) {
        RayHit hit;
        if (cpu_intersect(ray, hit)) {
            stats_.hits++;
        } else {
            stats_.misses++;
        }
        hits.push_back(hit);
        stats_.total_intersections++;
    }

    return true;
}

RayIntersectionEngine::IntersectionStats RayIntersectionEngine::get_statistics() const {
    QMutexLocker lock(&mutex_);
    return stats_;
}

bool RayIntersectionEngine::cpu_intersect(const Ray& ray, RayHit& hit) {
    if (!bvh_) {
        return false;
    }

    hit.t = -1.0f;
    hit.hit = false;

    // Traverse BVH
    const auto& nodes = bvh_->get_nodes();
    const auto& triangles = bvh_->get_triangles();

    // Simple iterative BVH traversal
    std::vector<uint32_t> to_visit;
    to_visit.push_back(0);

    while (!to_visit.empty()) {
        uint32_t node_idx = to_visit.back();
        to_visit.pop_back();

        if (node_idx >= nodes.size()) {
            continue;
        }

        const BVHNode& node = nodes[node_idx];

        // Check AABB intersection
        glm::vec3 t_mins = (node.bounds.min - ray.origin) / (ray.direction + 1e-6f);
        glm::vec3 t_maxs = (node.bounds.max - ray.origin) / (ray.direction + 1e-6f);

        glm::vec3 t_min_vec = glm::min(t_mins, t_maxs);
        glm::vec3 t_max_vec = glm::max(t_mins, t_maxs);

        float t_min = glm::max(glm::max(glm::max(t_min_vec.x, t_min_vec.y), t_min_vec.z), ray.t_min);
        float t_max = glm::min(glm::min(glm::min(t_max_vec.x, t_max_vec.y), t_max_vec.z), ray.t_max);

        if (t_min > t_max) {
            continue;
        }

        if (node.is_leaf) {
            // Test all triangles in leaf
            for (uint32_t i = 0; i < node.primitive_count; ++i) {
                uint32_t tri_idx = node.first_primitive + i;
                if (tri_idx < triangles.size()) {
                    RayHit tri_hit;
                    if (triangle_ray_intersection(triangles[tri_idx], ray, tri_hit)) {
                        if (hit.t < 0 || tri_hit.t < hit.t) {
                            hit = tri_hit;
                        }
                    }
                }
            }
        } else {
            // Add children to visit list
            to_visit.push_back(node.right_child);
            to_visit.push_back(node.left_child);
        }
    }

    return hit.hit;
}

bool RayIntersectionEngine::triangle_ray_intersection(const Triangle& tri, const Ray& ray,
                                                     RayHit& hit) {
    // Möller-Trumbore algorithm
    const float EPSILON = 1e-6f;

    glm::vec3 edge1 = tri.v1 - tri.v0;
    glm::vec3 edge2 = tri.v2 - tri.v0;
    glm::vec3 h = glm::cross(ray.direction, edge2);
    float a = glm::dot(edge1, h);

    if (std::fabs(a) < EPSILON) {
        return false; // Ray is parallel to triangle
    }

    float f = 1.0f / a;
    glm::vec3 s = ray.origin - tri.v0;
    float u = f * glm::dot(s, h);

    if (u < 0.0f || u > 1.0f) {
        return false;
    }

    glm::vec3 q = glm::cross(s, edge1);
    float v = f * glm::dot(ray.direction, q);

    if (v < 0.0f || u + v > 1.0f) {
        return false;
    }

    float t = f * glm::dot(edge2, q);

    if (t >= ray.t_min && t <= ray.t_max) {
        hit.t = t;
        hit.position = ray.origin + ray.direction * t;
        hit.normal = glm::normalize(glm::cross(edge1, edge2));
        hit.hit = true;
        hit.geometry_id = tri.geometry_id;

        return true;
    }

    return false;
}

// ============================================================================
// AdvancedRayTracer Implementation
// ============================================================================

AdvancedRayTracer& AdvancedRayTracer::instance() {
    static AdvancedRayTracer tracer;
    return tracer;
}

bool AdvancedRayTracer::initialize(uint32_t device_id, VkDevice device) {
    QMutexLocker lock(&mutex_);

    device_id_ = device_id;
    device_ = device;

    bvh_builder_ = std::make_unique<BVHBuilder>(device_id);
    intersection_engine_ = std::make_unique<RayIntersectionEngine>(device_id);

    return true;
}

uint32_t AdvancedRayTracer::add_geometry(const std::vector<Triangle>& triangles) {
    QMutexLocker lock(&mutex_);

    if (!bvh_builder_) {
        return 0xFFFFFFFF;
    }

    geometries_.push_back(triangles);
    return bvh_builder_->add_geometry(triangles);
}

bool AdvancedRayTracer::build_acceleration_structures() {
    QMutexLocker lock(&mutex_);

    auto start_time = std::chrono::high_resolution_clock::now();

    if (!bvh_builder_) {
        return false;
    }

    if (!bvh_builder_->build()) {
        return false;
    }

    if (!intersection_engine_->initialize(*bvh_builder_)) {
        return false;
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    return true;
}

bool AdvancedRayTracer::cast_ray(const glm::vec3& origin, const glm::vec3& direction,
                               RayHit& hit) {
    QMutexLocker lock(&mutex_);

    if (!intersection_engine_) {
        return false;
    }

    Ray ray;
    ray.origin = origin;
    ray.direction = glm::normalize(direction);

    return intersection_engine_->intersect(ray, hit);
}

bool AdvancedRayTracer::cast_rays_screen_space(uint32_t width, uint32_t height,
                                              const glm::mat4& projection,
                                              const glm::mat4& view,
                                              std::vector<RayHit>& hits) {
    QMutexLocker lock(&mutex_);

    if (!intersection_engine_) {
        return false;
    }

    std::vector<Ray> rays;
    rays.reserve(width * height);

    glm::mat4 inv_proj = glm::inverse(projection);
    glm::mat4 inv_view = glm::inverse(view);

    // Generate rays for each pixel
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            float ndc_x = (2.0f * x) / width - 1.0f;
            float ndc_y = (2.0f * y) / height - 1.0f;

            glm::vec4 ray_ndc(ndc_x, ndc_y, 1.0f, 1.0f);
            glm::vec4 ray_eye = inv_proj * ray_ndc;
            ray_eye.z = 1.0f;
            ray_eye.w = 0.0f;

            glm::vec3 ray_world = glm::normalize(glm::vec3(inv_view * ray_eye));

            Ray ray;
            ray.origin = glm::vec3(inv_view[3]);
            ray.direction = ray_world;

            rays.push_back(ray);
        }
    }

    return intersection_engine_->intersect_batch(rays, hits);
}

bool AdvancedRayTracer::enable_hardware_rt(bool enable) {
    QMutexLocker lock(&mutex_);

    hardware_rt_enabled_ = enable;
    return true;
}

AdvancedRayTracer::RayTracerStats AdvancedRayTracer::get_statistics() const {
    QMutexLocker lock(&mutex_);

    RayTracerStats stats;

    if (intersection_engine_) {
        stats.intersection_stats = intersection_engine_->get_statistics();
    }

    if (bvh_builder_) {
        auto bvh_stats = bvh_builder_->get_statistics();
        stats.geometry_count = bvh_stats.triangle_count;
    } else {
        stats.geometry_count = 0;
    }

    stats.hardware_rt_enabled = hardware_rt_enabled_;

    return stats;
}

// ============================================================================
// RayTracingShaderManager Implementation
// ============================================================================

RayTracingShaderManager& RayTracingShaderManager::instance() {
    static RayTracingShaderManager manager;
    return manager;
}

bool RayTracingShaderManager::compile_ray_generation_shader(const std::string& source,
                                                           VkShaderModule& module) {
    // In a real implementation, would compile GLSL/HLSL to SPIR-V
    return true;
}

bool RayTracingShaderManager::compile_closest_hit_shader(const std::string& source,
                                                        VkShaderModule& module) {
    return true;
}

bool RayTracingShaderManager::compile_any_hit_shader(const std::string& source,
                                                    VkShaderModule& module) {
    return true;
}

bool RayTracingShaderManager::compile_miss_shader(const std::string& source,
                                                 VkShaderModule& module) {
    return true;
}

VkPipeline RayTracingShaderManager::create_ray_tracing_pipeline(
    VkDevice device,
    const std::vector<VkPipelineShaderStageCreateInfo>& stages,
    VkPipelineLayout layout) {
    // In a real implementation, would create actual ray tracing pipeline
    return VK_NULL_HANDLE;
}

bool RayTracingShaderManager::compile_glsl_to_spirv(const std::string& source,
                                                   VkShaderStageFlagBits stage,
                                                   std::vector<uint32_t>& spirv) {
    // In a real implementation, would use glslang or similar
    return true;
}

} // namespace GPU
} // namespace RawrXD
