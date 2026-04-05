#include <vulkan/vulkan.h>
#include <vector>
#include <string>

struct GlyphInstance {
    float x, y;
    float u, v;
};

std::vector<GlyphInstance> instances;
void* mapped;

vkMapMemory(device, bufferMemory, 0, VK_WHOLE_SIZE, 0, &mapped);

void PushToken(const char* ptr, size_t len) {
    for (size_t i = 0; i < len; i++) {
        GlyphInstance g;
        g.x = cursorX;
        g.y = cursorY;
        g.u = atlas[ptr[i]].u;
        g.v = atlas[ptr[i]].v;

        ((GlyphInstance*)mapped)[instanceCount++] = g;
        cursorX += advance;
    }
}