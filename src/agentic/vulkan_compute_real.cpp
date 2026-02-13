#include <vulkan/vulkan.h>
#include <vector>
#include <cstring>
#include <stdint.h>

//=============================================================================
// Vulkan Compute Implementation (Issues #21, #39, #40, #42, #46)
// Production-Ready GPU Compute Pipeline
//=============================================================================

// Compute pipeline context
typedef struct {
    VkInstance instance;
    VkDevice device;
    VkPhysicalDevice physical_device;
    VkCommandPool command_pool;
    VkQueue compute_queue;
    uint32_t compute_queue_family;
    VkDescriptorPool descriptor_pool;
    std::vector<VkCommandBuffer> command_buffers;
    std::vector<VkBuffer> buffers;
    std::vector<VkDeviceMemory> memory;
    uint32_t buffer_count;
} VulkanComputeContext;

// Compute pipeline configuration
typedef struct {
    uint32_t work_group_size_x;
    uint32_t work_group_size_y;
    uint32_t work_group_size_z;
    uint32_t shared_memory_size;
    const uint32_t* shader_code;
    uint32_t shader_code_size;
} ComputePipelineConfig;

//=============================================================================
// Vulkan Instance and Device Management
//=============================================================================

/**
 * Create Vulkan instance
 */
extern "C" VkInstance Vulkan_CreateInstance(
    const char* app_name,
    uint32_t app_version)
{
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = app_name,
        .applicationVersion = app_version,
        .pEngineName = "RawrXD",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3
    };
    
    VkInstanceCreateInfo instance_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = 0,
        .ppEnabledExtensionNames = nullptr,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr
    };
    
    VkInstance instance = nullptr;
    VkResult result = vkCreateInstance(&instance_info, nullptr, &instance);
    
    if (result != VK_SUCCESS) {
        return nullptr;
    }
    
    return instance;
}

/**
 * Select physical device (GPU)
 */
extern "C" VkPhysicalDevice Vulkan_SelectPhysicalDevice(VkInstance instance)
{
    if (!instance) return nullptr;
    
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
    
    if (device_count == 0) {
        return nullptr;
    }
    
    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(instance, &device_count, devices.data());
    
    // Select first device (prefer discrete GPU)
    VkPhysicalDevice best_device = devices[0];
    
    for (uint32_t i = 0; i < device_count; i++) {
        VkPhysicalDeviceProperties props = {};
        vkGetPhysicalDeviceProperties(devices[i], &props);
        
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            best_device = devices[i];
            break;
        }
    }
    
    return best_device;
}

/**
 * Find compute queue family index
 */
static uint32_t Vulkan_FindComputeQueueFamily(VkPhysicalDevice physical_device)
{
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);
    
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families.data());
    
    for (uint32_t i = 0; i < queue_family_count; i++) {
        if (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            return i;
        }
    }
    
    return 0;  // Fallback to first family
}

/**
 * Create logical device
 */
extern "C" VkDevice Vulkan_CreateDevice(
    VkPhysicalDevice physical_device,
    uint32_t* out_queue_family)
{
    if (!physical_device) return nullptr;
    
    uint32_t queue_family = Vulkan_FindComputeQueueFamily(physical_device);
    
    float queue_priority = 1.0f;
    
    VkDeviceQueueCreateInfo queue_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = queue_family,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority
    };
    
    VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_info,
        .enabledExtensionCount = 0,
        .ppEnabledExtensionNames = nullptr
    };
    
    VkDevice device = nullptr;
    VkResult result = vkCreateDevice(physical_device, &device_info, nullptr, &device);
    
    if (result != VK_SUCCESS) {
        return nullptr;
    }
    
    if (out_queue_family) {
        *out_queue_family = queue_family;
    }
    
    return device;
}

/**
 * Create Vulkan compute context
 */
