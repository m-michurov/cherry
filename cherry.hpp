#pragma once

#include <stdexcept>
#include <limits>
#include <tuple>
#include <algorithm>
#include <cstdint>


namespace cherry {
    template<typename T>
    inline auto ClampTo(int64_t value) -> T {
        constexpr auto min = static_cast<decltype(value)>(std::numeric_limits<T>::min());
        constexpr auto max = static_cast<decltype(value)>(std::numeric_limits<T>::max());

        return static_cast<T>(std::clamp(value, min, max));
    }


    template<typename T>
    inline auto IsWithinRange(int64_t value) -> bool {
        constexpr auto min = static_cast<decltype(value)>(std::numeric_limits<T>::min());
        constexpr auto max = static_cast<decltype(value)>(std::numeric_limits<T>::max());

        return value >= min and value <= max;
    }


    constexpr auto SHIFT_RED = 0;
    constexpr auto SHIFT_GREEN = 8;
    constexpr auto SHIFT_BLUE = 16;
    constexpr auto SHIFT_ALPHA = 24;


    [[nodiscard]] constexpr inline auto CombineRGBA(
            uint32_t red,
            uint32_t green,
            uint32_t blue,
            uint32_t alpha) -> uint32_t {
        return
                ((red & 0xFF) << SHIFT_RED)
                | ((green & 0xFF) << SHIFT_GREEN)
                | ((blue & 0xFF) << SHIFT_BLUE)
                | ((alpha & 0xFF) << SHIFT_ALPHA);
    }


    [[nodiscard]] constexpr inline auto DeconstructRGBA(
            uint32_t pixel) -> std::tuple<uint8_t, uint8_t, uint8_t, uint8_t> {
        return {
                (pixel >> SHIFT_RED) & 0xFF,
                (pixel >> SHIFT_GREEN) & 0xFF,
                (pixel >> SHIFT_BLUE) & 0xFF,
                (pixel >> SHIFT_ALPHA) & 0xFF,
        };
    }


    [[nodiscard]] inline auto BlendOver(
            uint32_t src,
            uint32_t dst) -> uint32_t {
        const auto[src_r, src_g, src_b, src_a] = DeconstructRGBA(src);
        const auto[dst_r, dst_g, dst_b, dst_a] = DeconstructRGBA(dst);

        const auto a = src_a + dst_a * (255 - src_a) / 255;
        const auto r = (src_r * src_a + dst_r * dst_a * (255 - src_a) / 255) / a;
        const auto g = (src_g * src_a + dst_g * dst_a * (255 - src_a) / 255) / a;
        const auto b = (src_b * src_a + dst_b * dst_a * (255 - src_a) / 255) / a;

        return CombineRGBA(r, g, b, a);
    }


    enum class PixelBlendMode {
        OVERWRITE,
        ALPHA_COMPOSITING
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
            if (not IsWithinRange<decltype(Width)>(width)) {
                throw std::out_of_range("Invalid height: " + std::to_string(width));
            }
            if (not IsWithinRange<decltype(Height)>(height)) {
                throw std::out_of_range("Invalid height: " + std::to_string(height));
            }
            if (not IsWithinRange<decltype(Stride)>(stride)) {
                throw std::out_of_range("Invalid stride: " + std::to_string(stride));
            }

            SetBlendMode(mode);
        }


        Canvas(
                uint8_t * data,
                int64_t width,
                int64_t height,
                int64_t stride,
                PixelBlendMode mode = PixelBlendMode::OVERWRITE)
                :
                Canvas(reinterpret_cast<uint32_t *>(data), width, height, stride, mode) {}


        [[nodiscard]] inline auto BlendMode() const -> PixelBlendMode {
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
            }

