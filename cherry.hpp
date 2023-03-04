#pragma once

#ifdef CHERRY_CHECK_BOUNDS
#include <stdexcept>
#endif
#include <limits>
#include <tuple>
#include <algorithm>
#include <cstdint>


namespace cherry {
#ifdef CHERRY_CLAMP
    template<typename T>
    [[nodiscard]]
    inline auto ClampTo(int64_t value) -> T {
        constexpr auto min = static_cast<decltype(value)>(std::numeric_limits<T>::min());
        constexpr auto max = static_cast<decltype(value)>(std::numeric_limits<T>::max());

        return static_cast<T>(std::clamp(value, min, max));
    }
#endif // CHERRY_CLAMP


#ifdef CHERRY_CLAMP_LINE
    inline auto ClampLine(
            int64_t x0,
            int64_t y0,
            int64_t x1,
            int64_t y1,
            int64_t width,
            int64_t height) -> std::tuple<decltype(x0), decltype(y0), decltype(x1), decltype(y1)> {

    }
#endif


    template<typename T>
    [[nodiscard]]
    inline auto IsWithinRange(int64_t value) -> bool {
        constexpr auto min = static_cast<decltype(value)>(std::numeric_limits<T>::min());
        constexpr auto max = static_cast<decltype(value)>(std::numeric_limits<T>::max());

        return value >= min and value <= max;
    }


    [[nodiscard]]
    inline auto Sort(
            int64_t x0,
            int64_t y0,
            int64_t x1,
            int64_t y1) -> std::tuple<decltype(x0), decltype(y0), decltype(x1), decltype(y1)> {
        return { std::min(x0, x1), std::min(y0, y1), std::max(x0, x1), std::max(y0, y1) };
    }


    constexpr auto INDEX_RED = 0u;
    constexpr auto INDEX_GREEN = 1u;
    constexpr auto INDEX_BLUE = 2u;
    constexpr auto INDEX_ALPHA = 3u;


    constexpr auto SHIFT_RED = 8u * INDEX_RED;
    constexpr auto SHIFT_GREEN = 8u * INDEX_GREEN;
    constexpr auto SHIFT_BLUE = 8u * INDEX_BLUE;
    constexpr auto SHIFT_ALPHA = 8u * INDEX_ALPHA;


    constexpr auto MASK_RED_BLUE = (0xFF << SHIFT_RED) | (0xFF << SHIFT_BLUE);
    constexpr auto MASK_GREEN = (0xFF << SHIFT_GREEN);
    constexpr auto MASK_ALPHA = (0xFF << SHIFT_ALPHA);


    [[nodiscard]]
    constexpr inline auto CombineRGBA(
            uint32_t red,
            uint32_t green,
            uint32_t blue,
            uint32_t alpha = 0xFF) -> uint32_t {
        return
                ((red & 0xFF) << SHIFT_RED)
                | ((green & 0xFF) << SHIFT_GREEN)
                | ((blue & 0xFF) << SHIFT_BLUE)
                | ((alpha & 0xFF) << SHIFT_ALPHA);
    }


    [[nodiscard]]
    constexpr inline auto DeconstructRGBA(
            uint32_t pixel) -> std::tuple<uint8_t, uint8_t, uint8_t, uint8_t> {
        return {
                (pixel >> SHIFT_RED) & 0xFF,
                (pixel >> SHIFT_GREEN) & 0xFF,
                (pixel >> SHIFT_BLUE) & 0xFF,
                (pixel >> SHIFT_ALPHA) & 0xFF,
        };
    }


