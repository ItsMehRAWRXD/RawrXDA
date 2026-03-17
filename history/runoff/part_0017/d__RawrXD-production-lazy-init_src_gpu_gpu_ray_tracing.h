#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <cstdint>
#include <vulkan/vulkan.h>
#include <QMutex>

namespace RawrXD {
namespace GPU {

// Ray structure for ray tracing
struct Ray {
    glm::vec3 origin;
    float t_min = 0.001f;
    glm::vec3 direction;
    float t_max = 1e10f;
};

// Hit information
struct RayHit {
    float t = -1.0f;
    glm::vec3 position;
    glm::vec3 normal;
    uint32_t primitive_id = 0xFFFFFFFF;
    uint32_t geometry_id = 0xFFFFFFFF;
    bool hit = false;
};

// AABB for BVH
struct AABB {
    glm::vec3 min = glm::vec3(1e10f);
    glm::vec3 max = glm::vec3(-1e10f);

    AABB() = default;
    AABB(const glm::vec3& min_v, const glm::vec3& max_v) : min(min_v), max(max_v) {}

    void expand(const glm::vec3& point) {
        min = glm::min(min, point);
        max = glm::max(max, point);
    }

    void expand(const AABB& other) {
        min = glm::min(min, other.min);
        max = glm::max(max, other.max);
    }

    float surface_area() const {
        glm::vec3 size = max - min;
        return 2.0f * (size.x * size.y + size.y * size.z + size.z * size.x);
    }
};

// BVH Node
struct BVHNode {
    AABB bounds;
    uint32_t left_child = 0;
    uint32_t right_child = 0;
    uint32_t first_primitive = 0;
    uint32_t primitive_count = 0;
    bool is_leaf = false;
};

// Triangle for ray tracing
struct Triangle {
    glm::vec3 v0, v1, v2;
    uint32_t geometry_id = 0;

    AABB get_bounds() const {
        AABB bounds;
        bounds.expand(v0);
        bounds.expand(v1);
        bounds.expand(v2);
        return bounds;
    }
};

// BVH (Bounding Volume Hierarchy) Builder
class BVHBuilder {
public:
    explicit BVHBuilder(uint32_t device_id);
    ~BVHBuilder();

    // Add geometry (triangles)
    uint32_t add_geometry(const std::vector<Triangle>& triangles);

    // Build BVH structure
    bool build();

    // Get BVH nodes
    const std::vector<BVHNode>& get_nodes() const { return nodes_; }

    // Get triangles
    const std::vector<Triangle>& get_triangles() const { return triangles_; }

    // Get statistics
    struct BVHStats {
        uint32_t node_count = 0;
        uint32_t triangle_count = 0;
        uint32_t max_depth = 0;
        float average_depth = 0.0f;
    };

    BVHStats get_statistics() const;

private:
    uint32_t device_id_;
    std::vector<Triangle> triangles_;
    std::vector<BVHNode> nodes_;
    std::vector<uint32_t> triangle_indices_;

    mutable QMutex mutex_;

    // Build BVH recursively
    uint32_t build_recursive(uint32_t start, uint32_t end, uint32_t depth);

    // Calculate SAH (Surface Area Heuristic)
    uint32_t calculate_sah_split(uint32_t start, uint32_t end);
};

// Ray Intersection Engine - GPU-accelerated ray tracing
class RayIntersectionEngine {
public:
    explicit RayIntersectionEngine(uint32_t device_id);
    ~RayIntersectionEngine();

    // Initialize with BVH
    bool initialize(const BVHBuilder& bvh);

    // Upload BVH to GPU
    bool upload_bvh_to_gpu(VkDevice device, const BVHBuilder& bvh);

    // Perform ray intersection
    bool intersect(const Ray& ray, RayHit& hit);

    // Perform batch ray intersections
    bool intersect_batch(const std::vector<Ray>& rays, std::vector<RayHit>& hits);

