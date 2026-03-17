#include <vulkan/vulkan.h>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "logging/logger.h"
static Logger s_logger("harness_vram_budget");

struct Options {
    std::string mode = "near-budget"; // load|near-budget|oversubscribe
    double target_frac = 0.9;          // fraction of available budget to consume
    std::size_t chunk_mb = 256;        // allocation chunk size
    std::uint32_t sample_ms = 500;     // telemetry sampling interval
    bool disable_budget = false;       // simulate missing VK_EXT_memory_budget
    bool json = true;                  // log as JSON lines
};

struct BudgetSample {
    std::uint64_t total = 0;
    std::uint64_t budget = 0;
    std::uint64_t usage = 0;
    std::uint64_t available = 0;
    bool budget_supported = false;
};

static void log_json(const std::string &event, const BudgetSample &s, const std::string &note = "") {
    s_logger.info("{");
    s_logger.info("\");
    s_logger.info("\");
    s_logger.info("\");
    s_logger.info("\");
    s_logger.info("\");
    if (!note.empty()) {
        s_logger.info(",\");
    }
    s_logger.info("}");
}

class VulkanHarness {
public:
    bool init(bool disable_budget);
    void cleanup();
    BudgetSample sample_budget(bool force_fallback) const;
    std::optional<std::size_t> allocate_chunk(std::size_t bytes);
    std::size_t tracked_allocation() const { return tracked_bytes_; }

private:
    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue queue_ = VK_NULL_HANDLE;
    std::uint32_t queue_family_ = 0;
    bool budget_supported_ = false;
    VkPhysicalDeviceMemoryProperties mem_props_{};
    std::vector<const char *> enabled_extensions_;
    std::vector<std::pair<VkBuffer, VkDeviceMemory>> allocations_;
    std::size_t tracked_bytes_ = 0;

    std::optional<std::uint32_t> find_memory_type(std::uint32_t type_bits, VkMemoryPropertyFlags flags) const;
};

bool VulkanHarness::init(bool disable_budget) {
    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "VRAMHarness";
    app.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo = &app;
    if (vkCreateInstance(&ci, nullptr, &instance_) != VK_SUCCESS) {
        s_logger.error( "failed to create Vulkan instance" << std::endl;
        return false;
    }

    uint32_t dev_count = 0;
    vkEnumeratePhysicalDevices(instance_, &dev_count, nullptr);
    if (dev_count == 0) {
        s_logger.error( "no Vulkan devices found" << std::endl;
        return false;
    }
    std::vector<VkPhysicalDevice> devices(dev_count);
    vkEnumeratePhysicalDevices(instance_, &dev_count, devices.data());

    // Pick first discrete GPU or fallback to first.
    for (auto d : devices) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(d, &props);
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            physical_device_ = d;
            break;
        }
    }
    if (physical_device_ == VK_NULL_HANDLE) {
        physical_device_ = devices[0];
    }

    vkGetPhysicalDeviceMemoryProperties(physical_device_, &mem_props_);

    // Check extension support
    uint32_t ext_count = 0;
    vkEnumerateDeviceExtensionProperties(physical_device_, nullptr, &ext_count, nullptr);
    std::vector<VkExtensionProperties> exts(ext_count);
    vkEnumerateDeviceExtensionProperties(physical_device_, nullptr, &ext_count, exts.data());
    for (const auto &ext : exts) {
        if (std::strcmp(ext.extensionName, VK_EXT_MEMORY_BUDGET_EXTENSION_NAME) == 0) {
            budget_supported_ = true;
        }
    }
    if (disable_budget) {
        budget_supported_ = false;
    }
    if (budget_supported_) {
        enabled_extensions_.push_back(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
    }

    // Find a queue family with compute (or any)
    uint32_t q_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &q_count, nullptr);
    std::vector<VkQueueFamilyProperties> queues(q_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &q_count, queues.data());
    queue_family_ = 0;
    for (uint32_t i = 0; i < q_count; ++i) {
        if (queues[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            queue_family_ = i;
            break;
        }
    }

    float priority = 1.0f;
    VkDeviceQueueCreateInfo qci{};
    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = queue_family_;
    qci.queueCount = 1;
    qci.pQueuePriorities = &priority;

    VkDeviceCreateInfo dci{};
    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = static_cast<uint32_t>(enabled_extensions_.size());
    dci.ppEnabledExtensionNames = enabled_extensions_.empty() ? nullptr : enabled_extensions_.data();

    if (vkCreateDevice(physical_device_, &dci, nullptr, &device_) != VK_SUCCESS) {
        s_logger.error( "failed to create logical device" << std::endl;
        return false;
    }

    vkGetDeviceQueue(device_, queue_family_, 0, &queue_);
    return true;
}

void VulkanHarness::cleanup() {
    for (auto &p : allocations_) {
        if (p.second != VK_NULL_HANDLE) {
            vkFreeMemory(device_, p.second, nullptr);
        }
        if (p.first != VK_NULL_HANDLE) {
            vkDestroyBuffer(device_, p.first, nullptr);
        }
    }
    allocations_.clear();
    tracked_bytes_ = 0;

    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }
    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }
}

std::optional<std::uint32_t> VulkanHarness::find_memory_type(std::uint32_t type_bits, VkMemoryPropertyFlags flags) const {
    for (uint32_t i = 0; i < mem_props_.memoryTypeCount; ++i) {
        if ((type_bits & (1u << i)) && (mem_props_.memoryTypes[i].propertyFlags & flags) == flags) {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> VulkanHarness::allocate_chunk(std::size_t bytes) {
    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = bytes;
    bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buf = VK_NULL_HANDLE;
    if (vkCreateBuffer(device_, &bci, nullptr, &buf) != VK_SUCCESS) {
        return std::nullopt;
    }

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(device_, buf, &req);
    auto mem_type = find_memory_type(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (!mem_type) {
        vkDestroyBuffer(device_, buf, nullptr);
        return std::nullopt;
    }

    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = *mem_type;

    VkDeviceMemory mem = VK_NULL_HANDLE;
    if (vkAllocateMemory(device_, &mai, nullptr, &mem) != VK_SUCCESS) {
        vkDestroyBuffer(device_, buf, nullptr);
        return std::nullopt;
    }
    vkBindBufferMemory(device_, buf, mem, 0);

    allocations_.push_back({buf, mem});
    tracked_bytes_ += req.size;
    return static_cast<std::size_t>(req.size);
}

BudgetSample VulkanHarness::sample_budget(bool force_fallback) const {
    BudgetSample sample{};
    sample.budget_supported = budget_supported_ && !force_fallback;

    // Total device-local heap size
    for (uint32_t i = 0; i < mem_props_.memoryHeapCount; ++i) {
        if (mem_props_.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            sample.total += mem_props_.memoryHeaps[i].size;
        }
    }

    if (sample.budget_supported) {
        VkPhysicalDeviceMemoryBudgetPropertiesEXT budget{};
        budget.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;

        VkPhysicalDeviceMemoryProperties2 props2{};
        props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
        props2.pNext = &budget;

        auto fp = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties2>(
            vkGetInstanceProcAddr(instance_, "vkGetPhysicalDeviceMemoryProperties2"));
        if (fp) {
            fp(physical_device_, &props2);
            for (uint32_t i = 0; i < props2.memoryProperties.memoryHeapCount; ++i) {
                if (props2.memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                    sample.budget += budget.heapBudget[i];
                    sample.usage += budget.heapUsage[i];
                }
            }
        }
    }

    if (!sample.budget_supported || sample.budget == 0) {
        // Fallback: use total minus tracked allocations
        sample.budget = sample.total;
        sample.usage = tracked_bytes_;
    }

    if (sample.budget > sample.usage) {
        sample.available = sample.budget - sample.usage;
    } else {
        sample.available = 0;
    }

    return sample;
}

static Options parse(int argc, char **argv) {
    Options opt;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next_val = [&](double &out) {
            if (i + 1 < argc) {
                out = std::stod(argv[++i]);
            }
        };
        auto next_u = [&](std::size_t &out) {
            if (i + 1 < argc) {
                out = static_cast<std::size_t>(std::stoull(argv[++i]));
            }
        };
        if (a == "--mode" && i + 1 < argc) {
            opt.mode = argv[++i];
        } else if (a == "--target-vram-frac" && i + 1 < argc) {
            next_val(opt.target_frac);
        } else if (a == "--chunk-mb" && i + 1 < argc) {
            next_u(opt.chunk_mb);
        } else if (a == "--sample-ms" && i + 1 < argc) {
            std::size_t tmp = 0; next_u(tmp); opt.sample_ms = static_cast<std::uint32_t>(tmp);
        } else if (a == "--no-budget") {
            opt.disable_budget = true;
        } else if (a == "--text") {
            opt.json = false;
        }
    }
    return opt;
}

static void log_sample(const Options &opt, const std::string &event, const BudgetSample &s, const std::string &note = "") {
    if (opt.json) {
        log_json(event, s, note);
    } else {
        s_logger.info( event << ": total=" << (s.total / 1.0e9) << "GB budget=" << (s.budget / 1.0e9)
                  << "GB usage=" << (s.usage / 1.0e9) << "GB avail=" << (s.available / 1.0e9)
                  << (note.empty() ? "" : (" note=" + note)) << std::endl;
    }
}

int main(int argc, char **argv) {
    Options opt = parse(argc, argv);
    VulkanHarness h;
    if (!h.init(opt.disable_budget)) {
        return 1;
    }

    auto first = h.sample_budget(false);
    log_sample(opt, "init", first, opt.disable_budget ? "budget disabled via flag" : "");

    const double target = opt.target_frac;
    const std::size_t chunk_bytes = opt.chunk_mb * 1024ull * 1024ull;

    bool oversubscribe = (opt.mode == "oversubscribe");
    bool near_budget = (opt.mode == "near-budget");
    bool load = (opt.mode == "load");

    bool done = false;
    auto last_sample_time = std::chrono::steady_clock::now();

    while (!done) {
        auto sample = h.sample_budget(false);
        bool do_sample_log = (std::chrono::steady_clock::now() - last_sample_time) >= std::chrono::milliseconds(opt.sample_ms);

        bool should_alloc = false;
        if (oversubscribe) {
            should_alloc = true; // keep going until allocation fails
        } else if (near_budget) {
            double frac = sample.budget > 0 ? static_cast<double>(sample.usage) / static_cast<double>(sample.budget) : 0.0;
            should_alloc = frac < target;
        } else if (load) {
            should_alloc = sample.available > chunk_bytes; // simple load mode
        }

        if (should_alloc) {
            auto res = h.allocate_chunk(chunk_bytes);
            if (!res.has_value()) {
                log_sample(opt, "alloc_fail", h.sample_budget(false), "allocation_failed");
                if (!oversubscribe) {
                    done = true;
                } else {
                    // oversubscribe keeps trying until no progress
                    done = true;
                }
            } else {
                std::ostringstream note;
                note << "allocated_bytes=" << *res;
                log_sample(opt, "alloc", h.sample_budget(false), note.str());
            }
        } else {
            done = true;
        }

        if (do_sample_log) {
            log_sample(opt, "sample", sample);
            last_sample_time = std::chrono::steady_clock::now();
        }

        // Terminate if no allocations are pending
        if (!should_alloc && do_sample_log) {
            break;
        }
    }

    log_sample(opt, "final", h.sample_budget(false));
    h.cleanup();
    return 0;
}
