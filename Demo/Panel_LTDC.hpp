#pragma once

#include <stm32f7xx_hal_ltdc.h>
#include <lgfx/v1/panel/Panel_Device.hpp>

namespace lgfx
{
    inline namespace v1
    {
        struct Panel_LTDC : public Panel_Device
        {
        public:
            struct panel_timing_t
            {
                struct info_t
                {
                    uint16_t sync;
                    uint16_t back_porch;
                    uint16_t active;
                    uint16_t front_porch;
                };
                info_t h;
                info_t v;
            };

            Panel_LTDC();

            bool init(bool use_reset) override;
            void beginTransaction(void) override {}
            void endTransaction(void) override {}

            color_depth_t setColorDepth(color_depth_t depth) override;
            void setRotation(uint_fast8_t r) override;
            void setWindow(uint_fast16_t xs, uint_fast16_t ys, uint_fast16_t xe, uint_fast16_t ye) override;
            void setInvert(bool invert) override {}
            void setSleep(bool flg) override {}
            void setPowerSave(bool flg) override {}

            void waitDisplay(void) override {}
            bool displayBusy(void) override { return false; }

            void writeBlock(uint32_t rawcolor, uint32_t len) override;
            void writePixels(pixelcopy_t* param, uint32_t len, bool use_dma) override;
            void drawPixelPreclipped(uint_fast16_t x, uint_fast16_t y, uint32_t rawcolor) override;
            void writeFillRectPreclipped(uint_fast16_t x, uint_fast16_t y, uint_fast16_t w, uint_fast16_t h, uint32_t rawcolor) override;
            void writeImage(uint_fast16_t x, uint_fast16_t y, uint_fast16_t w, uint_fast16_t h, pixelcopy_t* param, bool use_dma) override;

            uint32_t readCommand(uint_fast8_t cmd, uint_fast8_t index, uint_fast8_t len) override { return 0; }
            uint32_t readData(uint_fast8_t index, uint_fast8_t len) override { return 0; }
            void readRect(uint_fast16_t x, uint_fast16_t y, uint_fast16_t w, uint_fast16_t h, void* dst, pixelcopy_t* param) override;

            void setPanelTiming(const panel_timing_t &param) { _panel_timing = param; }
            void setFrameBuffer(uint8_t * const framebuffer) { _fb = framebuffer; }

        protected:
            LTDC_HandleTypeDef _ltdc;
            panel_timing_t _panel_timing;

            uint8_t * _fb = nullptr;
            int32_t _xpos = 0;
            int32_t _ypos = 0;

            bool _setup_ltdc_clock(void);
            bool _init_ltdc(void);
            bool _init_ltdc_layer(void);
            void _rotate_pixelcopy(uint_fast16_t& x, uint_fast16_t& y, uint_fast16_t& w, uint_fast16_t& h, pixelcopy_t* param, uint32_t& nextx, uint32_t& nexty);
        };
    }
}
