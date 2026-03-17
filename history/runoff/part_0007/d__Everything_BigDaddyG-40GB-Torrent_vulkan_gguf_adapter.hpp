// ════════════════════════════════════════════════════════════════════════════════
//  VULKAN GGUF METADATA ADAPTER - Production-Ready GPU Integration
//  Reads live-patched GGUF metadata and reconfigures compute shaders dynamically
//  Features: Zero-copy metadata, dynamic KV cache, hot-reload on patch
// ════════════════════════════════════════════════════════════════════════════════

#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>
#include <fstream>
#include <array>
#include <cstring>

// ═══════════════════════════════════════════════════════════════════════════════
// ERROR HANDLING MACRO
// ═══════════════════════════════════════════════════════════════════════════════
#define VK_CHECK(call) \
    do { \
        VkResult result = call; \
        if (result != VK_SUCCESS) { \
            throw std::runtime_error(std::string("Vulkan error at ") + __FILE__ + \
                ":" + std::to_string(__LINE__) + " - " + #call + \
                " returned " + std::to_string(result)); \
        } \
    } while(0)

// ═══════════════════════════════════════════════════════════════════════════════
// METADATA STRUCTURES - Mirror your GGUF hotpatch state
// ═══════════════════════════════════════════════════════════════════════════════
struct GGUFMetadata {
    uint32_t context_len;           // Patched context length
    uint32_t head_count;            // Attention heads
    uint32_t head_size;             // Dimension per head (typically 64 or 128)
    uint32_t rope_freq_base;        // RoPE base frequency (10k/500k/1M)
    uint32_t layer_count;           // Number of transformer layers
    uint32_t kv_size;               // KV cache size per token
    uint32_t hidden_size;           // Hidden dimension
    uint32_t vocab_size;            // Vocabulary size
    uint32_t _padding[56];          // Align to 256 bytes for cache-line efficiency
};

// ═══════════════════════════════════════════════════════════════════════════════
// MAIN VULKAN ADAPTER CLASS
// ═══════════════════════════════════════════════════════════════════════════════
class VulkanGGUFAdapter {
public:
    // ─────────────────────────────────────────────────────────────────────────
    // INITIALIZATION
    // ─────────────────────────────────────────────────────────────────────────
    
    void init(VkPhysicalDevice phys_dev, VkDevice dev, VkQueue queue, 
              uint32_t queue_family, VkCommandPool cmd_pool) {
        physical_device = phys_dev;
        device = dev;
        compute_queue = queue;
        compute_queue_family = queue_family;
        command_pool = cmd_pool;
        
        create_descriptor_set_layout();
        create_pipeline();
        create_descriptor_pool();
        allocate_descriptor_set();
        
        initialized = true;
    }
    
    // ─────────────────────────────────────────────────────────────────────────
    // METADATA BUFFER MANAGEMENT
    // ─────────────────────────────────────────────────────────────────────────
    
