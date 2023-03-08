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


auto FunkyTree(
        cherry::Canvas & canvas,
        const cherry::Canvas & tree,
        double time_s) -> void {
    const auto left = canvas.Width / 2;
    const auto top = canvas.Height / 2;

    const auto left_offset = canvas.Width;
    const auto top_offset = canvas.Height;

    cherry::transform::Copy<cherry::color::FastAlphaBlend>(
            tree,
            canvas,
            left,
            top,
            std::lround(left + std::cos(time_s) * left_offset),
            std::lround(top + std::sin(time_s) * top_offset)
    );
}


auto Gradient(cherry::Canvas & canvas) -> void {
    for (auto y = decltype(canvas.Height){ 0 }; y < canvas.Height; y += 1) {
        for (auto x = decltype(canvas.Width){ 0 }; x < canvas.Width; x += 1) {
            const auto t = static_cast<uint8_t>((255.0 * (x + y)) / (canvas.Width + canvas.Height));
            canvas.BlendPixel<cherry::color::FastAlphaBlend>(x, y, cherry::color::FromRGBA(t, 128 - t / 2, 192, 192));
        }
    }
}


auto CheckeredBackground(cherry::Canvas & canvas) -> void {
    const auto white = cherry::color::FromRGBA(255, 255, 255, 128);
    const auto gray = cherry::color::FromRGBA(192, 192, 192, 128);

    for (auto y = decltype(canvas.Height){ 0 }; y < canvas.Height; y += 1) {
        for (auto x = decltype(canvas.Width){ 0 }; x < canvas.Width; x += 1) {
            canvas.BlendPixel<cherry::color::Overwrite>(x, y, (x / 25 + y / 25) % 2 ? white : gray);
        }
    }
}


template<typename TDuration, typename TResult = TDuration>
auto Elapsed(
        decltype(std::chrono::steady_clock::now()) since) -> TResult {
    const auto now = std::chrono::steady_clock::now();
    return static_cast<TResult>(std::chrono::duration_cast<TDuration>(now - since).count());
}


[[maybe_unused]]
auto TreeBenchmark(std::chrono::seconds benchmark_duration) -> void {
    constexpr auto width = 640;
    constexpr auto height = 480;

    auto window = sf::RenderWindow({ width, height }, "Funky Tree Benchmark");

    auto background_data = cherry::utility::PixelBuffer(width, height, cherry::color::FromRGBA(0, 0, 0, 255));
    auto background = cherry::Canvas(background_data.data(), width, height);
    CheckeredBackground(background);
    Gradient(background);

    auto canvas_data = cherry::utility::PixelBuffer(width, height);
    auto canvas = cherry::Canvas(canvas_data.data(), width, height);

    auto img = sf::Image();
    img.loadFromFile(std::string("../blue_tree.bmp"));
    auto blue_tree_data = cherry::utility::PixelBuffer(
            img.getPixelsPtr(),
            static_cast<int>(img.getSize().x),
            static_cast<int>(img.getSize().y)
    );
    const auto blue_tree = cherry::Canvas(
            reinterpret_cast<uint32_t *>(blue_tree_data.data()),
            static_cast<int>(img.getSize().x),
            static_cast<int>(img.getSize().y)
    );

    img.loadFromFile(std::string("../red_tree.bmp"));
    auto red_tree_data = cherry::utility::PixelBuffer(
            img.getPixelsPtr(),
            static_cast<int>(img.getSize().x),
            static_cast<int>(img.getSize().y)
    );
    const auto red_tree = cherry::Canvas(
            reinterpret_cast<uint32_t *>(red_tree_data.data()),
            static_cast<int>(img.getSize().x),
            static_cast<int>(img.getSize().y)
    );


    auto texture = sf::Texture();
    texture.create(canvas.Width, canvas.Height);

    auto sprite = sf::Sprite();
    sprite.setTexture(texture);

    const auto benchmark_start = std::chrono::steady_clock::now();
    auto frames_rendered = 0;

    while (window.isOpen()) {
        auto event = sf::Event{};
        while (window.pollEvent(event)) {
            if (sf::Event::Closed == event.type) {
                window.close();
                return;
            }
        }

        if (Elapsed<decltype(benchmark_duration)>(benchmark_start) > benchmark_duration) {
            window.close();
            break;
        }

        cherry::transform::Copy<cherry::color::Overwrite>(
                background,
                canvas,
                0,
                0,
                canvas.Width,
                canvas.Height
        );

        const auto t = Elapsed<std::chrono::milliseconds, double>(benchmark_start) / 1000.0;
        cherry::transform::Copy<cherry::color::FastAlphaBlend>(
                blue_tree,
                canvas,
                blue_tree.Width / 2,
                blue_tree.Height * 7 / 8,
                canvas.Width / 2,
                canvas.Height / 2,
                t
        );
        FunkyTree(canvas, red_tree, t * 2);

        const auto origin_x = canvas.Width / 2;
        const auto origin_y = canvas.Height / 2;
        constexpr auto r = 70;
        constexpr auto Pi = 3.14159265358979323846;

        cherry::drawing::Polygon<cherry::color::Overwrite>(
                canvas,
                {
                        { origin_x + r * std::cos(t + 0 * Pi / 2), origin_y - r * std::sin(t + 0 * Pi / 2) },
                        { origin_x + r * std::cos(t + 1 * Pi / 2), origin_y - r * std::sin(t + 1 * Pi / 2) },
                        { origin_x + r * std::cos(t + 2 * Pi / 2), origin_y - r * std::sin(t + 2 * Pi / 2) },
                        { origin_x + r * std::cos(t + 3 * Pi / 2), origin_y - r * std::sin(t + 3 * Pi / 2) },
                },
                cherry::color::FromRGBA(0, 0, 0)
        );

        cherry::drawing::Polygon<cherry::color::Overwrite>(
                canvas,
                {
                        {
                                origin_x + 2 * r * std::cos(2 * t + 0 * Pi / 2),
                                origin_y + 2 * r * std::sin(2 * t + 0 * Pi / 2)
                        },
                        {
                                origin_x + 2 * r * std::cos(2 * t + 1 * Pi / 2),
                                origin_y + 2 * r * std::sin(2 * t + 1 * Pi / 2)
                        },
                        {
                                origin_x + 2 * r * std::cos(2 * t + 2 * Pi / 2),
                                origin_y + 2 * r * std::sin(2 * t + 2 * Pi / 2)
                        },
                        {
                                origin_x + 2 * r * std::cos(2 * t + 3 * Pi / 2),
                                origin_y + 2 * r * std::sin(2 * t + 3 * Pi / 2)
                        },
                },
                cherry::color::FromRGBA(0, 0, 0)
        );

        cherry::drawing::FillTriangle<cherry::color::FastAlphaBlend>(
                canvas,
                50, 50,
                150 + std::lround(400 * std::sin(t * 2)), 180 + std::lround(400 * std::cos(t * 2)),
                400, 30,
                cherry::color::FromRGBA(192, 164, 255, 128)
        );

        texture.update(canvas.DataUint8);

        window.draw(sprite);

        window.display();
        frames_rendered += 1;
    }
    const auto elapsed_ms_total = Elapsed<std::chrono::milliseconds, double>(benchmark_start);

    std::cout << "FPS: "
              << frames_rendered / elapsed_ms_total * 1000
              << std::endl;
}


auto Main([[maybe_unused]] const std::string & file_name) -> void {
    TreeBenchmark(std::chrono::seconds{ 60 });
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

