#include "Panel_LTDC.hpp"
#include <stm32f7xx_hal_rcc.h>
#include <algorithm>

namespace lgfx
{
    inline namespace v1
    {
        static void memset_multi(uint8_t* buf,
                                    uint32_t c, size_t size, size_t length)
        {
            if (size == 1 || ((c & 0xFF) == ((c >> 8) & 0xFF)
                && (size == 2 || ((c & 0xFF) == ((c >> 16) & 0xFF)))))
            {
                memset(buf, c, size * length);
                return;
            }

            size_t l = length;
            if (l & ~0xF)
            {
                while ((l >>= 1) & ~0xF);
                ++l;
            }
            size_t len = l * size;
            length = (length * size) - len;
            uint8_t* dst = buf;
            if (size == 2) {
                do { // 2byte speed tweak
                    *(uint16_t*)dst = c;
                    dst += 2;
                } while (--l);
            } else {
                do {
                    size_t i = 0;
                    do {
                        *dst++ = *(((uint8_t*)&c) + i);
                    } while (++i != size);
                } while (--l);
            }
            if (!length) return;
            while (length > len) {
                memcpy(dst, buf, len);
                dst += len;
                length -= len;
                len <<= 1;
            }
            if (length) {
                memcpy(dst, buf, length);
            }
        }

        Panel_LTDC::Panel_LTDC() : Panel_Device()
        {
        }

        bool Panel_LTDC::init(bool use_reset)
        {
            if (_fb == nullptr)
            {
                return false;
            }

            _cfg.panel_width  = _panel_timing.h.active;
            _cfg.panel_height = _panel_timing.v.active;

            _setup_ltdc_clock();
            _init_ltdc();
            _init_ltdc_layer();

            return Panel_Device::init(use_reset);
        }

        color_depth_t Panel_LTDC::setColorDepth(color_depth_t depth)
        {
            _write_bits = 16;
            _read_bits = 16;
            _write_depth = color_depth_t::rgb565_2Byte;
            _read_depth = color_depth_t::rgb565_2Byte;
            return color_depth_t::rgb565_2Byte;
        }

        void Panel_LTDC::setRotation(uint_fast8_t r)
        {
            r &= 7;
            _rotation = r;
            _internal_rotation = ((r + _cfg.offset_rotation) & 3)
                                | ((r & 4) ^ (_cfg.offset_rotation & 4));

            _width  = _cfg.panel_width;
            _height = _cfg.panel_height;
            if (_internal_rotation & 1)
            {
                std::swap(_width, _height);
            }
        }

        void Panel_LTDC::setWindow(uint_fast16_t xs, uint_fast16_t ys,
                                    uint_fast16_t xe, uint_fast16_t ye)
        {
            xs = std::max(0u, std::min<uint_fast16_t>(_width  - 1, xs));
            xe = std::max(0u, std::min<uint_fast16_t>(_width  - 1, xe));
            ys = std::max(0u, std::min<uint_fast16_t>(_height - 1, ys));
            ye = std::max(0u, std::min<uint_fast16_t>(_height - 1, ye));
            _xpos = xs;
            _xs = xs;
            _xe = xe;
            _ypos = ys;
            _ys = ys;
            _ye = ye;
        }

        void Panel_LTDC::writeBlock(uint32_t rawcolor, uint32_t length)
        {
            do {
                uint32_t h = 1;
                auto w = std::min<uint32_t>(length, _xe + 1 - _xpos);
                if (length >= (w << 1) && _xpos == _xs)
                {
                    h = std::min<uint32_t>(length / w, _ye + 1 - _ypos);
                }
                writeFillRectPreclipped(_xpos, _ypos, w, h, rawcolor);
                if ((_xpos += w) <= _xe)
                {
                    return;
                }
                _xpos = _xs;
                if (_ye < (_ypos += h))
                {
                    _ypos = _ys;
                }
                length -= w * h;
            } while (length);
        }

        void Panel_LTDC::writePixels(pixelcopy_t* param, uint32_t length,
                                        bool use_dma)
        {
            uint_fast16_t xs = _xs;
            uint_fast16_t xe = _xe;
            uint_fast16_t ys = _ys;
            uint_fast16_t ye = _ye;
            uint_fast16_t x = _xpos;
            uint_fast16_t y = _ypos;
            const size_t bits = _write_bits;
            auto k = _cfg.panel_width * bits >> 3;

            uint_fast8_t r = _internal_rotation;
            if (!r)
            {
                uint_fast16_t linelength;
                do {
                    linelength = std::min<uint_fast16_t>(xe - x + 1, length);
                    param->fp_copy(&_fb[y * k], x, x + linelength, param);
                    if ((x += linelength) > xe)
                    {
                        x = xs;
                        y = (y != ye) ? (y + 1) : ys;
                    }
                } while (length -= linelength);
                _xpos = x;
                _ypos = y;
            return;
            }

            int_fast16_t ax = 1;
            int_fast16_t ay = 1;
            if ((1u << r) & 0b10010110)
            {
                y = _height - (y + 1);
                ys = _height - (ys + 1);
                ye = _height - (ye + 1);
                ay = -1;
            }
            if (r & 2)
            {
                x = _width - (x + 1);
                xs = _width - (xs + 1);
                xe = _width - (xe + 1);
                ax = -1;
            }
            if (param->no_convert)
            {
                size_t bytes = bits >> 3;
                size_t xw = 1;
                size_t yw = _cfg.panel_width;
                if (r & 1)
                {
                    std::swap(xw, yw);
                }
                size_t idx = y * yw + x * xw;
                auto data = (uint8_t*)param->src_data;
                do {
                    auto dst = &_fb[idx * bytes];
                    size_t b = 0;
                    do {
                        dst[b] = *data++;
                    } while (++b < bytes);
                    if (x != xe)
                    {
                        idx += xw * ax;
                        x += ax;
                    }
                    else
                    {
                        x = xs;
                        y = (y != ye) ? (y + ay) : ys;
                        idx = y * yw + x * xw;
                    }
                } while (--length);
            }
            else
            {
                if (r & 1)
                {
                    do {
                        param->fp_copy(&_fb[x * k], y, y + 1, param); /// xとyを入れ替えて処理する;
                        if (x != xe)
                        {
                            x += ax;
                        }
                        else
                        {
                            x = xs;
                            y = (y != ye) ? (y + ay) : ys;
                        }
                    } while (--length);
                }
                else
                {
                    do {
                        param->fp_copy(&_fb[y * k], x, x + 1, param);
                        if (x != xe)
                        {
                            x += ax;
                        }
                        else
                        {
                            x = xs;
                            y = (y != ye) ? (y + ay) : ys;
                        }
                    } while (--length);
                }
            }
            if ((1u << r) & 0b10010110)
            {
                y = _height - (y + 1);
            }
            if (r & 2)
            {
                x = _width  - (x + 1);
            }
            _xpos = x;
            _ypos = y;
        }

        void Panel_LTDC::drawPixelPreclipped(uint_fast16_t x, uint_fast16_t y,
                                                uint32_t rawcolor)
        {
            uint_fast8_t r = _internal_rotation;
            if (r)
            {
                if ((1u << r) & 0b10010110)
                {
                    y = _height - (y + 1);
                }
                if (r & 2)
                {
                    x = _width - (x + 1);
                }
                if (r & 1)
                {
                    std::swap(x, y);
                }
            }
            size_t bw = _cfg.panel_width;
            size_t index = x + y * bw;
            {
                auto img = &((rgb565_t*)_fb)[index];
                *img = rawcolor;
            }

            if (!getStartCount())
            {
                display(x, y, 1, 1);
            }
        }

        void Panel_LTDC::writeFillRectPreclipped(
                            uint_fast16_t x, uint_fast16_t y,
                            uint_fast16_t w, uint_fast16_t h, uint32_t rawcolor)
        {
            uint_fast8_t r = _internal_rotation;
            if (r)
            {
                if ((1u << r) & 0b10010110)
                {
                    y = _height - (y + h);
                }
                if (r & 2)
                {
                    x = _width  - (x + w);
                }
                if (r & 1)
                {
                    std::swap(x, y);
                    std::swap(w, h);
                }
            }
            if (w > 1)
            {
                uint_fast8_t bytes = 2;
                uint_fast16_t bw = _cfg.panel_width;
                uint8_t* dst = &_fb[(x + y * bw) * bytes];
                uint8_t* src = dst;
                uint_fast16_t add_dst = bw * bytes;
                uint_fast16_t len = w * bytes;

                if (w != bw)
                {
                    dst += add_dst;
                }
                else
                {
                    w *= h;
                    h = 1;
                }
                memset_multi(src, rawcolor, bytes, w);
                while (--h)
                {
                    memcpy(dst, src, len);
                    dst += add_dst;
                }
            }
            else
            {
                size_t bw = _cfg.panel_width;
                size_t index = x + y * bw;
                {
                    auto img = &((rgb565_t*)_fb)[index];
                    do {
                        *img = rawcolor; img += bw;
                    } while (--h);
                }
            }
        }

        void Panel_LTDC::writeImage(uint_fast16_t x, uint_fast16_t y,
                                    uint_fast16_t w, uint_fast16_t h,
                                    pixelcopy_t* param, bool use_dma)
        {
            uint_fast8_t r = _internal_rotation;
            if (r == 0 &&
                param->transp == pixelcopy_t::NON_TRANSP && param->no_convert)
            {
                auto sx = param->src_x;
                auto bits = param->src_bits;

                auto bw = _cfg.panel_width * bits >> 3;
                auto dst = _fb + (bw * y);
                auto sw = param->src_bitwidth * bits >> 3;
                auto src = &((uint8_t*)param->src_data)[param->src_y * sw];
                y = 0;
                dst +=  x * bits >> 3;
                src += sx * bits >> 3;
                w    =  w * bits >> 3;
                do {
                    memcpy(&dst[y * bw], &src[y * sw], w);
                } while (++y != h);
                return;
            }

            uint32_t nextx = 0;
            uint32_t nexty = 1 << pixelcopy_t::FP_SCALE;
            if (r)
            {
                _rotate_pixelcopy(x, y, w, h, param, nextx, nexty);
            }
            uint32_t sx32 = param->src_x32;
            uint32_t sy32 = param->src_y32;

            y *= _cfg.panel_width;
            do {
                int32_t pos = x + y;
                int32_t end = pos + w;
                while (end != (pos = param->fp_copy(_fb, pos, end, param))
                    &&  end != (pos = param->fp_skip(pos, end, param)));
                param->src_x32 = (sx32 += nextx);
                param->src_y32 = (sy32 += nexty);
                y += _cfg.panel_width;
            } while (--h);
        }

        void Panel_LTDC::readRect(uint_fast16_t x, uint_fast16_t y,
                                    uint_fast16_t w, uint_fast16_t h,
                                    void* dst, pixelcopy_t* param)
        {
            uint_fast8_t r = _internal_rotation;
            if (0 == r && param->no_convert)
            {
                h += y;
                auto bytes = _write_bits >> 3;
                auto bw = _cfg.panel_width;
                auto d = (uint8_t*)dst;
                w *= bytes;
                do {
                    memcpy(d, &_fb[(x + y * bw) * bytes], w);
                    d += w;
                } while (++y != h);
            }
            else
            {
                param->src_bitwidth = _cfg.panel_width;
                param->src_data = _fb;
                uint32_t nextx = 0;
                uint32_t nexty = 1 << pixelcopy_t::FP_SCALE;
                if (r)
                {
                    uint32_t addx = param->src_x32_add;
                    uint32_t addy = param->src_y32_add;
                    uint_fast8_t rb = 1 << r;
                    if (rb & 0b10010110) // case 1:2:4:7:
                    {
                        nexty = -(int32_t)nexty;
                        y = _height - (y + 1);
                    }
                    if (r & 2)
                    {
                        addx = -(int32_t)addx;
                        x = _width - (x + 1);
                    }
                    if ((r + 1) & 2)
                    {
                        addy  = -(int32_t)addy;
                    }
                    if (r & 1)
                    {
                        std::swap(x, y);
                        std::swap(addx, addy);
                        std::swap(nextx, nexty);
                    }
                    param->src_x32_add = addx;
                    param->src_y32_add = addy;
                }
                size_t dstindex = 0;
                uint32_t x32 = x << pixelcopy_t::FP_SCALE;
                uint32_t y32 = y << pixelcopy_t::FP_SCALE;
                param->src_x32 = x32;
                param->src_y32 = y32;
                do {
                    param->src_x32 = x32;
                    x32 += nextx;
                    param->src_y32 = y32;
                    y32 += nexty;
                    dstindex = param->fp_copy(dst, dstindex, dstindex + w, param);
                } while (--h);
            }
        }

        void Panel_LTDC::_rotate_pixelcopy(uint_fast16_t& x, uint_fast16_t& y,
                                            uint_fast16_t& w, uint_fast16_t& h,
                                            pixelcopy_t* param,
                                            uint32_t& nextx, uint32_t& nexty)
        {
            uint32_t addx = param->src_x32_add;
            uint32_t addy = param->src_y32_add;
            uint_fast8_t r = _internal_rotation;
            uint_fast8_t bitr = 1u << r;
            if (bitr & 0b10010110) // case 1:2:4:7:
            {
                param->src_y32 += nexty * (h - 1);
                nexty = -(int32_t)nexty;
                y = _height - (y + h);
            }
            if (r & 2)
            {
                param->src_x32 += addx * (w - 1);
                param->src_y32 += addy * (w - 1);
                addx = -(int32_t)addx;
                addy = -(int32_t)addy;
                x = _width - (x + w);
            }
            if (r & 1)
            {
                std::swap(x, y);
                std::swap(w, h);
                std::swap(nextx, addx);
                std::swap(nexty, addy);
            }
            param->src_x32_add = addx;
            param->src_y32_add = addy;
        }

        bool Panel_LTDC::_setup_ltdc_clock(void)
        {
            static RCC_PeriphCLKInitTypeDef  periph_clk_init_struct;

            __HAL_RCC_LTDC_CLK_ENABLE();

            periph_clk_init_struct.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
            periph_clk_init_struct.PLLSAI.PLLSAIN = 192;
            periph_clk_init_struct.PLLSAI.PLLSAIR = 5;
            periph_clk_init_struct.PLLSAIDivR = RCC_PLLSAIDIVR_4;
            return HAL_RCCEx_PeriphCLKConfig(&periph_clk_init_struct) == HAL_OK;
        }

        bool Panel_LTDC::_init_ltdc(void)
        {
            HAL_StatusTypeDef ret;

            /* LCD panel sync timing settings */
            _ltdc.Init.HorizontalSync     = _panel_timing.h.sync - 1;
            _ltdc.Init.VerticalSync       = _panel_timing.v.sync - 1;
            _ltdc.Init.AccumulatedHBP     = _panel_timing.h.sync \
                                            + _panel_timing.h.back_porch - 1;
            _ltdc.Init.AccumulatedVBP     = _panel_timing.v.sync \
                                            + _panel_timing.v.back_porch - 1;
            _ltdc.Init.AccumulatedActiveH = _panel_timing.v.active \
                                            + _panel_timing.v.sync \
                                            + _panel_timing.v.back_porch - 1;
            _ltdc.Init.AccumulatedActiveW = _panel_timing.h.active \
                                            + _panel_timing.h.sync \
                                            + _panel_timing.h.back_porch - 1;
            _ltdc.Init.TotalHeigh         = _panel_timing.v.active \
                                            + _panel_timing.v.sync \
                                            + _panel_timing.v.back_porch \
                                            + _panel_timing.v.front_porch - 1;
            _ltdc.Init.TotalWidth         = _panel_timing.h.active \
                                            + _panel_timing.h.sync \
                                            + _panel_timing.h.back_porch \
                                            + _panel_timing.h.front_porch - 1;

            /* Initialize the LCD pixel width and pixel height */
            _ltdc.LayerCfg->ImageWidth  = _panel_timing.h.active;
            _ltdc.LayerCfg->ImageHeight = _panel_timing.v.active;

            /* Background value */
            _ltdc.Init.Backcolor.Blue  = 0;
            _ltdc.Init.Backcolor.Green = 0;
            _ltdc.Init.Backcolor.Red   = 0;

            /* Polarity */
            _ltdc.Init.HSPolarity = LTDC_HSPOLARITY_AL;
            _ltdc.Init.VSPolarity = LTDC_VSPOLARITY_AL;
            _ltdc.Init.DEPolarity = LTDC_DEPOLARITY_AL;
            _ltdc.Init.PCPolarity = LTDC_PCPOLARITY_IPC;

            _ltdc.Instance = LTDC;
            return HAL_LTDC_Init(&_ltdc) == HAL_OK;
        }

        bool Panel_LTDC::_init_ltdc_layer(void)
        {
            LTDC_LayerCfgTypeDef layer_cfg;

            layer_cfg.WindowX0 = 0;
            layer_cfg.WindowX1 = _panel_timing.h.active;
            layer_cfg.WindowY0 = 0;
            layer_cfg.WindowY1 = _panel_timing.v.active;
            layer_cfg.PixelFormat = LTDC_PIXEL_FORMAT_RGB565;
            layer_cfg.FBStartAdress = (uint32_t)_fb;
            layer_cfg.Alpha = 255;
            layer_cfg.Alpha0 = 0;
            layer_cfg.Backcolor.Blue = 0;
            layer_cfg.Backcolor.Green = 0;
            layer_cfg.Backcolor.Red = 0;
            layer_cfg.BlendingFactor1 = LTDC_BLENDING_FACTOR1_PAxCA;
            layer_cfg.BlendingFactor2 = LTDC_BLENDING_FACTOR2_PAxCA;
            layer_cfg.ImageWidth = _panel_timing.h.active;
            layer_cfg.ImageHeight = _panel_timing.v.active;

            return HAL_LTDC_ConfigLayer(&_ltdc, &layer_cfg, 0) == HAL_OK;
        }
    }
}
