#pragma once

#include "stm32746g_discovery_sdram.h"

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "Panel_LTDC.hpp"

class LGFX_LTDC_STM32F746G_DISCO: public lgfx::LGFX_Device
{
    lgfx::Panel_LTDC _panel_instance;

    public:
    LGFX_LTDC_STM32F746G_DISCO()
    {
        BSP_SDRAM_Init();

        _init_gpios();
        _panel_instance.setFrameBuffer((uint8_t *)SDRAM_DEVICE_ADDR);

        {
            lgfx::Panel_LTDC::panel_timing_t panel_cfg = {
                .h = {
                    .sync        =  41,
                    .back_porch  =  13,
                    .active      = 480,
                    .front_porch =  32,
                },
                .v = {
                    .sync        =  10,
                    .back_porch  =  10,
                    .active      = 272,
                    .front_porch =   2,
                },
            };
            _panel_instance.setPanelTiming(panel_cfg);

            auto cfg = _panel_instance.config();
            cfg.memory_width  = panel_cfg.h.active;
            cfg.memory_height = panel_cfg.v.active;
            _panel_instance.config(cfg);
        }

        setPanel(&_panel_instance);
    }

    private:
    void _init_gpios()
    {
        GPIO_InitTypeDef gpio_init_structure;

        // GPIO clocks
        __HAL_RCC_GPIOE_CLK_ENABLE();
        __HAL_RCC_GPIOG_CLK_ENABLE();
        __HAL_RCC_GPIOI_CLK_ENABLE();
        __HAL_RCC_GPIOJ_CLK_ENABLE();
        __HAL_RCC_GPIOK_CLK_ENABLE();

        /* GPIOE configuration */
        gpio_init_structure.Pin       = GPIO_PIN_4;
        gpio_init_structure.Mode      = GPIO_MODE_AF_PP;
        gpio_init_structure.Pull      = GPIO_NOPULL;
        gpio_init_structure.Speed     = GPIO_SPEED_FAST;
        gpio_init_structure.Alternate = GPIO_AF14_LTDC;
        HAL_GPIO_Init(GPIOE, &gpio_init_structure);

        /* GPIOG configuration */
        gpio_init_structure.Pin       = GPIO_PIN_12;
        gpio_init_structure.Mode      = GPIO_MODE_AF_PP;
        gpio_init_structure.Alternate = GPIO_AF9_LTDC;
        HAL_GPIO_Init(GPIOG, &gpio_init_structure);

        /* GPIOI configuration */
        gpio_init_structure.Pin       = GPIO_PIN_9 | GPIO_PIN_10 \
                                        | GPIO_PIN_13 | GPIO_PIN_14 \
                                        | GPIO_PIN_15;
        gpio_init_structure.Mode      = GPIO_MODE_AF_PP;
        gpio_init_structure.Alternate = GPIO_AF14_LTDC;
        HAL_GPIO_Init(GPIOI, &gpio_init_structure);

        /* GPIOJ configuration */
        gpio_init_structure.Pin       = GPIO_PIN_0 | GPIO_PIN_1 \
                                        | GPIO_PIN_2 | GPIO_PIN_3 \
                                        | GPIO_PIN_4 | GPIO_PIN_5 \
                                        | GPIO_PIN_6 | GPIO_PIN_7 \
                                        | GPIO_PIN_8 | GPIO_PIN_9 \
                                        | GPIO_PIN_10 | GPIO_PIN_11 \
                                        | GPIO_PIN_13 | GPIO_PIN_14 \
                                        | GPIO_PIN_15;
        gpio_init_structure.Mode      = GPIO_MODE_AF_PP;
        gpio_init_structure.Alternate = GPIO_AF14_LTDC;
        HAL_GPIO_Init(GPIOJ, &gpio_init_structure);

        /* GPIOK configuration */
        gpio_init_structure.Pin       = GPIO_PIN_0 | GPIO_PIN_1 \
                                        | GPIO_PIN_2 | GPIO_PIN_4 \
                                        | GPIO_PIN_5 | GPIO_PIN_6 \
                                        | GPIO_PIN_7;
        gpio_init_structure.Mode      = GPIO_MODE_AF_PP;
        gpio_init_structure.Alternate = GPIO_AF14_LTDC;
        HAL_GPIO_Init(GPIOK, &gpio_init_structure);

        // Display enable
        pinMode(PI12, OUTPUT);
        digitalWrite(PI12, HIGH);

        // Backlight enable
        pinMode(PK3, OUTPUT);
        digitalWrite(PK3, HIGH);
    }
};