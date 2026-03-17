// Native image load for Paint (Qt-free). Uses stb_image.
#define STB_IMAGE_IMPLEMENTATION
#include "../../examples/stb_image.h"
#include "../../include/image_generator/image_generator.h"
#include <cstring>

namespace ig
{

Canvas load_image(const std::string& path)
{
    int w = 0, h = 0, n = 0;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &n, 4);
    if (!data || w <= 0 || h <= 0)
    {
        if (data)
            stbi_image_free(data);
        Canvas empty(1, 1);
        empty.clear(Color::white());
        return empty;
    }
    Canvas out(w, h);
    const size_t len = static_cast<size_t>(w * h * 4);
    out.data.assign(data, data + len);
    stbi_image_free(data);
    return out;
}

}  // namespace ig