    // Bind host memory-mapped GGUF metadata to GPU
    void bind_metadata_buffer(const void* gguf_mapped_data, size_t size) {
        // Create buffer
        VkBufferCreateInfo buffer_info{};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = size;
        buffer_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | 
                           VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                           VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        
        VK_CHECK(vkCreateBuffer(device, &buffer_info, nullptr, &metadata_buffer));
        
        // Allocate memory
        VkMemoryRequirements mem_reqs;
        vkGetBufferMemoryRequirements(device, metadata_buffer, &mem_reqs);
        
        VkMemoryAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_reqs.size;
        alloc_info.memoryTypeIndex = find_memory_type(mem_reqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | 
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        
        VK_CHECK(vkAllocateMemory(device, &alloc_info, nullptr, &metadata_memory));
        VK_CHECK(vkBindBufferMemory(device, metadata_buffer, metadata_memory, 0));
        
        // Copy metadata from GGUF
        void* gpu_data;
        VK_CHECK(vkMapMemory(device, metadata_memory, 0, size, 0, &gpu_data));
        std::memcpy(gpu_data, gguf_mapped_data, size);
        vkUnmapMemory(device, metadata_memory);
        
        // Get device address
        VkBufferDeviceAddressInfo addr_info{};
        addr_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        addr_info.buffer = metadata_buffer;
        metadata_address = vkGetBufferDeviceAddress(device, &addr_info);
        
        update_descriptor_metadata();
    }
    
    // Hot-reload: Re-sync patched metadata to GPU
    void reload_metadata_after_patch(const void* updated_gguf_data, size_t size) {
        vkDeviceWaitIdle(device);
        
        // Re-map and copy new values
        void* gpu_data;
        VK_CHECK(vkMapMemory(device, metadata_memory, 0, size, 0, &gpu_data));
        std::memcpy(gpu_data, updated_gguf_data, size);
        vkUnmapMemory(device, metadata_memory);
        
        // GPU will read new values on next dispatch
    }
    
    // ─────────────────────────────────────────────────────────────────────────
    // KV CACHE MANAGEMENT
    // ─────────────────────────────────────────────────────────────────────────
    
    // Allocate/reallocate KV cache based on patched context length
    void allocate_kv_cache(uint32_t context_len, uint32_t kv_size_per_token,
                          uint32_t layer_count) {
        // Cleanup old cache if exists
        if (kv_cache_buffer) {
            vkDestroyBuffer(device, kv_cache_buffer, nullptr);
            vkFreeMemory(device, kv_cache_memory, nullptr);
            kv_cache_buffer = VK_NULL_HANDLE;
            kv_cache_memory = VK_NULL_HANDLE;
        }
        
        // Calculate total size: context_len * layers * kv_size_per_token
        uint64_t cache_size = static_cast<uint64_t>(context_len) * layer_count * 
                             kv_size_per_token;
        
        if (cache_size == 0) return;
        
        // Create buffer
        VkBufferCreateInfo buffer_info{};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = cache_size;
        buffer_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        
        VK_CHECK(vkCreateBuffer(device, &buffer_info, nullptr, &kv_cache_buffer));
        
        // Allocate GPU-local memory
        VkMemoryRequirements mem_reqs;
        vkGetBufferMemoryRequirements(device, kv_cache_buffer, &mem_reqs);
        
        VkMemoryAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_reqs.size;
        alloc_info.memoryTypeIndex = find_memory_type(mem_reqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        
        VK_CHECK(vkAllocateMemory(device, &alloc_info, nullptr, &kv_cache_memory));
        VK_CHECK(vkBindBufferMemory(device, kv_cache_buffer, kv_cache_memory, 0));
        
        update_descriptor_kv_cache();
    }
    
    // ─────────────────────────────────────────────────────────────────────────
    // OUTPUT IMAGE MANAGEMENT
    // ─────────────────────────────────────────────────────────────────────────
    
    // Create output image for attention matrix visualization
    void create_output_image(uint32_t width, uint32_t height) {
        // Cleanup old image if exists
        if (output_image) {
            vkDestroyImage(device, output_image, nullptr);
            vkFreeMemory(device, image_memory, nullptr);
            vkDestroyImageView(device, image_view, nullptr);
            output_image = VK_NULL_HANDLE;
            image_memory = VK_NULL_HANDLE;
            image_view = VK_NULL_HANDLE;
        }
        
        if (width == 0 || height == 0) return;
        
        // Create image
        VkImageCreateInfo image_info{};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        image_info.extent.width = width;
        image_info.extent.height = height;
        image_info.extent.depth = 1;
        image_info.mipLevels = 1;
        image_info.arrayLayers = 1;
        image_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        
        VK_CHECK(vkCreateImage(device, &image_info, nullptr, &output_image));
        
        // Allocate memory
        VkMemoryRequirements mem_reqs;
        vkGetImageMemoryRequirements(device, output_image, &mem_reqs);
        
        VkMemoryAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_reqs.size;
        alloc_info.memoryTypeIndex = find_memory_type(mem_reqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        
        VK_CHECK(vkAllocateMemory(device, &alloc_info, nullptr, &image_memory));
        VK_CHECK(vkBindImageMemory(device, output_image, image_memory, 0));
        
        // Create image view
        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = output_image;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;
        
        VK_CHECK(vkCreateImageView(device, &view_info, nullptr, &image_view));
        
        update_descriptor_output_image();
    }
    
    // ─────────────────────────────────────────────────────────────────────────
    // COMPUTE DISPATCH
    // ─────────────────────────────────────────────────────────────────────────
    
    // Execute compute shader with dynamic dispatch based on patched metadata
    void dispatch(uint32_t context_len, uint32_t head_count) {
        if (!compute_pipeline || context_len == 0) return;
        
        // Allocate command buffer
        VkCommandBufferAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandPool = command_pool;
        alloc_info.commandBufferCount = 1;
        
        VkCommandBuffer cmd_buffer;
        VK_CHECK(vkAllocateCommandBuffers(device, &alloc_info, &cmd_buffer));
        
        // Begin recording
        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        VK_CHECK(vkBeginCommandBuffer(cmd_buffer, &begin_info));
        
        // Bind pipeline and descriptors
        vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline);
        vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
            pipeline_layout, 0, 1, &descriptor_set, 0, nullptr);
        
        // Dispatch with dynamic thread count
        // Local size = 32, so (context_len + 31) / 32 groups
        uint32_t group_count_x = (context_len + 31) / 32;
        uint32_t group_count_y = head_count;
        
        vkCmdDispatch(cmd_buffer, group_count_x, group_count_y, 1);
        
        // End recording
        VK_CHECK(vkEndCommandBuffer(cmd_buffer));
        
        // Submit and wait
        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &cmd_buffer;
        
        VK_CHECK(vkQueueSubmit(compute_queue, 1, &submit_info, VK_NULL_HANDLE));
        VK_CHECK(vkQueueWaitIdle(compute_queue));
        
        vkFreeCommandBuffers(device, command_pool, 1, &cmd_buffer);
    }
    
    // ─────────────────────────────────────────────────────────────────────────
    // CLEANUP
    // ─────────────────────────────────────────────────────────────────────────
    
    ~VulkanGGUFAdapter() {
        if (!initialized) return;
        
        vkDeviceWaitIdle(device);
        
        if (output_image) vkDestroyImage(device, output_image, nullptr);
        if (image_memory) vkFreeMemory(device, image_memory, nullptr);
        if (image_view) vkDestroyImageView(device, image_view, nullptr);
        
        if (metadata_buffer) vkDestroyBuffer(device, metadata_buffer, nullptr);
        if (metadata_memory) vkFreeMemory(device, metadata_memory, nullptr);
        
        if (kv_cache_buffer) vkDestroyBuffer(device, kv_cache_buffer, nullptr);
        if (kv_cache_memory) vkFreeMemory(device, kv_cache_memory, nullptr);
        
        if (compute_pipeline) vkDestroyPipeline(device, compute_pipeline, nullptr);
        if (pipeline_layout) vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
        if (descriptor_set_layout) vkDestroyDescriptorSetLayout(device, descriptor_set_layout, nullptr);
        if (descriptor_pool) vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
    }
    
private:
    // ─────────────────────────────────────────────────────────────────────────
    // VULKAN STATE
    // ─────────────────────────────────────────────────────────────────────────
    
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue compute_queue = VK_NULL_HANDLE;
    uint32_t compute_queue_family = 0;
    VkCommandPool command_pool = VK_NULL_HANDLE;
    
    VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkPipeline compute_pipeline = VK_NULL_HANDLE;
    
    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
    
    // Metadata buffer (GGUF hotpatch data)
    VkBuffer metadata_buffer = VK_NULL_HANDLE;
    VkDeviceMemory metadata_memory = VK_NULL_HANDLE;
    VkDeviceAddress metadata_address = 0;
    
    // KV cache buffer
    VkBuffer kv_cache_buffer = VK_NULL_HANDLE;
    VkDeviceMemory kv_cache_memory = VK_NULL_HANDLE;
    
    // Output image (attention matrix)
    VkImage output_image = VK_NULL_HANDLE;
    VkDeviceMemory image_memory = VK_NULL_HANDLE;
    VkImageView image_view = VK_NULL_HANDLE;
    
    bool initialized = false;
    
    // ─────────────────────────────────────────────────────────────────────────
    // INTERNAL HELPERS
    // ─────────────────────────────────────────────────────────────────────────
    
    void create_descriptor_set_layout() {
        std::array<VkDescriptorSetLayoutBinding, 3> bindings{};
        
        // Binding 0: GGUF metadata (read-only storage buffer)
        bindings[0].binding = 0;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        
        // Binding 1: KV cache (storage buffer)
        bindings[1].binding = 1;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        
        // Binding 2: Output image (storage image)
        bindings[2].binding = 2;
        bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        bindings[2].descriptorCount = 1;
        bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        
        VkDescriptorSetLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
        layout_info.pBindings = bindings.data();
        
        VK_CHECK(vkCreateDescriptorSetLayout(device, &layout_info, nullptr, 
                                            &descriptor_set_layout));
    }
    
    void create_pipeline() {
        // Load pre-compiled SPIR-V shader
        std::vector<uint32_t> spv_code = load_shader_spv("gguf_metadata_adapter.spv");
        
        VkShaderModuleCreateInfo shader_info{};
        shader_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shader_info.codeSize = spv_code.size() * sizeof(uint32_t);
        shader_info.pCode = spv_code.data();
        
        VkShaderModule shader_module;
        VK_CHECK(vkCreateShaderModule(device, &shader_info, nullptr, &shader_module));
        
        VkPipelineShaderStageCreateInfo stage_info{};
        stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stage_info.module = shader_module;
        stage_info.pName = "main";
        
        VkPipelineLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.setLayoutCount = 1;
        layout_info.pSetLayouts = &descriptor_set_layout;
        
        VK_CHECK(vkCreatePipelineLayout(device, &layout_info, nullptr, 
                                       &pipeline_layout));
        
        VkComputePipelineCreateInfo pipeline_info{};
        pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipeline_info.stage = stage_info;
        pipeline_info.layout = pipeline_layout;
        
        VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, 
                                         &pipeline_info, nullptr, &compute_pipeline));
        