            return *this;
        }


        class BlendModeGuard {
            Canvas & canvas;
            PixelBlendMode previous_mode;

        public:
            BlendModeGuard(
                    Canvas & canvas,
                    PixelBlendMode new_mode)
                    :
                    canvas(canvas),
                    previous_mode(canvas.BlendMode()) {
                canvas.SetBlendMode(new_mode);
            }


            ~BlendModeGuard() {
                canvas.SetBlendMode(previous_mode);
            }
        };


        [[nodiscard]] inline auto WithBlendMode(PixelBlendMode mode) -> BlendModeGuard {
            return { *this, mode };
        }


        inline auto SubCanvas(
                int64_t left,
                int64_t top,
                int64_t right,
                int64_t bottom) -> Canvas {
            std::tie(left, right) = std::tuple(std::min(left, right), std::max(left, right));
            std::tie(top, bottom) = std::tuple(std::min(top, bottom), std::max(top, bottom));

            CheckBounds(left, top);
            CheckBounds(right - 1, bottom - 1);

            return Canvas{
                    Data + Stride * top + left,
                    right - left,
                    bottom - top,
                    Stride,
                    blend_mode
            }
#ifdef CHERRY_NAMED_CANVAS
                .SetName(
                        name + "$(Left="
                        + std::to_string(box.Left)
                        + ", Top=" + std::to_string(box.Top)
                        + ", Right=" + std::to_string(box.Right)
                        + ", Bottom=" + std::to_string(box.Bottom) + ")"
                )
#endif // CHERRY_NAMED_CANVAS
                    ;
        }


        inline auto BlendPixel(
                int64_t x,
                int64_t y,
                uint32_t pixel) -> Canvas & {
            CheckBounds(x, y);

            BlendPixelFn(*this, x, y, pixel);

            return *this;
        }


        [[nodiscard]] inline auto Pixel(
                int64_t x,
                int64_t y) const -> uint32_t {
            CheckBounds(x, y);

            return Data[Stride * y + x];
        }


        [[nodiscard]] inline auto Pixel(
                int64_t x,
                int64_t y) -> uint32_t & {
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


        inline auto FillRectangle(
                int64_t left,
                int64_t top,
                int64_t right,
                int64_t bottom,
                uint32_t color) -> Canvas & {
            SubCanvas(left, top, right, bottom).Fill(color);

            return *this;
        }


        inline auto Line(
                int64_t x0,
                int64_t y0,
                int64_t x1,
                int64_t y1,
                uint32_t color) -> Canvas & {
            if (std::abs(y1 - y0) < std::abs(x1 - x0)) {
                if (x0 > x1) {
                    LineLow(x1, y1, x0, y0, color);
                }
                else {
                    LineLow(x0, y0, x1, y1, color);
                }
            }
            else {
                if (y0 > y1) {
                    LineHigh(x1, y1, x0, y0, color);
                }
                else {
                    LineHigh(x0, y0, x1, y1, color);
                }
            }


            return *this;
        }


        inline auto Blend(
                const Canvas & src,
                int64_t left,
                int64_t top,
                int64_t right,
                int64_t bottom) -> Canvas & {
            if (src.Empty) {
                return *this;
            }

            auto target = SubCanvas(left, top, right, bottom);
            if (target.Empty) {
                return *this;
            }

            const auto mirrored_x = left > right;
            const auto mirrored_y = top > bottom;

            for (auto y = 0; y < target.Height; y += 1) {
                for (auto x = 0; x < target.Width; x += 1) {
                    auto src_x = x * src.Width / target.Width;
                    if (mirrored_x) {
                        src_x = src.Width - 1 - src_x;
                    }
                    auto src_y = y * src.Height / target.Height;
                    if (mirrored_y) {
                        src_y = src.Height - 1 - src_y;
                    }

                    target.BlendPixel(x, y, src.Pixel(src_x, src_y));
                }
            }

            return *this;
        }


#ifdef CHERRY_NAMED_CANVAS
        inline auto SetName(const std::string & new_name) -> Canvas & {
            name = new_name;

            return *this;
        }
#endif // CHERRY_NAMED_CANVAS


    private:
        inline auto LineLow(
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
                BlendPixel(x, y, color);

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
                BlendPixel(x, y, color);

                if (D > 0) {
                    x += xi;
                    D += 2 * (dx - dy);
                }
                else {
                    D += 2 * dx;
                }
            }
        }


        [[nodiscard]] inline auto WithinBounds(
                int64_t x,
                int64_t y) const -> bool {
            return x >= 0 and y >= 0 and x < Width and y < Height;
        }


        inline auto CheckBounds(
                int64_t x,
                int64_t y) const -> void {
#ifdef CHERRY_CHECK_BOUNDS
            if (WithinBounds(x, y)) {
                return;
            }

            const auto message =
#ifdef CHERRY_NAMED_CANVAS
                    "Canvas \"" + name + "\": coordinates ("
#else // CHERRY_NAMED_CANVAS
                    "Coordinates ("
#endif // CHERRY_NAMED_CANVAS
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
            canvas.Pixel(x, y) = color;
        }


        static inline auto BlendAlpha(
                Canvas & canvas,
                int64_t x,
                int64_t y,
                uint32_t color) -> void {
            canvas.Pixel(x, y) = BlendOver(color, canvas.Pixel(x, y));
        }


#ifdef CHERRY_NAMED_CANVAS
        std::string name;
#endif // CHERRY_NAMED_CANVAS
    };
}