    // Get intersection statistics
    struct IntersectionStats {
        uint64_t total_intersections = 0;
        uint64_t hits = 0;
        uint64_t misses = 0;
        float average_intersection_time_ms = 0.0f;
    };

    IntersectionStats get_statistics() const;

private:
    uint32_t device_id_;
    std::unique_ptr<BVHBuilder> bvh_;

    VkBuffer bvh_buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory bvh_memory_ = VK_NULL_HANDLE;
    VkBuffer triangles_buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory triangles_memory_ = VK_NULL_HANDLE;

    IntersectionStats stats_;

    mutable QMutex mutex_;

    // CPU-side ray intersection (when GPU is unavailable)
    bool cpu_intersect(const Ray& ray, RayHit& hit);

    // Triangle-ray intersection (Möller-Trumbore algorithm)
    bool triangle_ray_intersection(const Triangle& tri, const Ray& ray, RayHit& hit);
};

// Advanced Ray Tracer - high-level ray tracing interface
class AdvancedRayTracer {
public:
    static AdvancedRayTracer& instance();

    // Initialize ray tracer
    bool initialize(uint32_t device_id, VkDevice device);

    // Add scene geometry
    uint32_t add_geometry(const std::vector<Triangle>& triangles);

    // Build acceleration structures
    bool build_acceleration_structures();

    // Cast ray
    bool cast_ray(const glm::vec3& origin, const glm::vec3& direction, RayHit& hit);

    // Cast rays in parallel (screen space)
    bool cast_rays_screen_space(uint32_t width, uint32_t height,
                               const glm::mat4& projection,
                               const glm::mat4& view,
                               std::vector<RayHit>& hits);

    // Get intersection engine
    RayIntersectionEngine* get_intersection_engine() { return intersection_engine_.get(); }

    // Enable hardware ray tracing (RT cores)
    bool enable_hardware_rt(bool enable);

    // Get statistics
    struct RayTracerStats {
        RayIntersectionEngine::IntersectionStats intersection_stats;
        uint32_t geometry_count = 0;
        float build_time_ms = 0.0f;
        bool hardware_rt_enabled = false;
    };

    RayTracerStats get_statistics() const;

private:
    AdvancedRayTracer() = default;
    ~AdvancedRayTracer() = default;

    uint32_t device_id_;
    VkDevice device_ = VK_NULL_HANDLE;

    std::unique_ptr<BVHBuilder> bvh_builder_;
    std::unique_ptr<RayIntersectionEngine> intersection_engine_;

    bool hardware_rt_enabled_ = false;

    std::vector<std::vector<Triangle>> geometries_;

    mutable QMutex mutex_;
};

// Ray Tracing Shader Manager
class RayTracingShaderManager {
public:
    static RayTracingShaderManager& instance();

    // Compile ray generation shader
    bool compile_ray_generation_shader(const std::string& source,
                                      VkShaderModule& module);

    // Compile closest hit shader
    bool compile_closest_hit_shader(const std::string& source,
                                   VkShaderModule& module);

    // Compile any hit shader
    bool compile_any_hit_shader(const std::string& source,
                               VkShaderModule& module);

    // Compile miss shader
    bool compile_miss_shader(const std::string& source,
                            VkShaderModule& module);

    // Create ray tracing pipeline
    VkPipeline create_ray_tracing_pipeline(
        VkDevice device,
        const std::vector<VkPipelineShaderStageCreateInfo>& stages,
        VkPipelineLayout layout);

private:
    RayTracingShaderManager() = default;
    ~RayTracingShaderManager() = default;

    mutable QMutex mutex_;

    // Compile GLSL to SPIR-V
    bool compile_glsl_to_spirv(const std::string& source,
                              VkShaderStageFlagBits stage,
                              std::vector<uint32_t>& spirv);
};

} // namespace GPU
} // namespace RawrXD
