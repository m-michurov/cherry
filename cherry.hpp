#pragma once

#include <stdexcept>
#include <limits>
#include <tuple>
#include <algorithm>


namespace cherry {
    template<typename T>
    constexpr inline auto ClampTo(int64_t value) -> T {
        constexpr auto min = static_cast<int64_t>(std::numeric_limits<T>::min());
        constexpr auto max = static_cast<int64_t>(std::numeric_limits<T>::max());

        return static_cast<T>(std::clamp(value, min, max));
    }


    struct ChannelLayout {
        uint32_t OffsetRed{ 0 };
        uint32_t OffsetGreen{ 1 };
        uint32_t OffsetBlue{ 2 };
        uint32_t OffsetAlpha{ 3 };
    };

    constexpr auto LayoutRGBA = ChannelLayout{
            .OffsetRed = 0,
            .OffsetGreen = 1,
            .OffsetBlue = 2,
            .OffsetAlpha = 3
    };


    struct Color {
        uint32_t Blue{ 0 };
        uint32_t Green{ 0 };
        uint32_t Red{ 0 };
        uint32_t Alpha{ 255 };


        inline auto Clamp() -> Color & {
            Blue = ClampTo<uint8_t>(Blue);
            Green = ClampTo<uint8_t>(Green);
            Red = ClampTo<uint8_t>(Red);
            Alpha = ClampTo<uint8_t>(Alpha);

            return *this;
        }


        /**
         * Blend two colors using alpha compositing.
         * This operates on straight alpha colors.
         * <p>
         * alpha_result = alpha_src + alpha_dst * (1.0 - alpha_src)
         * </p>
         * <p>
         * RGB_result = 1.0 / alpha_result * (RGB_src * alpha_src + RGB_dst * alpha_dst * (1.0 - alpha_src))
         * </p>
         */
        [[nodiscard]] inline auto BlendOver(Color dst) const -> Color {
            auto result = Color{};

            result.Alpha = Alpha + dst.Alpha * (255 - Alpha) / 255;

            result.Blue = (Blue * Alpha + dst.Blue * dst.Alpha * (255 - Alpha) / 255) / result.Alpha;
            result.Green = (Green * Alpha + dst.Green * dst.Alpha * (255 - Alpha) / 255) / result.Alpha;
            result.Red = (Red * Alpha + dst.Red * dst.Alpha * (255 - Alpha) / 255) / result.Alpha;

            return result.Clamp();
        }
    };


    struct Vec2 {
        int64_t X{ 0 };
        int64_t Y{ 0 };
    };


    struct Box {
        int64_t Left{ 0 };
        int64_t Top{ 0 };
        int64_t Right{ 0 };
        int64_t Bottom{ 0 };


        inline auto SortBounds() -> Box & {
            std::tie(Left, Right) = std::tuple(std::min(Left, Right), std::max(Left, Right));
            std::tie(Top, Bottom) = std::tuple(std::min(Top, Bottom), std::max(Top, Bottom));

            return *this;
        }


        [[nodiscard]] inline auto Width() const -> int64_t {
            return Right - Left;
        }


        [[nodiscard]] inline auto Height() const -> int64_t {
            return Bottom - Top;
        }
    };


    enum PixelBlendMode {
        OVERWRITE,
        ALPHA_COMPOSITING
    };


    class Canvas {
        static constexpr auto CHANNELS = 4;


    public:
        [[maybe_unused]] Canvas(
                uint8_t * data,
                int64_t width,
                int64_t height,
                int64_t stride,
                PixelBlendMode mode = PixelBlendMode::OVERWRITE)
                :
                blend_mode(mode),
                BlendPixelFn(nullptr),
                data_bytes_mutable_view(data),
                width_pixels(width),
                height_pixels(height),
                stride_bytes(stride * CHANNELS),
                stride_pixels(stride) {
            if (width < 0) {
                throw std::out_of_range("Canvas width must be positive");
            }

            if (height < 0) {
                throw std::out_of_range("Canvas height must be positive");
            }

            if (stride < 0) {
                throw std::out_of_range("Canvas stride must be positive");
            }

            SetBlendMode(mode);
        }


        [[nodiscard]] inline auto Data() const -> uint8_t const * {
            return data_bytes_mutable_view;
        }


        inline auto SetChannelLayout(ChannelLayout new_layout) -> Canvas & {
            layout = new_layout;

            return *this;
        }


        [[nodiscard]] inline auto BlendMode() const -> PixelBlendMode {
            return blend_mode;
        }


        inline auto SetBlendMode(PixelBlendMode mode) -> Canvas & {
            blend_mode = mode;

            switch (mode) {
                case OVERWRITE:
                    BlendPixelFn = BlendOverwrite;
                    break;
                case ALPHA_COMPOSITING:
                    BlendPixelFn = BlendAlpha;
                    break;
            }

            return *this;
        }


        [[nodiscard]] inline auto Width() const -> uint32_t {
            return width_pixels;
        }


        [[nodiscard]] inline auto Height() const -> uint32_t {
            return height_pixels;
        }