    [[nodiscard]]
    inline auto AlphaBlend(
            uint32_t foreground,
            uint32_t background) -> uint32_t {
        const auto[fg_r, fg_g, fg_b, fg_a] = DeconstructRGBA(foreground);
        const auto[bg_r, bg_g, bg_b, bg_a] = DeconstructRGBA(background);

        const auto a = fg_a + bg_a * (255 - fg_a) / 255;
        const auto r = (fg_r * fg_a + bg_r * bg_a * (255 - fg_a) / 255) / a;
        const auto g = (fg_g * fg_a + bg_g * bg_a * (255 - fg_a) / 255) / a;
        const auto b = (fg_b * fg_a + bg_b * bg_a * (255 - fg_a) / 255) / a;

        return CombineRGBA(r, g, b, a);
    }


    [[nodiscard]]
    inline auto FastAlphaBlend(
            uint64_t foreground,
            uint64_t background) -> uint32_t {
        const auto fg_a = ((foreground & MASK_ALPHA) >> SHIFT_ALPHA);
        const auto alpha = fg_a + 1;
        const auto inv_alpha = 256 - fg_a;

        const auto rb =
                (alpha * (foreground & MASK_RED_BLUE) + inv_alpha * (background & MASK_RED_BLUE)) >> 8;
        const auto g =
                (alpha * (foreground & MASK_GREEN) + inv_alpha * (background & MASK_GREEN)) >> 8;

        return (rb & MASK_RED_BLUE) | (g & MASK_GREEN) | (~0 & MASK_ALPHA);
    }


    enum class PixelBlendMode {
        OVERWRITE,
        ALPHA_COMPOSITING,
        FAST_ALPHA_COMPOSITING
    };


    class Canvas {
        PixelBlendMode blend_mode{ PixelBlendMode::OVERWRITE };

        void (* BlendPixelFn)(
                Canvas &,
                int64_t,
                int64_t,
                uint32_t){ nullptr };

    public:
        uint32_t * const Data{ nullptr };
        uint8_t * const DataUint8{ nullptr };
        const uint32_t Width{ 0u };
        const uint32_t Height{ 0u };
        const uint32_t Stride{ 0u };
        const bool Empty{ false };


        Canvas(
                uint32_t * data,
                int64_t width,
                int64_t height,
                int64_t stride,
                PixelBlendMode mode = PixelBlendMode::OVERWRITE)
                :
                Data(data),
                DataUint8(reinterpret_cast<uint8_t *>(data)),
                Width(width),
                Height(height),
                Stride(stride),
                Empty(not width or not height) {
#ifdef CHERRY_CHECK_BOUNDS
            if (not IsWithinRange<decltype(Width)>(width)) {
                throw std::out_of_range("Invalid height: " + std::to_string(width));
            }
            if (not IsWithinRange<decltype(Height)>(height)) {
                throw std::out_of_range("Invalid height: " + std::to_string(height));
            }
            if (not IsWithinRange<decltype(Stride)>(stride)) {
                throw std::out_of_range("Invalid stride: " + std::to_string(stride));
            }
#endif

            SetBlendMode(mode);
        }


        Canvas(
                uint32_t * data,
                int64_t width,
                int64_t height)
                :
                Canvas(data, width, height, width) {}


        [[nodiscard]]
        [[maybe_unused]]
        inline auto BlendMode() const -> PixelBlendMode {
            return blend_mode;
        }


        inline auto SetBlendMode(PixelBlendMode mode) -> Canvas & {
            blend_mode = mode;

            switch (mode) {
                case PixelBlendMode::OVERWRITE:
                    BlendPixelFn = BlendOverwrite;
                    break;
                case PixelBlendMode::ALPHA_COMPOSITING:
                    BlendPixelFn = BlendAlpha;
                    break;
                case PixelBlendMode::FAST_ALPHA_COMPOSITING:
                    BlendPixelFn = BlendFastAlpha;
                    break;
            }

            return *this;
        }


        [[nodiscard]]
        inline auto SubCanvas(
                int64_t x0,
                int64_t y0,
                int64_t x1,
                int64_t y1) const -> Canvas {
            std::tie(x0, y0, x1, y1) = Sort(x0, y0, x1, y1);

            CheckBounds(x0, y0);
            CheckBounds(x1 - 1, y1 - 1);

            return Canvas{
                    Data + Stride * y0 + x0,
                    x1 - x0,
                    y1 - y0,
                    Stride,
                    blend_mode
            };
        }


