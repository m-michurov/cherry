#pragma once

#ifdef CHERRY_CHECK_BOUNDS
#include <stdexcept>
#endif
#include <cmath>
#include <limits>
#include <tuple>
#include <algorithm>
#include <cstdint>
#include <utility>
#include <type_traits>
#include <vector>


namespace cherry {
    namespace utility {
        [[maybe_unused]]
        [[nodiscard]]
        inline auto PixelBuffer(
                int stride,
                int height) -> std::vector<uint32_t> {
            return std::vector<uint32_t>(stride * height);
        }


        [[maybe_unused]]
        [[nodiscard]]
        inline auto PixelBuffer(
                int stride,
                int height,
                uint32_t fill_color) -> std::vector<uint32_t> {
            auto buffer = PixelBuffer(stride, height);

            std::fill(buffer.begin(), buffer.end(), fill_color);

            return buffer;
        }


        [[maybe_unused]]
        [[nodiscard]]
        inline auto PixelBuffer(
                const uint8_t * data,
                int stride,
                int height) -> std::vector<uint32_t> {
            const auto size = stride * height;
            auto buffer = std::vector<uint32_t>(
                    reinterpret_cast<const uint32_t *>(data),
                    reinterpret_cast<const uint32_t *>(data) + size
            );

            return buffer;
        }


        inline auto SortTopLeft(
                int & x0,
                int & y0,
                int & x1,
                int & y1) -> void {
            if (x1 < x0) {
                std::swap(x0, x1);
            }

            if (y1 < y0) {
                std::swap(y0, y1);
            }
        }


        class Fixed final {
            static constexpr auto DIGITS = 8;

            int repr;
        public:
            [[maybe_unused]]
            explicit constexpr Fixed(float value) :
                    repr(static_cast<int>(value * (1 << DIGITS))) {}


            [[maybe_unused]]
            explicit constexpr Fixed(double value) :
                    repr(static_cast<int>(value * (1 << DIGITS))) {}


            [[nodiscard]]
            inline auto operator*(int value) const -> int {
                return (value * repr) >> DIGITS;
            }


            friend auto operator/(
                    int a,
                    Fixed b) -> int;

            friend auto operator*(
                    int a,
                    Fixed b) -> int;
        };


        [[nodiscard]]
        inline auto operator/(
                int a,
                Fixed b) -> int {
            return (a << Fixed::DIGITS) / b.repr;
        }


        [[nodiscard]]
        inline auto operator*(
                int a,
                Fixed b) -> int {
            return (a * b.repr) >> Fixed::DIGITS;
        }
    }


    namespace color {
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
        constexpr inline auto FromRGBA(
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
        constexpr inline auto ToRGBA(
                uint32_t pixel) -> std::tuple<uint8_t, uint8_t, uint8_t, uint8_t> {
            return {
                    (pixel >> SHIFT_RED) & 0xFF,
                    (pixel >> SHIFT_GREEN) & 0xFF,
                    (pixel >> SHIFT_BLUE) & 0xFF,
                    (pixel >> SHIFT_ALPHA) & 0xFF,
            };
        }


        [[maybe_unused]]
        [[nodiscard]]
        inline auto Overwrite(
                uint32_t foreground,
                uint32_t) -> uint32_t {
            return foreground;
        }


        [[maybe_unused]]
        [[nodiscard]]
        inline auto AlphaBlend(
                uint32_t foreground,
                uint32_t background) -> uint32_t {
            const auto[fg_r, fg_g, fg_b, fg_a] = ToRGBA(foreground);
            const auto[bg_r, bg_g, bg_b, bg_a] = ToRGBA(background);

            const auto a = fg_a + bg_a * (255 - fg_a) / 255;
            const auto r = (fg_r * fg_a + bg_r * bg_a * (255 - fg_a) / 255) / a;
            const auto g = (fg_g * fg_a + bg_g * bg_a * (255 - fg_a) / 255) / a;
            const auto b = (fg_b * fg_a + bg_b * bg_a * (255 - fg_a) / 255) / a;

            return FromRGBA(r, g, b, a);
        }


        [[maybe_unused]]
        [[nodiscard]]
        inline auto FastAlphaBlend(
                uint32_t foreground,
                uint32_t background) -> uint32_t {
            const auto fg_a = ((foreground & MASK_ALPHA) >> SHIFT_ALPHA);
            if (not fg_a) {
                return background;
            }

            const auto alpha = fg_a + 1;
            const auto inv_alpha = 256 - fg_a;

            const auto rb =
                    (alpha * static_cast<uint64_t >(foreground & MASK_RED_BLUE)
                     + inv_alpha * static_cast<uint64_t >(background & MASK_RED_BLUE)) >> 8;
            const auto g =
                    (alpha * static_cast<uint64_t >(foreground & MASK_GREEN)
                     + inv_alpha * static_cast<uint64_t >(background & MASK_GREEN)) >> 8;

            return (rb & MASK_RED_BLUE) | (g & MASK_GREEN) | (~0 & MASK_ALPHA);
        }


        using BlendType = decltype(Overwrite);
    }


