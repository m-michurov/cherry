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
#include <map>


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


        template<typename T = uint8_t>
        [[nodiscard]]
        constexpr inline auto ToRGBA(
                uint32_t pixel) -> std::tuple<T, T, T, T> {
            return {
                    static_cast<T>((pixel >> SHIFT_RED) & 0xFF),
                    static_cast<T>((pixel >> SHIFT_GREEN) & 0xFF),
                    static_cast<T>((pixel >> SHIFT_BLUE) & 0xFF),
                    static_cast<T>((pixel >> SHIFT_ALPHA) & 0xFF),
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
        inline auto AlphaWeightedAdd(
                uint32_t foreground,
                uint32_t background) -> uint32_t {
            const auto[fg_r, fg_g, fg_b, fg_a] = ToRGBA(foreground);
            const auto[bg_r, bg_g, bg_b, bg_a] = ToRGBA(background);

            const auto r = std::clamp(fg_r * fg_a / 255 + bg_r, 0, 0xFF);
            const auto g = std::clamp(fg_g * fg_a / 255 + bg_g, 0, 0xFF);
            const auto b = std::clamp(fg_b * fg_a / 255 + bg_b, 0, 0xFF);
            const auto a = bg_a;

            return FromRGBA(r, g, b, a);
        }


        [[maybe_unused]]
        [[nodiscard]]
        inline auto Add(
                uint32_t foreground,
                uint32_t background) -> uint32_t {
            const auto[fg_r, fg_g, fg_b, fg_a] = ToRGBA(foreground);
            const auto[bg_r, bg_g, bg_b, bg_a] = ToRGBA(background);

            const auto r = std::clamp(fg_r + bg_r, 0, 0xFF);
            const auto g = std::clamp(fg_g + bg_g, 0, 0xFF);
            const auto b = std::clamp(fg_b + bg_b, 0, 0xFF);
            const auto a = bg_a;

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
        uint32_t * const pixels{ nullptr };

    public:
        const int Width{ 0u };
        const int Height{ 0u };
        const int Stride{ 0u };


        [[nodiscard]]
        inline auto DataUint8() const -> uint8_t const * {
            return reinterpret_cast<uint8_t *>(pixels);
        }


        [[nodiscard]]
        inline auto Empty() const -> bool {
            return not Width or not Height;
        }


        Canvas(
                uint32_t * data,
                int width,
                int height,
                int stride)
                :
                pixels(data),
                Width(width),
                Height(height),
                Stride(stride) {
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

            pixels[Stride * y + x] = BlendFn(color, pixels[Stride * y + x]);

            return *this;
        }


        [[nodiscard]]
        inline auto Pixel(
                int x,
                int y) const -> uint32_t {
            CheckBounds(x, y);

            return pixels[Stride * y + x];
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


    class PixelBufferPool final {
        std::vector<std::pair<int, std::unique_ptr<uint32_t[]>>> free;
        std::map<uint32_t *, int> used;


        inline auto ReturnBuffer(uint32_t * buffer) -> void {
            const auto size = used[buffer];
            used.erase(buffer);

            free.emplace_back(size, std::unique_ptr<uint32_t[]>(buffer));
        }


        inline auto BorrowBuffer(int requested_size) -> uint32_t * {
            auto it = free.begin();

            for (; it != free.end(); it++) {
                const auto buffer_size = it->first;

                if (buffer_size >= requested_size) {
                    break;
                }
            }

            if (it != free.end()) {
                const auto buffer_size = it->first;
                const auto buffer = it->second.get();

                it->second.release();
                free.erase(it);
                used[buffer] = buffer_size;

                return buffer;
            }

            const auto buffer = new uint32_t[requested_size];
            used[buffer] = requested_size;

            return buffer;
        }


        struct BufferDeleter {
            PixelBufferPool & pool;


            inline auto operator()(uint32_t * buffer) -> void {
                pool.ReturnBuffer(buffer);
            }
        };


    public:
        inline auto BorrowBuffer(
                int width,
                int height) -> std::unique_ptr<uint32_t[], BufferDeleter> {
            return std::unique_ptr<uint32_t[], BufferDeleter>(
                    BorrowBuffer(width * height),
                    BufferDeleter{ *this }
            );
        }


        class CanvasWrapper {
            std::unique_ptr<uint32_t[], BufferDeleter> Buffer;

        public:
            Canvas Canvas;


            CanvasWrapper(
                    decltype(Buffer) buffer,
                    int width,
                    int height)
                    :
                    Buffer(std::move(buffer)),
                    Canvas(Buffer.get(), width, height) {}


            CanvasWrapper(const CanvasWrapper &) = delete;

            auto operator=(const CanvasWrapper &) = delete;
        };


        inline auto BorrowCanvas(
                int width,
                int height) -> CanvasWrapper {
            auto buffer = BorrowBuffer(width, height);
            return { std::move(buffer), width, height };
        }


        static inline auto Default() -> PixelBufferPool & {
            static auto default_instance = PixelBufferPool();
            return default_instance;
        }
    };


    namespace math {
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


        [[nodiscard]]
        constexpr inline auto Gaussian(
                float x,
                float standard_deviation) -> float {
            constexpr auto double_pi_sqrt = 2.50662827463f;

            const auto exp_denominator = -2.0f * standard_deviation * standard_deviation;
            const auto denominator = double_pi_sqrt * standard_deviation;

            return std::exp(x * x / exp_denominator) / denominator;
        }


        struct Kernel1D {
            const int Size;
            const std::vector<float> Values;
        };


        [[maybe_unused]]
        inline auto GaussianKernel1D(
                int size,
                float standard_deviation = 0) -> Kernel1D {
            if (size < 0) {
                size = 0;
            }

            size += (not(size % 2));

            if (0.0f == standard_deviation) {
                standard_deviation = (static_cast<float >(size) - 1.0f) / 2.0f;
            }

            auto values = std::vector<float>(size, 1.0f);
            const auto origin = size / 2;

            for (auto x = -size / 2; x <= size / 2; x += 1) {
                values[origin + x] = Gaussian(static_cast<float>(x), standard_deviation);
            }

            return { size, values };
        }


        [[maybe_unused]]
        inline auto BoxBlurKernel1D(int size) -> Kernel1D {
            if (size < 0) {
                size = 0;
            }

            size += (not(size % 2));

            return { size, std::vector<float>(size, 1.0f / static_cast<float>(size)) };
        }


        template<int N>
        struct Float {
            float Values[N]{ 0.0f };


            Float(const Float<N> &) = default;

            auto operator=(const Float<N> &) -> Float<N> & = default;


            inline auto operator*=(float x) -> Float<N> & {
                for (auto i = 0; i < N; i += 1) {
                    Values[i] *= x;
                }

                return *this;
            }


            inline auto operator/=(float x) -> Float<N> & {
                return (*this *= (1.0f / x));
            }


            inline auto operator+=(const Float<N> & other) -> Float<N> & {
                for (auto i = 0; i < N; i += 1) {
                    Values[i] += other.Values[i];
                }

                return *this;
            }
        };


        inline auto UnpackPixel(
                uint32_t pixel,
                Float<3> & rgb,
                float & a) -> void {
            auto&[r, g, b] = rgb.Values;
            r = static_cast<float>((pixel >> color::SHIFT_RED) & 0xFF);
            g = static_cast<float>((pixel >> color::SHIFT_GREEN) & 0xFF);
            b = static_cast<float>((pixel >> color::SHIFT_BLUE) & 0xFF);
            a = static_cast<float>((pixel >> color::SHIFT_ALPHA) & 0xFF);
        }


        inline auto PackPixel(
                const Float<3> & rgb,
                float a) -> uint32_t {
            return color::FromRGBA(
                    std::clamp<int>(static_cast<int>(rgb.Values[color::INDEX_RED]), 0, 255),
                    std::clamp<int>(static_cast<int>(rgb.Values[color::INDEX_GREEN]), 0, 255),
                    std::clamp<int>(static_cast<int>(rgb.Values[color::INDEX_BLUE]), 0, 255),
                    std::clamp<int>(static_cast<int>(a), 0, 255)
            );
        }


        [[maybe_unused]]
        inline auto Conv1DHorizontal(
                const Canvas & src,
                Canvas & dst,
                const Kernel1D & kernel) -> decltype(dst) {
            auto rgb0 = Float<3>{};
            auto a0 = 0.0f;

            for (auto y = 0; y < dst.Height; y += 1) {
                for (auto x = 0; x < dst.Width; x += 1) {
                    const auto u0 = x - kernel.Size / 2;
                    const auto v = y;

                    auto rgb = Float<3>{};
                    auto a = 0.0f;

                    for (auto i = 0; i < kernel.Size; i += 1) {
                        const auto u = u0 + i;
                        if (not src.IsWithinBounds(u, v)) {
                            continue;
                        }

                        UnpackPixel(src.Pixel(u, v), rgb0, a0);
                        rgb0 *= a0 / 255.0f * kernel.Values[i];
                        rgb += rgb0;

                        a += a0 * kernel.Values[i];
                    }

                    rgb *= 255.0f / a;
                    dst.BlendPixel<color::Overwrite>(x, y, PackPixel(rgb, a));
                }
            }

            return dst;
        }


        [[maybe_unused]]
        inline auto Conv1DVertical(
                const Canvas & src,
                Canvas & dst,
                const Kernel1D & kernel) -> decltype(dst) {
            auto rgb0 = Float<3>{};
            auto a0 = 0.0f;

            for (auto y = 0; y < dst.Height; y += 1) {
                for (auto x = 0; x < dst.Width; x += 1) {
                    const auto v0 = y - kernel.Size / 2;
                    const auto u = x;

                    auto rgb = Float<3>{};
                    auto a = 0.0f;

                    for (auto i = 0; i < kernel.Size; i += 1) {
                        const auto v = v0 + i;
                        if (not src.IsWithinBounds(u, v)) {
                            continue;
                        }

                        UnpackPixel(src.Pixel(u, v), rgb0, a0);
                        rgb0 *= a0 / 255.0f * kernel.Values[i];
                        rgb += rgb0;

                        a += a0 * kernel.Values[i];
                    }

                    rgb *= 255.0f / a;
                    dst.BlendPixel<color::Overwrite>(x, y, PackPixel(rgb, a));
                }
            }

            return dst;
        }
    }


    namespace transform {
        template<color::BlendType BlendFn>
        [[maybe_unused]]
        inline auto Copy(
                const Canvas & src,
                Canvas & dst,
                int x0,
                int y0,
                int x1,
                int y1) -> decltype(dst) {
            if (src.Empty()) {
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

            const auto[ax, ay] = math::ApplyRotation(src_left, src_top, sin, cos);
            const auto[bx, by] = math::ApplyRotation(src_right, src_top, sin, cos);
            const auto[cx, cy] = math::ApplyRotation(src_right, src_bottom, sin, cos);
            const auto[dx, dy] = math::ApplyRotation(src_left, src_bottom, sin, cos);

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
                    auto[u, v] = math::ApplyRotation(x - x0, y - y0, -sin, cos);
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
                float scale_x,
                float scale_y) -> decltype(dst) {
            const auto sin = std::sin(rotation);
            const auto cos = std::cos(rotation);

            const auto src_left = static_cast<int>(static_cast<float >(-u0) * scale_x);
            const auto src_top = static_cast<int>(static_cast<float >(-v0) * scale_y);
            const auto src_right = src_left + static_cast<int>(static_cast<float >(src.Width) * scale_x);
            const auto src_bottom = src_top + static_cast<int>(static_cast<float >(src.Height) * scale_y);

            const auto[ax, ay] = math::ApplyRotation(src_left, src_top, sin, cos);
            const auto[bx, by] = math::ApplyRotation(src_right, src_top, sin, cos);
            const auto[cx, cy] = math::ApplyRotation(src_right, src_bottom, sin, cos);
            const auto[dx, dy] = math::ApplyRotation(src_left, src_bottom, sin, cos);

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
                    auto[u, v] = math::ApplyRotation(x - x0, y - y0, -sin, cos);

                    u = u0 + static_cast<int>(static_cast<float>(u) / scale_x);
                    v = v0 + static_cast<int>(static_cast<float>(v) / scale_y);

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
                return Copy<BlendFn>(
                        src,
                        dst,
                        x0 - static_cast<int>(static_cast<float >(tf.OriginX) * tf.ScaleX),
                        y0 - static_cast<int>(static_cast<float >(tf.OriginY) * tf.ScaleY),
                        x0 + static_cast<int>(static_cast<float >(src.Width - tf.OriginX) * tf.ScaleX),
                        y0 + static_cast<int>(static_cast<float >(src.Height - tf.OriginY) * tf.ScaleY)
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
                    tf.ScaleX,
                    tf.ScaleY
            );
        }
    }


    namespace drawing {
        template<color::BlendType BlendFn = color::Overwrite>
        [[maybe_unused]]
        inline auto Fill(
                Canvas & canvas,
                uint32_t color) -> decltype(canvas) {
            for (auto y = 0; y < canvas.Height; y += 1) {
                for (auto x = 0; x < canvas.Width; x += 1) {
                    canvas.BlendPixel<BlendFn>(x, y, color);
                }
            }

            return canvas;
        }


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


    namespace postprocessing {


#if 0
        [[maybe_unused]]
        inline auto Conv2D(
                const Canvas & src,
                const Kernel2D & kernel) -> uint32_t {
            return 0;
        }
#endif


        [[maybe_unused]]
        inline auto GaussianBlur(
                const Canvas & src,
                Canvas & dst,
                const math::Kernel1D & kernel) -> decltype(dst) {
            auto intermediate = PixelBufferPool::Default().BorrowCanvas(src.Width, src.Height);

            Conv1DHorizontal(src, intermediate.Canvas, kernel);
            return math::Conv1DVertical(intermediate.Canvas, dst, kernel);
        }


        [[maybe_unused]]
        inline auto MaxChannel(uint32_t pixel) -> float {
            const auto[r, g, b, a] = color::ToRGBA<float>(pixel);
            return std::max({ r, g, b }) * a / 255.0f / 255.0f;
        }


        [[maybe_unused]]
        inline auto Luminance(uint32_t pixel) -> float {
            const auto[r, g, b, a] = color::ToRGBA<float>(pixel);
            return std::sqrt(0.299f * r * r + 0.587f * g * g + 0.114f * b * b) * a / 255.0f / 255.0f;
        }


        using BrightnessFunction = decltype(MaxChannel);


        template<BrightnessFunction Brightness>
        [[maybe_unused]]
        inline auto Grayscale(
                const Canvas & src,
                Canvas & dst) -> decltype(dst) {
            for (auto y = 0; y < src.Height; y += 1) {
                for (auto x = 0; x < src.Width; x += 1) {
                    const auto pixel = src.Pixel(x, y);
                    const auto brightness = static_cast<uint32_t>(255.0f * Brightness(pixel));
                    const auto alpha = (pixel >> color::SHIFT_ALPHA) & 0xFF;

                    dst.BlendPixel(
                            x, y,
                            color::FromRGBA(
                                    brightness,
                                    brightness,
                                    brightness,
                                    alpha
                            )
                    );
                }
            }


            return dst;
        }


        template<BrightnessFunction Brightness>
        inline auto FilterByBrightness(
                const Canvas & src,
                Canvas & dst,
                float brightness_threshold,
                uint32_t fill_dark = color::FromRGBA(0, 0, 0, 0)) -> decltype(dst) {
            for (auto y = 0; y < src.Height; y += 1) {
                for (auto x = 0; x < src.Width; x += 1) {
                    const auto pixel = src.Pixel(x, y);
                    const auto brightness = Brightness(pixel);

                    dst.BlendPixel(x, y, brightness < brightness_threshold ? fill_dark : pixel);
                }
            }

            return dst;
        }


        template<BrightnessFunction Brightness>
        inline auto Bloom(
                const Canvas & src,
                Canvas & dst,
                const math::Kernel1D & blur_kernel,
                float brightness_threshold) -> decltype(dst) {
            auto buffer1 = PixelBufferPool::Default().BorrowCanvas(src.Width, src.Height);
            auto buffer2 = PixelBufferPool::Default().BorrowCanvas(src.Width, src.Height);

            FilterByBrightness<Brightness>(src, buffer1.Canvas, brightness_threshold, color::FromRGBA(0, 0, 0, 255));
            GaussianBlur(buffer1.Canvas, buffer2.Canvas, blur_kernel);

            transform::Copy<color::Overwrite>(src, dst);
            transform::Copy<color::Add>(buffer2.Canvas, dst);

            return dst;
        }


#if 0
        [[maybe_unused]]
        inline auto GaussianBlur(
                const Canvas & src,
                Canvas & dst,
                int kernel_size) -> decltype(dst) {
            return dst;
        }
#endif
    }
}
