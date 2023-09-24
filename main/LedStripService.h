//
// Created by Ivan Kishchenko on 20/08/2023.
//

#pragma once
#include <led_strip.h>
#include <core/Logger.h>
#include <core/Registry.h>
#include "AppConfig.h"

#define LED_STRIP_RMT_RES_HZ  (10 * 1000 * 1000)

struct LedColor {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

class LedStrip : public Service {
    Registry &_registry;
public:
    explicit LedStrip(Registry &registry) : _registry(registry) {}

    Registry &getRegistry() override {
        return _registry;
    }

    virtual uint16_t getNumOfPins() = 0;

    virtual void setColor(size_t start, size_t end, const LedColor &color) = 0;

    virtual void setColor(size_t start, size_t end, uint32_t red, uint32_t green, uint32_t blue) = 0;

    virtual void refresh() = 0;
};

template<ServiceSubId id, int pin = 8, uint16_t numOfPins = 1>
class LedStripService : public LedStrip {
    led_strip_handle_t _ledStrip{};
public:
    enum {
        ID = id | (((uint16_t) Sys_User) << 8)
    };
    explicit LedStripService(Registry &registry) : LedStrip(registry) {
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


    uint16_t getNumOfPins() override {
        return numOfPins;
    }

    [[nodiscard]] ServiceId getServiceId() const override {
        return id | (Sys_User << 8);
    }

    void setColor(size_t start, size_t end, const LedColor &color) override {
        setColor(start, end, color.red, color.green, color.blue);
    }

    void setColor(size_t start, size_t end, uint32_t red, uint32_t green, uint32_t blue) override {
        for (size_t idx = start; idx <= end; idx++) {
            ESP_ERROR_CHECK(led_strip_set_pixel(_ledStrip, idx, red, green, blue));
        }
    }

    void refresh() override {
        ESP_ERROR_CHECK(led_strip_refresh(_ledStrip));
    }
};