        inline auto BlendPixel(
                int64_t x,
                int64_t y,
                uint32_t pixel) -> Canvas & {
            CheckBounds(x, y);

            BlendPixelFn(*this, x, y, pixel);

            return *this;
        }


        inline auto OverwritePixel(
                int64_t x,
                int64_t y,
                uint32_t pixel) -> Canvas & {
            CheckBounds(x, y);

            Data[Stride * y + x] = pixel;

            return *this;
        }


        [[nodiscard]]
        inline auto Pixel(
                int64_t x,
                int64_t y) const -> uint32_t {
            CheckBounds(x, y);

            return Data[Stride * y + x];
        }


        inline auto Fill(uint32_t color) -> Canvas & {
            for (auto y = 0; y < Height; y += 1) {
                for (auto x = 0; x < Width; x += 1) {
                    BlendPixel(x, y, color);
                }
            }

            return *this;
        }


        inline auto Copy(
                const Canvas & src,
                int64_t left,
                int64_t top,
                int64_t right,
                int64_t bottom) -> Canvas & {
            if (src.Empty) {
                return *this;
            }

            const auto target_width = std::abs(left - right);
            const auto target_height = std::abs(top - bottom);

            const auto mirrored_x = left > right;
            const auto mirrored_y = top > bottom;

            auto src_x0 = int64_t{ 0 };
            auto src_y0 = int64_t{ 0 };
            auto src_x1 = int64_t{ src.Width };
            auto src_y1 = int64_t{ src.Height };

            auto target = SubCanvas(
                    std::clamp<decltype(left)>(left, 0, Width),
                    std::clamp<decltype(top)>(top, 0, Height),
                    std::clamp<decltype(right)>(right, 0, Width),
                    std::clamp<decltype(bottom)>(bottom, 0, Height)
            );

            std::tie(left, top, right, bottom) = Sort(left, top, right, bottom);

            if (left < 0) {
                src_x0 = -left * src.Width / target_width;
            }
            if (top < 0) {
                src_y0 = -top * src.Height / target_height;
            }
            if (right >= Width) {
                src_x1 = (Width - left) * src.Width / target_width;
            }
            if (bottom >= Height) {
                src_y1 = (Height - top) * src.Height / target_height;
            }

            if (mirrored_x) {
                src_x0 = src.Width - src_x0;
                src_x1 = src.Width - src_x1;
            }

            if (mirrored_y) {
                src_y0 = src.Height - src_y0;
                src_y1 = src.Height - src_y1;
            }

            auto src2 = src.SubCanvas(src_x0, src_y0, src_x1, src_y1);

            if (target.Empty) {
                return *this;
            }

            for (auto y = 0; y < target.Height; y += 1) {
                auto src_y = y * src2.Height / target.Height;
                if (mirrored_y) {
                    src_y = src2.Height - 1 - src_y;
                }

                for (auto x = 0; x < target.Width; x += 1) {
                    auto src_x = x * src2.Width / target.Width;
                    if (mirrored_x) {
                        src_x = src2.Width - 1 - src_x;
                    }

                    target.BlendPixel(x, y, src2.Pixel(src_x, src_y));
                }
            }

            return *this;
        }


    private:
        [[nodiscard]]
        inline auto IsWithinBounds(
                int64_t x,
                int64_t y) const -> bool {
            return x >= 0 and y >= 0 and x < Width and y < Height;
        }


        inline auto CheckBounds(
                int64_t x,
                int64_t y) const -> void {
#ifdef CHERRY_CHECK_BOUNDS
            if (IsWithinBounds(x, y)) {
                return;
            }

            const auto message =
                    "Coordinates ("
                    + std::to_string(x) + ", " + std::to_string(y) +
                    ") are out of bounds for image size ("
                    + std::to_string(Width) + ", " + std::to_string(Height) + ")";
            throw std::out_of_range(message);
#endif // CHERRY_CHECK_BOUNDS
        }


