#include <cstdlib>
#include <cmath>
#include <string>
#include <iostream>
#include <chrono>

#include "SFML/Graphics.hpp"

#ifdef DEBUG
#define CHERRY_CHECK_BOUNDS
#endif
#include "cherry.hpp"


auto CreateCanvasData(
        int64_t width,
        int64_t height) -> std::vector<uint32_t> {
    return std::vector<uint32_t>(width * height);
}


auto CreateCanvas(
        std::vector<uint32_t> & canvas_data,
        int64_t width,
        int64_t height) -> cherry::Canvas {
    return cherry::Canvas(canvas_data.data(), width, height, width)
            .SetBlendMode(cherry::PixelBlendMode::OVERWRITE)
            .Fill(0xFFFFFFFF)
            .SetBlendMode(cherry::PixelBlendMode::ALPHA_COMPOSITING);
}


auto FunkyTree(
        cherry::Canvas & canvas,
        const cherry::Canvas & tree,
        double time_s) -> void {
    auto _ = canvas.WithBlendMode(cherry::PixelBlendMode::ALPHA_COMPOSITING);

    const auto right = canvas.Width / 2;
    const auto bottom = canvas.Height / 2;

    const auto left_offset = canvas.Width / 2;
    const auto top_offset = canvas.Height / 2;

    canvas.Blend(
            tree,
            static_cast<int64_t>(right + std::sin(time_s) * left_offset),
            static_cast<int64_t>(bottom + std::sin(time_s) * top_offset),
            right,
            bottom
    );
}


auto Gradient(cherry::Canvas & canvas) -> cherry::Canvas & {
    auto _ = canvas.WithBlendMode(cherry::PixelBlendMode::ALPHA_COMPOSITING);

    for (auto y = 0; y < canvas.Height; y += 1) {
        for (auto x = 0; x < canvas.Width; x += 1) {
            const auto t = static_cast<uint8_t>((255.0 * (x + y)) / (canvas.Width + canvas.Height));
            canvas.BlendPixel(x, y, cherry::CombineRGBA(t, 128 - t / 2, 192, 192));
        }
    }

    return canvas;
}


auto CheckeredBackground(cherry::Canvas & canvas) -> cherry::Canvas & {
    const auto white = cherry::CombineRGBA(255, 255, 255, 255);
    const auto gray = cherry::CombineRGBA(192, 192, 192, 255);

    auto _ = canvas.WithBlendMode(cherry::PixelBlendMode::OVERWRITE);

    for (auto y = 0; y < canvas.Height; y += 1) {
        for (auto x = 0; x < canvas.Width; x += 1) {
            canvas.BlendPixel(x, y, (x / 25 + y / 25) % 2 ? white : gray);
        }
    }

    return canvas;
}


template<typename TDuration, typename TResult = TDuration>
auto Elapsed(
        decltype(std::chrono::steady_clock::now()) since) -> TResult {
    const auto now = std::chrono::steady_clock::now();
    return static_cast<TResult>(std::chrono::duration_cast<TDuration>(now - since).count());
}


auto RunBenchmark(std::chrono::seconds benchmark_duration) -> void {
    constexpr auto width = 640;
    constexpr auto height = 480;

    auto window = sf::RenderWindow({ width, height }, "Bencho");

    auto data1 = CreateCanvasData(width, height);
    auto background = CreateCanvas(data1, width, height);
    CheckeredBackground(background);
    Gradient(background);

    auto data2 = CreateCanvasData(width, height);
    auto final_canvas = CreateCanvas(data2, width, height)
            .SetBlendMode(cherry::PixelBlendMode::ALPHA_COMPOSITING);

    auto tree = sf::Image();
    tree.loadFromFile(std::string("../sprites.bmp"));
    const auto[tree_width, tree_height] = tree.getSize();
    const auto tree_data_bytes_count = 4 * tree_width * tree_height;
    auto tree_data = std::vector<uint8_t>(tree.getPixelsPtr(), tree.getPixelsPtr() + tree_data_bytes_count);
    const auto sprite_canvas = cherry::Canvas(
            tree_data.data(),
            tree_width,
            tree_height,
            tree_width
    );

    auto texture = sf::Texture();
    texture.create(final_canvas.Width, final_canvas.Height);

    auto sprite = sf::Sprite();
    sprite.setTexture(texture);

    const auto benchmark_start = std::chrono::steady_clock::now();
    auto frames_rendered = 0;

    auto background_draw_time = 0.0;
    auto tree_draw_time = 0.0;

    while (window.isOpen()) {
        auto event = sf::Event{};
        while (window.pollEvent(event)) {
            if (sf::Event::Closed == event.type) {
                window.close();
                return;
            }
        }

        if (Elapsed<std::chrono::seconds>(benchmark_start) > benchmark_duration) {
            window.close();
            break;
        }

        auto start = std::chrono::steady_clock::now();
        final_canvas
                .SetBlendMode(cherry::PixelBlendMode::OVERWRITE)
                .Blend(background, 0, 0, width, height);
        background_draw_time += Elapsed<std::chrono::milliseconds, double>(start);

        start = std::chrono::steady_clock::now();
        const auto t =
                Elapsed<std::chrono::milliseconds, double>(benchmark_start) / 1000.0;
        FunkyTree(final_canvas, sprite_canvas, t);
        tree_draw_time += Elapsed<std::chrono::milliseconds, double>(start);
        texture.update(final_canvas.DataUint8);

        window.draw(sprite);

        window.display();
        frames_rendered += 1;
    }
    const auto elapsed_ms_total = Elapsed<std::chrono::milliseconds, double>(benchmark_start);

    std::cout << "FPS: "
              << frames_rendered / elapsed_ms_total * 1000
              << std::endl;
    std::cout << "Background: " << background_draw_time / elapsed_ms_total * 100 << "%" << std::endl;
    std::cout << "Tree: " << tree_draw_time / elapsed_ms_total * 100 << "%" << std::endl;
}


auto Main(const std::string & file_name) -> void {
    RunBenchmark(std::chrono::seconds{ 60 });
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