    class Canvas final {
    public:
        uint32_t * const Data{ nullptr };
        uint8_t * const DataUint8{ nullptr };
        const int Width{ 0u };
        const int Height{ 0u };
        const int Stride{ 0u };
        const bool Empty{ false };


        Canvas(
                uint32_t * data,
                int width,
                int height,
                int stride)
                :
                Data(data),
                DataUint8(reinterpret_cast<uint8_t *>(data)),
                Width(width),
                Height(height),
                Stride(stride),
                Empty(not width or not height) {
#ifdef CHERRY_CHECK_BOUNDS
            if (width < 0) {
                throw std::out_of_range("Invalid width: " + std::to_string(width));
            }
            if (height < 0) {
                throw std::out_of_range("Invalid height: " + std::to_string(height));
            }
            if (stride < 0 or stride < width) {
                throw std::out_of_range("Invalid stride: " + std::to_string(stride));
            }
#endif
        }


        [[maybe_unused]]
        Canvas(
                uint32_t * data,
                int width,
                int height)
                :
                Canvas(data, width, height, width) {}


        template<color::BlendType BlendFn = color::Overwrite>
        inline auto BlendPixel(
                int x,
                int y,
                uint32_t color) -> Canvas & {
            CheckBounds(x, y);

            Data[Stride * y + x] = BlendFn(color, Data[Stride * y + x]);

            return *this;
        }


        [[nodiscard]]
        inline auto Pixel(
                int x,
                int y) const -> uint32_t {
            CheckBounds(x, y);

            return Data[Stride * y + x];
        }


        [[nodiscard]]
        inline auto IsWithinBounds(
                int x,
                int y) const -> bool {
            return x >= 0 and y >= 0 and x < Width and y < Height;
        }


    private:
#ifdef CHERRY_CHECK_BOUNDS


        inline auto CheckBounds(
                int x,
                int y) const -> void {
            if (IsWithinBounds(x, y)) {
                return;
            }

            const auto message =
                    "Coordinates ("
                    + std::to_string(x) + ", " + std::to_string(y) +
                    ") are out of bounds for image size ("
                    + std::to_string(Width) + ", " + std::to_string(Height) + ")";
            throw std::out_of_range(message);
        }


#else


        inline auto CheckBounds(
                int,
                int) const -> void {}


#endif // CHERRY_CHECK_BOUNDS
    };


    namespace transform {
        using FixedPoint = utility::Fixed;


        [[nodiscard]]
        constexpr inline auto ApplyRotation(
                int x,
                int y,
                float sin,
                float cos) -> std::pair<int, int> {
            return {
                    std::lround(cos * static_cast<float>(x) + sin * static_cast<float>(y)),
                    std::lround(-sin * static_cast<float>(x) + cos * static_cast<float>(y))
            };
        }