        static inline auto BlendOverwrite(
                Canvas & canvas,
                int64_t x,
                int64_t y,
                uint32_t color) -> void {
            canvas.OverwritePixel(x, y, color);
        }


        static inline auto BlendAlpha(
                Canvas & canvas,
                int64_t x,
                int64_t y,
                uint32_t color) -> void {
            canvas.OverwritePixel(x, y, AlphaBlend(color, canvas.Pixel(x, y)));
        }


        static inline auto BlendFastAlpha(
                Canvas & canvas,
                int64_t x,
                int64_t y,
                uint32_t color) -> void {
            canvas.OverwritePixel(x, y, FastAlphaBlend(color, canvas.Pixel(x, y)));
        }
    };


    namespace drawing {
        inline auto LineLow(
                Canvas & canvas,
                int64_t x0,
                int64_t y0,
                int64_t x1,
                int64_t y1,
                uint32_t color) -> void {
            const auto dx = x1 - x0;
            const auto[dy, yi] = ((y1 - y0) >= 0) ? std::tuple(y1 - y0, 1) : std::tuple(y0 - y1, -1);

            auto D = 2 * dy - dx;
            auto y = y0;

            for (auto x = x0; x <= x1; x += 1) {
                canvas.BlendPixel(x, y, color);

                if (D > 0) {
                    y += yi;
                    D += 2 * (dy - dx);
                }
                else {
                    D += 2 * dy;
                }
            }
        }


        inline auto LineHigh(
                Canvas & canvas,
                int64_t x0,
                int64_t y0,
                int64_t x1,
                int64_t y1,
                uint32_t color) -> void {
            const auto[dx, xi] = ((x1 - x0) >= 0) ? std::tuple(x1 - x0, 1) : std::tuple(x0 - x1, -1);
            const auto dy = y1 - y0;

            auto D = 2 * dx - dy;
            auto x = x0;

            for (auto y = y0; y <= y1; y += 1) {
                canvas.BlendPixel(x, y, color);

                if (D > 0) {
                    x += xi;
                    D += 2 * (dx - dy);
                }
                else {
                    D += 2 * dx;
                }
            }
        }


        [[maybe_unused]]
        inline auto Line(
                Canvas & canvas,
                int64_t x0,
                int64_t y0,
                int64_t x1,
                int64_t y1,
                uint32_t color) -> Canvas & {
            if (std::abs(y1 - y0) < std::abs(x1 - x0)) {
                if (x0 > x1) {
                    LineLow(canvas, x1, y1, x0, y0, color);
                }
                else {
                    LineLow(canvas, x0, y0, x1, y1, color);
                }
            }
            else {
                if (y0 > y1) {
                    LineHigh(canvas, x1, y1, x0, y0, color);
                }
                else {
                    LineHigh(canvas, x0, y0, x1, y1, color);
                }
            }


            return canvas;
        }


        [[maybe_unused]]
        inline auto FillRectangle(
                Canvas & canvas,
                int64_t left,
                int64_t top,
                int64_t right,
                int64_t bottom,
                uint32_t color) -> Canvas & {
            canvas.SubCanvas(left, top, right, bottom).Fill(color);

            return canvas;
        }


        [[maybe_unused]]
        inline auto Shape(
                Canvas & canvas,
                const std::vector<std::tuple<int64_t, int64_t>> & vertices,
                uint32_t color) -> Canvas & {
            if (vertices.empty()) {
                return canvas;
            }

            for (auto i = 0; i < vertices.size(); i += 1) {
                const auto[x0, y0] = vertices[i % vertices.size()];
                const auto[x1, y1] = vertices[(i + 1) % vertices.size()];

                Line(canvas, x0, y0, x1, y1, color);
            }

            return canvas;
        }
    }
}