        vkDestroyShaderModule(device, shader_module, nullptr);
    }
    
    void create_descriptor_pool() {
        std::array<VkDescriptorPoolSize, 2> pool_sizes{};
        pool_sizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        pool_sizes[0].descriptorCount = 2;  // metadata + kv_cache
        pool_sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        pool_sizes[1].descriptorCount = 1;  // output_attention
        
        VkDescriptorPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.maxSets = 1;
        pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
        pool_info.pPoolSizes = pool_sizes.data();
        
        VK_CHECK(vkCreateDescriptorPool(device, &pool_info, nullptr, 
                                       &descriptor_pool));
    }
    
    void allocate_descriptor_set() {
        VkDescriptorSetAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = descriptor_pool;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pSetLayouts = &descriptor_set_layout;
        
        VK_CHECK(vkAllocateDescriptorSets(device, &alloc_info, &descriptor_set));
    }
    
    void update_descriptor_metadata() {
        if (!metadata_buffer) return;
        
        VkDescriptorBufferInfo buffer_info{};
        buffer_info.buffer = metadata_buffer;
        buffer_info.offset = 0;
        buffer_info.range = VK_WHOLE_SIZE;
        
        VkWriteDescriptorSet descriptor_write{};
        descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_write.dstSet = descriptor_set;
        descriptor_write.dstBinding = 0;
        descriptor_write.descriptorCount = 1;
        descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptor_write.pBufferInfo = &buffer_info;
        
        vkUpdateDescriptorSets(device, 1, &descriptor_write, 0, nullptr);
    }
    
    void update_descriptor_kv_cache() {
        if (!kv_cache_buffer) return;
        
        VkDescriptorBufferInfo buffer_info{};
        buffer_info.buffer = kv_cache_buffer;
        buffer_info.offset = 0;
        buffer_info.range = VK_WHOLE_SIZE;
        
        VkWriteDescriptorSet descriptor_write{};
        descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_write.dstSet = descriptor_set;
        descriptor_write.dstBinding = 1;
        descriptor_write.descriptorCount = 1;
        descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptor_write.pBufferInfo = &buffer_info;
        
        vkUpdateDescriptorSets(device, 1, &descriptor_write, 0, nullptr);
    }
    
    void update_descriptor_output_image() {
        if (!image_view) return;
        
        VkDescriptorImageInfo image_info{};
        image_info.imageView = image_view;
        image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        
        VkWriteDescriptorSet descriptor_write{};
        descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_write.dstSet = descriptor_set;
        descriptor_write.dstBinding = 2;
        descriptor_write.descriptorCount = 1;
        descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        descriptor_write.pImageInfo = &image_info;
        
        vkUpdateDescriptorSets(device, 1, &descriptor_write, 0, nullptr);
    }
    
    uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties mem_properties;
        vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);
        
        for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
            if ((type_filter & (1 << i)) && 
                (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }
        
        throw std::runtime_error("Failed to find suitable memory type");
    }
    
    std::vector<uint32_t> load_shader_spv(const char* filename) {
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            throw std::runtime_error(std::string("Failed to load shader: ") + filename);
        }
        
        size_t file_size = file.tellg();
        std::vector<uint32_t> code(file_size / sizeof(uint32_t));
        
        file.seekg(0);
        file.read(reinterpret_cast<char*>(code.data()), file_size);
        file.close();
        
        return code;
    }
};