        template<color::BlendType BlendFn>
        [[maybe_unused]]
        inline auto Copy(
                const Canvas & src,
                Canvas & dst,
                int x0,
                int y0,
                int x1,
                int y1) -> decltype(dst) {
            if (src.Empty) {
                return dst;
            }

            const auto target_width = std::abs(x0 - x1);
            const auto target_height = std::abs(y0 - y1);

            const auto mirrored_x = x0 > x1;
            const auto mirrored_y = y0 > y1;

            utility::SortTopLeft(x0, y0, x1, y1);

            const auto dst_start_y = std::max(y0, 0);
            const auto dst_end_y = std::min(y1, dst.Height);

            const auto dst_start_x = std::max(x0, 0);
            const auto dst_end_x = std::min(x1, dst.Width);

            for (auto y = dst_start_y; y < dst_end_y; y += 1) {
                auto v = (y - y0) * src.Height / target_height;
                if (mirrored_y) {
                    v = src.Height - 1 - v;
                }

                for (auto x = dst_start_x; x < dst_end_x; x += 1) {
                    auto u = (x - x0) * src.Width / target_width;
                    if (mirrored_x) {
                        u = src.Width - 1 - u;
                    }

                    dst.BlendPixel<BlendFn>(x, y, src.Pixel(u, v));
                }
            }

            return dst;
        }


        template<color::BlendType BlendFn>
        [[maybe_unused]]
        inline auto Copy(
                const Canvas & src,
                Canvas & dst,
                int x0,
                int y0,
                int u0,
                int v0,
                float rotation) -> decltype(dst) {
            const auto sin = std::sin(rotation);
            const auto cos = std::cos(rotation);

            const auto src_left = -u0;
            const auto src_top = -v0;
            const auto src_right = src_left + src.Width;
            const auto src_bottom = src_top + src.Height;

            const auto[ax, ay] = ApplyRotation(src_left, src_top, sin, cos);
            const auto[bx, by] = ApplyRotation(src_right, src_top, sin, cos);
            const auto[cx, cy] = ApplyRotation(src_right, src_bottom, sin, cos);
            const auto[dx, dy] = ApplyRotation(src_left, src_bottom, sin, cos);

            auto[min_x, max_x] = std::minmax({ ax, bx, cx, dx });
            min_x += x0;
            max_x += x0;

            auto[min_y, max_y] = std::minmax({ ay, by, cy, dy });
            min_y += y0;
            max_y += y0;

            const auto start_y = std::max(min_y, 0);
            const auto end_y = std::min(max_y, dst.Height);

            const auto start_x = std::max(min_x, 0);
            const auto end_x = std::min(max_x, dst.Width);

            constexpr auto transparent = color::FromRGBA(0xFF, 0xFF, 0xFF, 0x00);

            for (auto y = start_y; y < end_y; y += 1) {
                for (auto x = start_x; x < end_x; x += 1) {
                    auto[u, v] = ApplyRotation(x - x0, y - y0, -sin, cos);
                    u += u0;
                    v += v0;

                    dst.BlendPixel<BlendFn>(x, y, src.IsWithinBounds(u, v) ? src.Pixel(u, v) : transparent);
                }
            }

            return dst;
        }