        inline auto SubCanvas(Box box) -> Canvas {
            box.SortBounds();

            CheckBounds(box.Left, box.Top);
            CheckBounds(box.Right - 1, box.Bottom - 1);

            return Canvas{
                    data_bytes_mutable_view + stride_bytes * box.Top + CHANNELS * box.Left,
                    ClampTo<uint32_t>(box.Width()),
                    ClampTo<uint32_t>(box.Height()),
                    ClampTo<uint32_t>(stride_pixels),
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
                Vec2 point,
                Color color) -> Canvas & {
            CheckBounds(point.X, point.Y);

            BlendPixelFn(*this, point.X, point.Y, color);

            return *this;
        }


        [[nodiscard]] inline auto Pixel(Vec2 point) const -> Color {
            const auto[x, y] = point;

            return GetPixel(x, y);
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


        inline auto Fill(Color color) -> Canvas & {
            for (auto y = 0; y < Height(); y += 1) {
                for (auto x = 0; x < Width(); x += 1) {
                    BlendPixel({ x, y }, color);
                }
            }

            return *this;
        }


        inline auto FillRectangle(
                Box box,
                Color color) -> Canvas & {
            SubCanvas(box).Fill(color);

            return *this;
        }


        inline auto Line(
                Vec2 p0,
                Vec2 p1,
                Color color) -> Canvas & {
            const auto[x0, y0] = p0;
            const auto[x1, y1] = p1;

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


        inline auto CopyInto(
                const Canvas & src,
                Box box) -> Canvas & {
            auto target = SubCanvas(box);

            const auto dx = (box.Width() > 0) ? 1 : -1;
            const auto dy = (box.Height() > 0) ? 1 : -1;

            for (auto y = 0; y < target.Height(); y += 1) {
                for (auto x = 0; x < target.Width(); x += 1) {
                    auto src_x = x * src.Width() / target.Width();
                    if (dx < 0) {
                        src_x = src.Width() - src_x - 1;
                    }

                    auto src_y = y * src.Height() / target.Height();
                    if (dy < 0) {
                        src_y = src.Height() - src_y - 1;
                    }


                    target.BlendPixel({ x, y }, src.Pixel({ src_x, src_y }));
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
                Color color) -> void {
            const auto dx = x1 - x0;
            const auto[dy, yi] = ((y1 - y0) >= 0) ? std::tuple(y1 - y0, 1) : std::tuple(y0 - y1, -1);

            auto D = 2 * dy - dx;
            auto y = y0;

            for (auto x = x0; x <= x1; x += 1) {
                BlendPixel({ x, y }, color);

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
                Color color) -> void {
            const auto[dx, xi] = ((x1 - x0) >= 0) ? std::tuple(x1 - x0, 1) : std::tuple(x0 - x1, -1);
            const auto dy = y1 - y0;

            auto D = 2 * dx - dy;
            auto x = x0;

            for (auto y = y0; y <= y1; y += 1) {
                BlendPixel({ x, y }, color);

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
            return x >= 0 and y >= 0 and x < width_pixels and y < height_pixels;
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
                    + std::to_string(width_pixels) + ", " + std::to_string(height_pixels) + ")";
            throw std::out_of_range(message);
#endif // CHERRY_CHECK_BOUNDS
        }


        inline auto SetPixel(
                int64_t x,
                int64_t y,
                Color color) -> Canvas & {
            CheckBounds(x, y);

            data_bytes_mutable_view[stride_bytes * y + CHANNELS * x + layout.OffsetBlue] = color.Blue;
            data_bytes_mutable_view[stride_bytes * y + CHANNELS * x + layout.OffsetGreen] = color.Green;
            data_bytes_mutable_view[stride_bytes * y + CHANNELS * x + layout.OffsetRed] = color.Red;
            data_bytes_mutable_view[stride_bytes * y + CHANNELS * x + layout.OffsetAlpha] = color.Alpha;

            return *this;
        }


        [[nodiscard]] inline auto GetPixel(
                int64_t x,
                int64_t y) const -> Color {
            CheckBounds(x, y);

            return {
                    .Blue = data_bytes_mutable_view[stride_bytes * y + CHANNELS * x + layout.OffsetBlue],
                    .Green = data_bytes_mutable_view[stride_bytes * y + CHANNELS * x + layout.OffsetGreen],
                    .Red = data_bytes_mutable_view[stride_bytes * y + CHANNELS * x + layout.OffsetRed],
                    .Alpha = data_bytes_mutable_view[stride_bytes * y + CHANNELS * x + layout.OffsetAlpha],
            };
        }


        static inline auto BlendOverwrite(
                Canvas & canvas,
                int64_t x,
                int64_t y,
                Color color) -> void {
            canvas.SetPixel(x, y, color);
        }


        static inline auto BlendAlpha(
                Canvas & canvas,
                int64_t x,
                int64_t y,
                Color color) -> void {
            const auto new_color = color.BlendOver(canvas.Pixel({ x, y }));
            canvas.SetPixel(x, y, new_color);
        }


        PixelBlendMode blend_mode;
        std::function<void(
                Canvas &,
                int64_t,
                int64_t,
                Color)> BlendPixelFn;

        ChannelLayout layout{ LayoutRGBA };

        uint8_t * const data_bytes_mutable_view{ nullptr };
        const int64_t width_pixels{ 0 };
        const int64_t height_pixels{ 0 };
        const int64_t stride_bytes{ 0 };
        const int64_t stride_pixels{ 0 };

#ifdef CHERRY_NAMED_CANVAS
        std::string name;
#endif // CHERRY_NAMED_CANVAS
    };
}
