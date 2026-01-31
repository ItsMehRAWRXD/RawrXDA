#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#endif

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "image_generator/stb_image_write.h"

#include "image_generator/image_generator.h"
#include "image_generator/gradients.h"
#include <iostream>

#ifdef RadialGradient
#undef RadialGradient
#endif
#ifdef LinearGradient
#undef LinearGradient
#endif
#ifdef ig
#undef ig
#endif

using namespace ig;

int main() {


    // Create canvas
    const int WIDTH = 1024;
    const int HEIGHT = 768;
    Canvas canvas(WIDTH, HEIGHT);
    canvas.clear(Color::rgb(0.08f, 0.09f, 0.11f));

    // Background gradient
    
    auto bg = create_layer(WIDTH, HEIGHT);
    LinearGradient grad(0, 0, WIDTH, HEIGHT);
    grad.add_stop(0.0f, Color::rgb(0.10f, 0.15f, 0.35f));
    grad.add_stop(0.5f, Color::rgb(0.08f, 0.12f, 0.25f));
    grad.add_stop(1.0f, Color::rgb(0.04f, 0.08f, 0.18f));

    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            bg.set(x, y, grad.sample(static_cast<float>(x), static_cast<float>(y)));
        }
    }
    canvas.composite(bg);

    // Perlin noise fog
    
    auto fog = create_layer(WIDTH, HEIGHT);
    Perlin2D perlin(42);

    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            float n = perlin.noise(x * 0.006f, y * 0.006f);
            float alpha = std::pow(n, 2.0f) * 0.25f;
            fog.blend(x, y, Color::rgba(0.9f, 0.95f, 1.0f, alpha));
        }
    }
    canvas.composite(fog);

    // Draw shapes
    
    fill_rect(canvas, 60, HEIGHT - 160, 320, 100, Color::rgba(0.9f, 0.5f, 0.2f, 0.8f));
    fill_circle(canvas, 860.0f, 160.0f, 90.0f, Color::rgba(0.2f, 0.7f, 0.9f, 0.85f));
    line_aa(canvas, 80.0f, 80.0f, 920.0f, 540.0f, Color::rgba(1.0f, 0.95f, 0.3f, 0.9f));

    std::vector<std::pair<float, float>> poly = {
        {500, 300}, {620, 260}, {740, 330}, {700, 460}, {560, 420}
    };
    fill_polygon(canvas, poly, Color::rgba(0.6f, 0.2f, 0.7f, 0.75f));

    // Radial highlight
    
    RadialGradient radial(860.0f, 160.0f, 140.0f);
    radial.add_stop(0.0f, Color::rgba(1.0f, 1.0f, 1.0f, 0.35f));
    radial.add_stop(1.0f, Color::rgba(1.0f, 1.0f, 1.0f, 0.0f));

    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            canvas.blend(x, y, radial.sample(static_cast<float>(x), static_cast<float>(y)));
        }
    }

    // Save as BMP
    
    const std::string bmp_out = "output.bmp";
    if (!write_bmp(canvas, bmp_out)) {
        
        return 1;
    }


    // Save as PNG (requires stb_image_write)
    
    const std::string png_out = "output.png";
    if (!write_png(canvas, png_out)) {
        
        return 1;
    }


    // Create a gradient showcase
    
    Canvas gradient_demo(512, 512);
    gradient_demo.clear(Color::white());

    // Linear gradients
    LinearGradient lin1(0, 0, 256, 256);
    lin1.add_stop(0.0f, Color::red());
    lin1.add_stop(0.5f, Color::rgb(1.0f, 1.0f, 0.0f));
    lin1.add_stop(1.0f, Color::green());

    for (int y = 0; y < 256; ++y) {
        for (int x = 0; x < 256; ++x) {
            gradient_demo.set(x, y, lin1.sample(static_cast<float>(x), static_cast<float>(y)));
        }
    }

    // Radial gradients
    RadialGradient rad1(384.0f, 128.0f, 100.0f);
    rad1.add_stop(0.0f, Color::white());
    rad1.add_stop(0.5f, Color::blue());
    rad1.add_stop(1.0f, Color::black());

    for (int y = 0; y < 256; ++y) {
        for (int x = 256; x < 512; ++x) {
            gradient_demo.set(x, y, rad1.sample(static_cast<float>(x), static_cast<float>(y)));
        }
    }

    if (write_bmp(gradient_demo, "gradients.bmp")) {
        
    }
    if (write_png(gradient_demo, "gradients.png")) {
        
    }


    return 0;
}