        template<color::BlendType BlendFn>
        [[maybe_unused]]
        inline auto Copy(
                const Canvas & src,
                Canvas & dst,
                int x0,
                int y0,
                int u0,
                int v0,
                float rotation,
                FixedPoint scale_x,
                FixedPoint scale_y) -> decltype(dst) {
            const auto sin = std::sin(rotation);
            const auto cos = std::cos(rotation);

            const auto src_left = -u0 * scale_x;
            const auto src_top = -v0 * scale_y;
            const auto src_right = src_left + src.Width * scale_x;
            const auto src_bottom = src_top + src.Height * scale_y;

            const auto[ax, ay] = ApplyRotation(src_left, src_top, sin, cos);
            const auto[bx, by] = ApplyRotation(src_right, src_top, sin, cos);
            const auto[cx, cy] = ApplyRotation(src_right, src_bottom, sin, cos);
            const auto[dx, dy] = ApplyRotation(src_left, src_bottom, sin, cos);

            auto[min_x, max_x] = std::minmax({ ax, bx, cx, dx });
            min_x += x0;
            max_x += x0;

            auto[min_y, max_y] = std::minmax({ ay, by, cy, dy });
            min_y += y0;
            max_y += y0;

            const auto start_y = std::max(min_y, 0);
            const auto end_y = std::min(max_y, dst.Height);

            const auto start_x = std::max(min_x, 0);
            const auto end_x = std::min(max_x, dst.Width);

            constexpr auto transparent = color::FromRGBA(0xFF, 0xFF, 0xFF, 0x00);

            for (auto y = start_y; y < end_y; y += 1) {
                for (auto x = start_x; x < end_x; x += 1) {
                    auto[u, v] = ApplyRotation(x - x0, y - y0, -sin, cos);

                    u = u0 + u / scale_x;
                    v = v0 + v / scale_y;

                    dst.BlendPixel<BlendFn>(x, y, src.IsWithinBounds(u, v) ? src.Pixel(u, v) : transparent);
                }
            }

            return dst;
        }


        struct Transform {
            float RotationRadians{ 0.0f };

            int OriginX{ 0 };
            int OriginY{ 0 };

            float ScaleX{ 1.0f };
            float ScaleY{ 1.0f };
        };


        template<color::BlendType BlendFn>
        [[maybe_unused]]
        inline auto Copy(
                const Canvas & src,
                Canvas & dst,
                int x0 = 0,
                int y0 = 0,
                const Transform & tf = {}) -> decltype(dst) {
            if (0.0f == tf.RotationRadians) {
                const auto scale_x = FixedPoint{ tf.ScaleX };
                const auto scale_y = FixedPoint{ tf.ScaleY };

                return Copy<BlendFn>(
                        src,
                        dst,
                        x0 - tf.OriginX * scale_x,
                        y0 - tf.OriginY * scale_y,
                        x0 + (src.Width - tf.OriginX) * scale_x,
                        y0 + (src.Height - tf.OriginY) * scale_y
                );
            }

            if (1.0f == tf.ScaleX and 1.0f == tf.ScaleY) {
                return Copy<BlendFn>(
                        src,
                        dst,
                        x0, y0,
                        tf.OriginX, tf.OriginY,
                        tf.RotationRadians
                );
            }

            return Copy<BlendFn>(
                    src,
                    dst,
                    x0, y0,
                    tf.OriginX, tf.OriginY,
                    tf.RotationRadians,
                    FixedPoint{ tf.ScaleX },
                    FixedPoint{ tf.ScaleY }
            );
        }
    }


    namespace drawing {
        template<color::BlendType BlendFn>
        inline auto LineLow(
                Canvas & canvas,
                int x0,
                int y0,
                int x1,
                int y1,
                uint32_t color) -> void {
            const auto dx = x1 - x0;
            const auto[dy, yi] = ((y1 - y0) >= 0) ? std::pair(y1 - y0, 1) : std::pair(y0 - y1, -1);

            auto D = 2 * dy - dx;
            auto y = y0;

            for (auto x = x0; x <= x1; x += 1) {
                canvas.BlendPixel<BlendFn>(x, y, color);

                if (D > 0) {
                    y += yi;
                    D += 2 * (dy - dx);
                }
                else {
                    D += 2 * dy;
                }
            }
        }


        template<color::BlendType BlendFn>
        inline auto LineHigh(
                Canvas & canvas,
                int x0,
                int y0,
                int x1,
                int y1,
                uint32_t color) -> void {
            const auto[dx, xi] = ((x1 - x0) >= 0) ? std::pair(x1 - x0, 1) : std::pair(x0 - x1, -1);
            const auto dy = y1 - y0;

            auto D = 2 * dx - dy;
            auto x = x0;

            for (auto y = y0; y <= y1; y += 1) {
                canvas.BlendPixel<BlendFn>(x, y, color);

                if (D > 0) {
                    x += xi;
                    D += 2 * (dx - dy);
                }
                else {
                    D += 2 * dx;
                }
            }
        }


