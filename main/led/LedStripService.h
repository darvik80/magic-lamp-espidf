//
// Created by Ivan Kishchenko on 20/08/2023.
//

#pragma once

#include <led_strip.h>
#include "core/Registry.h"
#include "AppConfig.h"
#include "LedColor.h"
#include "core/system/SystemService.h"

#define LED_STRIP_RMT_RES_HZ  (10 * 1000 * 1000)

class LedStrip {
public:
    virtual uint16_t getNumOfPins() = 0;

    virtual void setColor(size_t start, size_t end, const LedColor &color) = 0;

    virtual void setColor(size_t start, size_t end, uint32_t red, uint32_t green, uint32_t blue) = 0;

    virtual void refresh() = 0;
};

template<ServiceSubId id, int pin = 8, uint16_t numOfPins = 1>
class LedStripService : public TService<LedStripService<id, pin, numOfPins>,  id, Sys_User>, public LedStrip {
    led_strip_handle_t _ledStrip{};
public:
    explicit LedStripService(Registry &registry) : TService<LedStripService<id, pin, numOfPins>, id, Sys_User>(registry) {
        led_strip_config_t strip_config = {
                .strip_gpio_num = pin,   // The GPIO that connected to the LED strip's data line
                .max_leds = numOfPins,        // The number of LEDs in the strip,
                .led_pixel_format = LED_PIXEL_FORMAT_GRB, // Pixel format of your LED strip
                .led_model = LED_MODEL_WS2812,            // LED strip model
                .flags {
                        .invert_out = false,                // whether to invert the output signal
                }
        };

        led_strip_rmt_config_t rmt_config = {
                .clk_src = RMT_CLK_SRC_DEFAULT,        // different clock source can lead to different power consumption
                .resolution_hz = LED_STRIP_RMT_RES_HZ, // RMT counter clock frequency
                .mem_block_symbols = 0,
                .flags {
                        .with_dma = false,               // DMA feature is available on ESP target like ESP32-S3
                }
        };

        ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &_ledStrip));
        esp_logi(led, "Created LED strip object with RMT backend");
    }

    [[nodiscard]] std::string_view getServiceName() const override {
        return "led";
    }

    uint16_t getNumOfPins() {
        return numOfPins;
    }

    void setColor(size_t start, size_t end, const LedColor &color) {
        setColor(start, end, color.red, color.green, color.blue);
    }

    void setColor(size_t start, size_t end, uint32_t red, uint32_t green, uint32_t blue) {
        for (size_t idx = start; idx <= end; idx++) {
            ESP_ERROR_CHECK(led_strip_set_pixel(_ledStrip, idx, red, green, blue));
        }
    }

    void refresh() {
        ESP_ERROR_CHECK(led_strip_refresh(_ledStrip));
    }


    ~LedStripService()  {
        led_strip_del(_ledStrip);
    }
};