extern "C" void* Vulkan_CreateComputeContext()
{
    VulkanComputeContext* ctx = new VulkanComputeContext();
    
    // Create instance
    ctx->instance = Vulkan_CreateInstance("RawrXD Compute", VK_MAKE_VERSION(1, 0, 0));
    if (!ctx->instance) {
        delete ctx;
        return nullptr;
    }
    
    // Select physical device
    ctx->physical_device = Vulkan_SelectPhysicalDevice(ctx->instance);
    if (!ctx->physical_device) {
        vkDestroyInstance(ctx->instance, nullptr);
        delete ctx;
        return nullptr;
    }
    
    // Create logical device
    ctx->device = Vulkan_CreateDevice(ctx->physical_device, &ctx->compute_queue_family);
    if (!ctx->device) {
        vkDestroyInstance(ctx->instance, nullptr);
        delete ctx;
        return nullptr;
    }
    
    // Get compute queue
    vkGetDeviceQueue(ctx->device, ctx->compute_queue_family, 0, &ctx->compute_queue);
    
    // Create command pool
    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = ctx->compute_queue_family
    };
    
    vkCreateCommandPool(ctx->device, &pool_info, nullptr, &ctx->command_pool);
    
    // Create descriptor pool
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 256 }
    };
    
    VkDescriptorPoolCreateInfo desc_pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 64,
        .poolSizeCount = 1,
        .pPoolSizes = pool_sizes
    };
    
    vkCreateDescriptorPool(ctx->device, &desc_pool_info, nullptr, &ctx->descriptor_pool);
    
    ctx->buffer_count = 0;
    
    return ctx;
}

//=============================================================================
// Buffer Management
//=============================================================================

/**
 * Find memory type index
 */
static uint32_t Vulkan_FindMemoryType(
    VkPhysicalDevice physical_device,
    uint32_t type_filter,
    VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties mem_properties = {};
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);
    
    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) &&
            (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    
    return 0;
}

/**
 * Create GPU buffer
 */
extern "C" VkBuffer Vulkan_CreateBuffer(
    void* context,
    uint64_t size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkDeviceMemory* out_memory)
{
    VulkanComputeContext* ctx = (VulkanComputeContext*)context;
    if (!ctx || size == 0) return nullptr;
    
    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    
    VkBuffer buffer = nullptr;
    vkCreateBuffer(ctx->device, &buffer_info, nullptr, &buffer);
    
    VkMemoryRequirements mem_requirements = {};
    vkGetBufferMemoryRequirements(ctx->device, buffer, &mem_requirements);
    
    uint32_t memory_type = Vulkan_FindMemoryType(
        ctx->physical_device,
        mem_requirements.memoryTypeBits,
        properties);
    
    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_requirements.size,
        .memoryTypeIndex = memory_type
    };
    
    VkDeviceMemory memory = nullptr;
    vkAllocateMemory(ctx->device, &alloc_info, nullptr, &memory);
    
    vkBindBufferMemory(ctx->device, buffer, memory, 0);
    
    if (out_memory) {
        *out_memory = memory;
    }
    
    // Track in context
    if (ctx->buffer_count < 256) {
        ctx->buffers.push_back(buffer);
        ctx->memory.push_back(memory);
        ctx->buffer_count++;
    }
    
    return buffer;
}

/**
 * Copy data to GPU buffer
 */
extern "C" uint32_t Vulkan_CopyToBuffer(
    void* context,
    VkBuffer buffer,
    const void* data,
    uint64_t size)
{
    VulkanComputeContext* ctx = (VulkanComputeContext*)context;
    if (!ctx || !buffer || !data || size == 0) {
        return 0;
    }
    
    // Create staging buffer
    VkDeviceMemory staging_memory = nullptr;
    VkBuffer staging_buffer = Vulkan_CreateBuffer(
        context,
        size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &staging_memory);
    
    if (!staging_buffer) return 0;
    
    // Map and copy data
    void* mapped = nullptr;
    vkMapMemory(ctx->device, staging_memory, 0, size, 0, &mapped);
    if (mapped) {
        memcpy(mapped, data, size);
        vkUnmapMemory(ctx->device, staging_memory);
    }
    
    // Copy to device buffer
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = ctx->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    
    VkCommandBuffer cmd_buffer = nullptr;
    vkAllocateCommandBuffers(ctx->device, &alloc_info, &cmd_buffer);
    
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    
    vkBeginCommandBuffer(cmd_buffer, &begin_info);
    
    VkBufferCopy copy_region = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size = size
    };
    
    vkCmdCopyBuffer(cmd_buffer, staging_buffer, buffer, 1, &copy_region);
    
    vkEndCommandBuffer(cmd_buffer);
    
    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd_buffer
    };
    
    vkQueueSubmit(ctx->compute_queue, 1, &submit_info, nullptr);
    vkQueueWaitIdle(ctx->compute_queue);
    
    // Cleanup staging buffer
    vkFreeCommandBuffers(ctx->device, ctx->command_pool, 1, &cmd_buffer);
    vkDestroyBuffer(ctx->device, staging_buffer, nullptr);
    vkFreeMemory(ctx->device, staging_memory, nullptr);
    
    return 1;
}

