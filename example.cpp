#include <cstdlib>
#include <cstdint>
#include <cmath>
#include <string>
#include <iostream>
#include <chrono>

#include "SFML/Graphics.hpp"

#define CHERRY_CHECK_BOUNDS
#include "cherry.hpp"
#undef CHERRY_CHECK_BOUNDS


auto CreateCanvasData(
        int64_t width,
        int64_t height) -> std::vector<uint8_t> {
    return std::vector<uint8_t>(4 * width * height);
}


auto CreateCanvas(
        std::vector<uint8_t> & canvas_data,
        int64_t width,
        int64_t height) -> cherry::Canvas {
    return cherry::Canvas(canvas_data.data(), width, height, width)
            .SetChannelLayout(cherry::LayoutRGBA)
            .SetBlendMode(cherry::PixelBlendMode::OVERWRITE)
            .Fill({ 255, 255, 255, 255 })
            .SetBlendMode(cherry::PixelBlendMode::ALPHA_COMPOSITING);
}


auto FunkyTree(
        cherry::Canvas & canvas,
        const cherry::Canvas & tree,
        double time_s) -> void {
    auto _ = canvas.WithBlendMode(cherry::PixelBlendMode::ALPHA_COMPOSITING);

    const auto right = canvas.Width() / 2;
    const auto bottom = canvas.Height() / 2;

    const auto left_offset = canvas.Width() / 3;
    const auto top_offset = canvas.Height() / 3;

    canvas.CopyInto(
            tree, {
                    .Left = static_cast<int64_t>(right + std::sin(time_s) * left_offset),
                    .Top = static_cast<int64_t>(bottom + std::sin(time_s) * top_offset),
                    .Right = right,
                    .Bottom = bottom,
            }
    );
}


auto Lines(cherry::Canvas & canvas) -> cherry::Canvas & {
    const auto x0 = canvas.Width() / 2;
    const auto y0 = canvas.Height() / 2;
    const auto length = std::min(canvas.Width() / 2, canvas.Height() / 2) * 3 / 4;

    auto rad = 0.0;
    while (rad < 2.0 * M_PI) {
        const auto x1 = x0 + static_cast<int64_t >(length * std::cos(rad));
        const auto y1 = y0 + static_cast<int64_t >(length * std::sin(rad));

        canvas.Line(
                { x0, y0 }, { x1, y1 },
                cherry::Color{ static_cast<uint32_t>(std::abs(x1)), static_cast<uint32_t>(std::abs(y1)) }.Clamp());

        rad += M_PI / 10;
    }


    return canvas;
}


auto Gradient(cherry::Canvas & canvas) -> cherry::Canvas & {
    auto _ = canvas.WithBlendMode(cherry::PixelBlendMode::ALPHA_COMPOSITING);

    for (auto y = 0; y < canvas.Height(); y += 1) {
        for (auto x = 0; x < canvas.Width(); x += 1) {
            const auto t = static_cast<uint8_t>((255.0 * (x + y)) / (canvas.Height() + canvas.Width()));
            canvas.BlendPixel({ x, y }, { .Blue = 128, .Green = 64, .Red = t, .Alpha = 192 });
        }
    }

    return canvas;
}


auto CheckeredBackground(cherry::Canvas & canvas) -> cherry::Canvas & {
    const auto white = cherry::Color{ 255, 255, 255 };
    const auto gray = cherry::Color{ 192, 192, 192 };

    auto _ = canvas.WithBlendMode(cherry::PixelBlendMode::OVERWRITE);

    for (auto y = 0; y < canvas.Height(); y += 1) {
        for (auto x = 0; x < canvas.Width(); x += 1) {
            canvas.BlendPixel({ x, y }, (x / 25 + y / 25) % 2 ? white : gray);
        }
    }

    return canvas;
}


auto Benchmark(double run_time_s) -> void {
    constexpr auto width = 640;
    constexpr auto height = 480;

    auto window = sf::RenderWindow({ width, height }, "Rendered Image");

    auto data1 = CreateCanvasData(width, height);
    auto background = CreateCanvas(data1, width, height);
    CheckeredBackground(background);
    Gradient(background);

    auto data2 = CreateCanvasData(width, height);
    auto final_canvas = CreateCanvas(data2, width, height).SetBlendMode(cherry::PixelBlendMode::ALPHA_COMPOSITING);

    auto tree = sf::Image();
    tree.loadFromFile(std::string("../sprites.bmp"));
    const auto[tree_width, tree_height] = tree.getSize();
    const auto tree_data_bytes_count = 4 * tree_width * tree_height;
    auto tree_data = std::vector<uint8_t>(tree.getPixelsPtr(), tree.getPixelsPtr() + tree_data_bytes_count);
    const auto sprite_canvas = cherry::Canvas(tree_data.data(), tree_width, tree_height, tree_width)
            .SetChannelLayout(cherry::LayoutRGBA);

    auto texture = sf::Texture();
    texture.create(width, height);

    auto sprite = sf::Sprite();
    sprite.setTexture(texture);

    const auto benchmark_start = std::chrono::steady_clock::now();
    auto frames_rendered = 0;

    auto elapsed_ms = [](
            decltype(std::chrono::steady_clock::now()) start
    ) -> double {
        const auto now = std::chrono::steady_clock::now();
        const auto millis =
                static_cast<double >(std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count());
        return millis;
    };

    auto background_draw_time_ms = 0.0;
    auto tree_draw_time_ms = 0.0;

    while (window.isOpen()) {
        auto event = sf::Event{};
        while (window.pollEvent(event)) {
            if (sf::Event::Closed == event.type) {
                window.close();
                return;
            }
        }

        if (elapsed_ms(benchmark_start) / 1000 > run_time_s) {
            window.close();
            break;
        }

        auto start = std::chrono::steady_clock::now();
        final_canvas.CopyInto(background, { 0, 0, width, height });
        background_draw_time_ms += elapsed_ms(start);
        start = std::chrono::steady_clock::now();
        FunkyTree(final_canvas, sprite_canvas, elapsed_ms(benchmark_start) / 1000.0);
        tree_draw_time_ms += elapsed_ms(start);
        texture.update(final_canvas.Data());

        window.draw(sprite);

        window.display();
        frames_rendered += 1;
    }
    const auto elapsed_ms_total = elapsed_ms(benchmark_start);

    std::cout << "FPS: "
              << frames_rendered / elapsed_ms_total * 1000
              << std::endl;
    std::cout << "Background: " << background_draw_time_ms / elapsed_ms_total * 100 << "%" << std::endl;
    std::cout << "Tree: " << tree_draw_time_ms / elapsed_ms_total * 100 << "%" << std::endl;
}


auto Main(const std::string & file_name) -> void {
    Benchmark(20);
}


auto main(
        int argc,
        char ** argv) -> int {
    using namespace std::string_literals;
    const auto file_name = "../"s + (argc > 1 ? argv[1] : "image.bmp");

    try {
        Main(file_name);
    }
    catch (const std::exception & e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