        template<color::BlendType BlendFn>
        [[maybe_unused]]
        inline auto Line(
                Canvas & canvas,
                int x0,
                int y0,
                int x1,
                int y1,
                uint32_t color) -> decltype(canvas) {
            if (std::abs(y1 - y0) < std::abs(x1 - x0)) {
                if (x0 > x1) {
                    LineLow<BlendFn>(canvas, x1, y1, x0, y0, color);
                }
                else {
                    LineLow<BlendFn>(canvas, x0, y0, x1, y1, color);
                }
            }
            else {
                if (y0 > y1) {
                    LineHigh<BlendFn>(canvas, x1, y1, x0, y0, color);
                }
                else {
                    LineHigh<BlendFn>(canvas, x0, y0, x1, y1, color);
                }
            }


            return canvas;
        }


        template<color::BlendType BlendFn>
        [[maybe_unused]]
        inline auto Polygon(
                Canvas & canvas,
                const std::vector<std::pair<int, int>> & vertices,
                uint32_t color) -> decltype(canvas) {
            if (vertices.empty()) {
                return canvas;
            }

            for (auto i = decltype(vertices.size()){ 0 }; i < vertices.size(); i += 1) {
                const auto[x0, y0] = vertices[i % vertices.size()];
                const auto[x1, y1] = vertices[(i + 1) % vertices.size()];

                Line<BlendFn>(canvas, x0, y0, x1, y1, color);
            }

            return canvas;
        }


        template<color::BlendType BlendFn>
        inline auto FillFlatTriangle(
                Canvas & canvas,
                int x0,
                int y0,
                int x1,
                int y1,
                int x2,
                uint32_t color) -> decltype(canvas) {
            if (y1 == y0) {
                return canvas;
            }

            const auto dy = y1 > y0 ? 1 : -1;

            if (x2 < x1) {
                std::swap(x2, x1);
            }

            for (
                    auto y = std::clamp(y0, 0, canvas.Height - 1);
                    y != std::clamp(y1 + dy, 0, canvas.Height - 1);
                    y += dy) {
                const auto x_left = x0 + (y - y0) * (x1 - x0) / (y1 - y0);
                const auto x_right = x0 + (y - y0) * (x2 - x0) / (y1 - y0);

                for (auto x = std::max<int>(0, x_left); x < std::min<int>(canvas.Width, x_right + 1); x += 1) {
                    canvas.BlendPixel<BlendFn>(x, y, color);
                }
            }
            return canvas;
        }


        template<color::BlendType BlendFn>
        [[maybe_unused]]
        inline auto FillTriangle(
                Canvas & canvas,
                int x0,
                int y0,
                int x1,
                int y1,
                int x2,
                int y2,
                uint32_t color) -> decltype(canvas) {
            if (y0 > y1) {
                std::swap(x0, x1);
                std::swap(y0, y1);
            }

            if (y0 > y2) {
                std::swap(x0, x2);
                std::swap(y0, y2);
            }

            if (y1 > y2) {
                std::swap(x1, x2);
                std::swap(y1, y2);
            }

            if (y1 == y2) {
                return FillFlatTriangle<BlendFn>(canvas, x0, y0, x1, y1, x2, color);
            }

            const auto x_intermediate = x0 + (y1 - y0) * (x2 - x0) / (y2 - y0);

            FillFlatTriangle<BlendFn>(canvas, x0, y0, x1, y1, x_intermediate, color);
            FillFlatTriangle<BlendFn>(canvas, x2, y2, x1, y1 + 1, x_intermediate, color);

            return canvas;
        }
    }
}