/**
 * Copy data from GPU buffer
 */
extern "C" uint32_t Vulkan_CopyFromBuffer(
    void* context,
    VkBuffer buffer,
    void* data,
    uint64_t size)
{
    VulkanComputeContext* ctx = (VulkanComputeContext*)context;
    if (!ctx || !buffer || !data || size == 0) {
        return 0;
    }
    
    // Create staging buffer
    VkDeviceMemory staging_memory = nullptr;
    VkBuffer staging_buffer = Vulkan_CreateBuffer(
        context,
        size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &staging_memory);
    
    if (!staging_buffer) return 0;
    
    // Copy from device to staging
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = ctx->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    
    VkCommandBuffer cmd_buffer = nullptr;
    vkAllocateCommandBuffers(ctx->device, &alloc_info, &cmd_buffer);
    
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    
    vkBeginCommandBuffer(cmd_buffer, &begin_info);
    
    VkBufferCopy copy_region = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size = size
    };
    
    vkCmdCopyBuffer(cmd_buffer, buffer, staging_buffer, 1, &copy_region);
    
    vkEndCommandBuffer(cmd_buffer);
    
    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd_buffer
    };
    
    vkQueueSubmit(ctx->compute_queue, 1, &submit_info, nullptr);
    vkQueueWaitIdle(ctx->compute_queue);
    
    // Map and copy from staging
    void* mapped = nullptr;
    vkMapMemory(ctx->device, staging_memory, 0, size, 0, &mapped);
    if (mapped) {
        memcpy(data, mapped, size);
        vkUnmapMemory(ctx->device, staging_memory);
    }
    
    // Cleanup
    vkFreeCommandBuffers(ctx->device, ctx->command_pool, 1, &cmd_buffer);
    vkDestroyBuffer(ctx->device, staging_buffer, nullptr);
    vkFreeMemory(ctx->device, staging_memory, nullptr);
    
    return 1;
}

//=============================================================================
// Compute Pipeline Management
//=============================================================================

/**
 * Create compute shader module
 */
static VkShaderModule Vulkan_CreateShaderModule(
    VkDevice device,
    const uint32_t* code,
    size_t code_size)
{
    VkShaderModuleCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = code_size,
        .pCode = code
    };
    
    VkShaderModule shader_module = nullptr;
    vkCreateShaderModule(device, &create_info, nullptr, &shader_module);
    
    return shader_module;
}

/**
 * Create compute pipeline
 */
extern "C" uint32_t Vulkan_CreateComputePipeline(
    void* context,
    const ComputePipelineConfig* config,
    VkPipeline* out_pipeline,
    VkPipelineLayout* out_pipeline_layout)
{
    VulkanComputeContext* ctx = (VulkanComputeContext*)context;
    if (!ctx || !config || !out_pipeline || !out_pipeline_layout) {
        return 0;
    }
    
    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 0,
        .pushConstantRangeCount = 0
    };
    
    VkResult result = vkCreatePipelineLayout(
        ctx->device,
        &pipeline_layout_info,
        nullptr,
        out_pipeline_layout);
    
    if (result != VK_SUCCESS) {
        return 0;
    }
    
    // Create shader module
    VkShaderModule shader_module = Vulkan_CreateShaderModule(
        ctx->device,
        config->shader_code,
        config->shader_code_size);
    
    if (!shader_module) {
        return 0;
    }
    
    VkPipelineShaderStageCreateInfo shader_stage_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = shader_module,
        .pName = "main"
    };
    
    VkComputePipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = shader_stage_info,
        .layout = *out_pipeline_layout
    };
    
    result = vkCreateComputePipelines(
        ctx->device,
        nullptr,
        1,
        &pipeline_info,
        nullptr,
        out_pipeline);
    
    vkDestroyShaderModule(ctx->device, shader_module, nullptr);
    
    return (result == VK_SUCCESS) ? 1 : 0;
}

