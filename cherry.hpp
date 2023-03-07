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


        enum class BlendMode {
            OVERWRITE,
            ALPHA_COMPOSITING,
            FAST_ALPHA_COMPOSITING
        };


        [[maybe_unused]]
        [[nodiscard]]
        inline auto OverwriteBlend(
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
    }

    using TBlendFn = uint32_t(*)(
            uint32_t,
            uint32_t);

    template<TBlendFn BlendPixelFn = color::OverwriteBlend>
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


        template<TBlendFn NewBlendFn>
        [[nodiscard]]
        inline auto WithBlendMode() -> Canvas<NewBlendFn> {
            return {
                    Data,
                    Width,
                    Height,
                    Stride
            };
        }


        [[nodiscard]]
        inline auto SubCanvas(
                int x0,
                int y0,
                int x1,
                int y1) const -> Canvas {
            utility::SortTopLeft(x0, y0, x1, y1);

            CheckBounds(x0, y0);
            CheckBounds(x1 - 1, y1 - 1);

            return Canvas{
                    Data + Stride * y0 + x0,
                    x1 - x0,
                    y1 - y0,
                    Stride
            };
        }


        inline auto BlendPixel(
                int x,
                int y,
                uint32_t pixel) -> Canvas & {
            CheckBounds(x, y);

            OverwritePixel(x, y, BlendPixelFn(pixel, Pixel(x, y)));

            return *this;
        }


        inline auto OverwritePixel(
                int x,
                int y,
                uint32_t pixel) -> Canvas & {
            CheckBounds(x, y);

            Data[Stride * y + x] = pixel;

            return *this;
        }


        [[nodiscard]]
        inline auto Pixel(
                int x,
                int y) const -> uint32_t {
            CheckBounds(x, y);

            return Data[Stride * y + x];
        }


        inline auto Fill(uint32_t color) -> Canvas & {
            for (auto y = decltype(Height){ 0 }; y < Height; y += 1) {
                for (auto x = decltype(Width){ 0 }; x < Width; x += 1) {
                    BlendPixel(x, y, color);
                }
            }

            return *this;
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
        [[nodiscard]]
        constexpr inline auto ApplyRotationX(
                int x,
                int y,
                double sin,
                double cos) -> int {
            return std::lround(cos * x + sin * y);
        }


        [[nodiscard]]
        constexpr inline auto ApplyRotationY(
                int x,
                int y,
                double sin,
                double cos) -> int {
            return std::lround(-sin * x + cos * y);
        }


        [[nodiscard]]
        constexpr inline auto ApplyRotation(
                int x,
                int y,
                double sin,
                double cos) -> std::pair<int, int> {
            return { ApplyRotationX(x, y, sin, cos), ApplyRotationY(x, y, sin, cos) };
        }


        template<TBlendFn SrcBlendFn, TBlendFn DstBlendFn>
        [[maybe_unused]]
        inline auto Copy(
                const Canvas<SrcBlendFn> & src,
                Canvas<DstBlendFn> & dst,
                int left,
                int top,
                int right,
                int bottom) -> decltype(dst) {
            if (src.Empty) {
                return dst;
            }

            const auto target_width = std::abs(left - right);
            const auto target_height = std::abs(top - bottom);

            const auto mirrored_x = left > right;
            const auto mirrored_y = top > bottom;

            utility::SortTopLeft(left, top, right, bottom);

            const auto dst_start_y = std::max(top, 0);
            const auto dst_end_y = std::min(bottom, dst.Height);

            const auto dst_start_x = std::max(left, 0);
            const auto dst_end_x = std::min(right, dst.Width);

            for (auto dst_y = dst_start_y; dst_y < dst_end_y; dst_y += 1) {
                auto src_y = (dst_y - top) * src.Height / target_height;
                if (mirrored_y) {
                    src_y = src.Height - 1 - src_y;
                }

                for (auto dst_x = dst_start_x; dst_x < dst_end_x; dst_x += 1) {
                    auto src_x = (dst_x - left) * src.Width / target_width;
                    if (mirrored_x) {
                        src_x = src.Width - 1 - src_x;
                    }

                    dst.BlendPixel(dst_x, dst_y, src.Pixel(src_x, src_y));
                }
            }

            return dst;
        }


        template<TBlendFn SrcBlendFn, TBlendFn DstBlendFn>
        [[maybe_unused]]
        inline auto Rotate(
                const Canvas<SrcBlendFn> & src,
                Canvas<DstBlendFn> & dst,
                int src_origin_x,
                int src_origin_y,
                int dst_origin_x,
                int dst_origin_y,
                double radians) -> decltype(dst) {
            const auto sin = std::sin(radians);
            const auto cos = std::cos(radians);

            const auto src_left = -src_origin_x;
            const auto src_top = -src_origin_y;
            const auto src_right = src_left + src.Width;
            const auto src_bottom = src_top + src.Height;

            const auto[ax, ay] = ApplyRotation(src_left, src_top, sin, cos);
            const auto[bx, by] = ApplyRotation(src_right, src_top, sin, cos);
            const auto[cx, cy] = ApplyRotation(src_right, src_bottom, sin, cos);
            const auto[dx, dy] = ApplyRotation(src_left, src_bottom, sin, cos);

            auto[min_x, max_x] = std::minmax({ ax, bx, cx, dx });
            min_x += dst_origin_x;
            max_x += dst_origin_x;

            auto[min_y, max_y] = std::minmax({ ay, by, cy, dy });
            min_y += dst_origin_y;
            max_y += dst_origin_y;

            const auto start_y = std::max(min_y, 0);
            const auto end_y = std::min(max_y, dst.Height);

            const auto start_x = std::max(min_x, 0);
            const auto end_x = std::min(max_x, dst.Width);

            for (auto y2 = start_y; y2 < end_y; y2 += 1) {
                for (auto x2 = start_x; x2 < end_x; x2 += 1) {
                    auto[x1, y1] = ApplyRotation(x2 - dst_origin_x, y2 - dst_origin_y, -sin, cos);
                    x1 += src_origin_x;
                    y1 += src_origin_y;

                    dst.BlendPixel(
                            x2, y2,
                            src.IsWithinBounds(x1, y1) ? src.Pixel(x1, y1) : color::FromRGBA(0xFF, 0xFF, 0xFF, 0x00)
                    );
                }
            }

            return dst;
        }
    }


    namespace drawing {
        template<TBlendFn BlendFn>
        inline auto LineLow(
                Canvas<BlendFn> & canvas,
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


        template<TBlendFn BlendFn>
        inline auto LineHigh(
                Canvas<BlendFn> & canvas,
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


        template<TBlendFn BlendFn>
        [[maybe_unused]]
        inline auto Line(
                Canvas<BlendFn> & canvas,
                int x0,
                int y0,
                int x1,
                int y1,
                uint32_t color) -> decltype(canvas) {
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


        template<TBlendFn BlendFn>
        [[maybe_unused]]
        inline auto FillRectangle(
                Canvas<BlendFn> & canvas,
                int left,
                int top,
                int right,
                int bottom,
                uint32_t color) -> decltype(canvas) {
            canvas.SubCanvas(left, top, right, bottom).Fill(color);

            return canvas;
        }


        template<TBlendFn BlendFn>
        [[maybe_unused]]
        inline auto Polygon(
                Canvas<BlendFn> & canvas,
                const std::vector<std::pair<int, int>> & vertices,
                uint32_t color) -> decltype(canvas) {
            if (vertices.empty()) {
                return canvas;
            }

            for (auto i = decltype(vertices.size()){ 0 }; i < vertices.size(); i += 1) {
                const auto[x0, y0] = vertices[i % vertices.size()];
                const auto[x1, y1] = vertices[(i + 1) % vertices.size()];

                Line(canvas, x0, y0, x1, y1, color);
            }

            return canvas;
        }


        template<TBlendFn BlendFn>
        inline auto FillFlatTriangle(
                Canvas<BlendFn> & canvas,
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
                    canvas.BlendPixel(x, y, color);
                }
            }
            return canvas;
        }


        template<TBlendFn BlendFn>
        [[maybe_unused]]
        inline auto FillTriangle(
                Canvas<BlendFn> & canvas,
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
                return FillFlatTriangle(canvas, x0, y0, x1, y1, x2, color);
            }

            const auto x_intermediate = x0 + (y1 - y0) * (x2 - x0) / (y2 - y0);

            FillFlatTriangle(canvas, x0, y0, x1, y1, x_intermediate, color);
            FillFlatTriangle(canvas, x2, y2, x1, y1 + 1, x_intermediate, color);

            return canvas;
        }
    }
}