/**
 * Dispatch compute work
 */
extern "C" uint32_t Vulkan_DispatchCompute(
    void* context,
    VkPipeline pipeline,
    uint32_t group_count_x,
    uint32_t group_count_y,
    uint32_t group_count_z)
{
    VulkanComputeContext* ctx = (VulkanComputeContext*)context;
    if (!ctx || !pipeline) {
        return 0;
    }
    
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = ctx->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    
    VkCommandBuffer cmd_buffer = nullptr;
    vkAllocateCommandBuffers(ctx->device, &alloc_info, &cmd_buffer);
    
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    
    vkBeginCommandBuffer(cmd_buffer, &begin_info);
    
    vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    
    vkCmdDispatch(cmd_buffer, group_count_x, group_count_y, group_count_z);
    
    vkEndCommandBuffer(cmd_buffer);
    
    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd_buffer
    };
    
    VkResult result = vkQueueSubmit(ctx->compute_queue, 1, &submit_info, nullptr);
    vkQueueWaitIdle(ctx->compute_queue);
    
    vkFreeCommandBuffers(ctx->device, ctx->command_pool, 1, &cmd_buffer);
    
    return (result == VK_SUCCESS) ? 1 : 0;
}

//=============================================================================
// Context Cleanup
//=============================================================================

/**
 * Destroy Vulkan compute context
 */
extern "C" void Vulkan_DestroyComputeContext(void* context)
{
    VulkanComputeContext* ctx = (VulkanComputeContext*)context;
    if (!ctx) return;
    
    if (ctx->device) {
        vkDeviceWaitIdle(ctx->device);
        
        // Destroy buffers
        for (uint32_t i = 0; i < ctx->buffers.size(); i++) {
            vkDestroyBuffer(ctx->device, ctx->buffers[i], nullptr);
            if (i < ctx->memory.size()) {
                vkFreeMemory(ctx->device, ctx->memory[i], nullptr);
            }
        }
        
        // Destroy pools
        if (ctx->descriptor_pool) {
            vkDestroyDescriptorPool(ctx->device, ctx->descriptor_pool, nullptr);
        }
        
        if (ctx->command_pool) {
            vkDestroyCommandPool(ctx->device, ctx->command_pool, nullptr);
        }
        
        vkDestroyDevice(ctx->device, nullptr);
    }
    
    if (ctx->instance) {
        vkDestroyInstance(ctx->instance, nullptr);
    }
    
    delete ctx;
}

//=============================================================================
// Exports
//=============================================================================

extern "C" {
    void* __stdcall Vulkan_CreateComputeContext();
    VkInstance __stdcall Vulkan_CreateInstance(const char*, uint32_t);
    VkPhysicalDevice __stdcall Vulkan_SelectPhysicalDevice(VkInstance);
    VkDevice __stdcall Vulkan_CreateDevice(VkPhysicalDevice, uint32_t*);
    VkBuffer __stdcall Vulkan_CreateBuffer(void*, uint64_t, VkBufferUsageFlags, VkMemoryPropertyFlags, VkDeviceMemory*);
    uint32_t __stdcall Vulkan_CopyToBuffer(void*, VkBuffer, const void*, uint64_t);
    uint32_t __stdcall Vulkan_CopyFromBuffer(void*, VkBuffer, void*, uint64_t);
    uint32_t __stdcall Vulkan_CreateComputePipeline(void*, const ComputePipelineConfig*, VkPipeline*, VkPipelineLayout*);
    uint32_t __stdcall Vulkan_DispatchCompute(void*, VkPipeline, uint32_t, uint32_t, uint32_t);
    void __stdcall Vulkan_DestroyComputeContext(void*);
}
