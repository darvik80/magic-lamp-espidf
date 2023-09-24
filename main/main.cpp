#include <core/Application.h>
#include <nvs_flash.h>
#include "LedStripService.h"
#include <core/Timer.h>
#include <driver/gpio.h>

#include "AppEvent.h"
#include "IrReceiver.h"
#include "core/system/wifi/WifiService.h"
#include "core/system/mqtt/MqttService.h"
#include "core/system/telemetry/TelemetryService.h"

#ifdef CONFIG_IDF_TARGET_ESP32S3
#define LED_STRIP_PIN 48
#define LED_STRIP_SIZE 1
#else
#define LED_STRIP_PIN 8
#define LED_STRIP_SIZE 1
#endif


class MagicGenerator {
    uint32_t _delay;
public:
    explicit MagicGenerator(uint32_t delay) : _delay(delay) {}

    uint32_t getDelay() const {
        return _delay;
    }

    virtual void generate(LedStrip &ledStrip) = 0;
};

class OffGenerator : public MagicGenerator {
public:
    OffGenerator() : MagicGenerator(1000) {
    }

    void generate(LedStrip &ledStrip) override {
        for (size_t idx = 0; idx < ledStrip.getNumOfPins(); idx++) {
            ledStrip.setColor(idx, idx, LedColor{0, 0, 0});
        }

        ledStrip.refresh();
    }
};

class SingleColorGenerator : public MagicGenerator {
    LedColor _color;
public:
    explicit SingleColorGenerator(const LedColor &color) : MagicGenerator(1000), _color(color) {
    }

    void generate(LedStrip &ledStrip) override {
        ledStrip.setColor(0, ledStrip.getNumOfPins() - 1, _color);

        ledStrip.refresh();
    }
};


class RainbowGenerator : public MagicGenerator {
    size_t _offset{0};
    std::array<LedColor, 4> _shape{};
public:
    RainbowGenerator() : MagicGenerator(1000) {
        _shape[0] = LedColor{0xFF, 0x00, 0x00};
        _shape[1] = LedColor{0x00, 0xFF, 0x00};
        _shape[2] = LedColor{0x00, 0x00, 0xFF};
        _shape[3] = LedColor{0xFF, 0xFF, 0xFF};
    }

    void generate(LedStrip &ledStrip) override {
        auto frameOffset = _offset;
        for (size_t idx = 0; idx < ledStrip.getNumOfPins(); idx++) {
            ledStrip.setColor(idx, idx, _shape[frameOffset]);
            if (++frameOffset >= _shape.size()) {
                frameOffset = 0;
            }
        }

        if (++_offset >= _shape.size()) _offset = 0;
        ledStrip.refresh();
    }
};

class MagicLampApplication
        : public Application<MagicLampApplication>,
          public TEventSubscriber<MagicLampApplication, SystemEventChanged, MagicActionEvent, TimerEvent<AppTid_MagicLamp>, Command, IrReceiverEvent> {
    EspTimer _timer{"led-timer"};
    size_t _genId = 1;
    std::vector<MagicGenerator *> _generators;
public:
    MagicLampApplication() {
        _generators.emplace_back(new OffGenerator());
        _generators.emplace_back(new RainbowGenerator());
        _generators.emplace_back(new SingleColorGenerator(LedColor{.red=255, .green = 0, .blue = 0}));
        _generators.emplace_back(new SingleColorGenerator(LedColor{.red=0, .green = 255, .blue = 0}));
        _generators.emplace_back(new SingleColorGenerator(LedColor{.red=0, .green = 0, .blue = 255}));
    }

    void userSetup() override {
        getRegistry().getEventBus().subscribe(shared_from_this());

        getRegistry().create<TelemetryService>();
        getRegistry().create<WifiService>();
        auto &mqtt = getRegistry().create<MqttService>();
        mqtt.addJsonHandler<MagicActionEvent>("/user/action", MQTT_SUB_RELATIVE);
        mqtt.addJsonHandler<MagicActionEvent>("/action", MQTT_SUB_BROADCAST);
        mqtt.addJsonProcessor<SystemEventChanged>("/user/telemetry");
        mqtt.addJsonProcessor<Telemetry>("/user/telemetry");
        //getRegistry().create<UartConsoleService>();
        getRegistry().create<IrReceiver>((gpio_num_t) 10);

        LedStrip &led1 = getRegistry().create<LedStripService<Service_App_LedStatus, 3, 4>>();
        led1.setColor(0, 1, LedColor{.red=0xff, .green= 0x00, .blue=0x00});
        led1.setColor(2, 3, LedColor{.red=0x00, .green= 0x00, .blue=0x00});
        led1.refresh();

        LedStrip &led2 = getRegistry().create<LedStripService<Service_App_LedCircle, 2, 12>>();
        led2.setColor(0, 11, LedColor{.red=0x00, .green= 0xff, .blue=0x00});
        led2.refresh();
    }

    void onEvent(const MagicActionEvent &action) {
        esp_logi(app, "pin: %d, action: %d", action.pin, action.id);
        if (action.id < _generators.size()) {
            _genId = action.id;
            _timer.fire<AppTid_MagicLamp>(_generators[_genId]->getDelay(), true);
        }
    }

    void onEvent(const IrReceiverEvent &event) {
        esp_logi(app, "ir-recv: addr: 0x%0x, cmd: 0x%0x", event.addr, event.cmd);
        switch (event.cmd) {
            case 0xad52:
                _genId = 0;
                break;
            case 0xe916:
                _genId = 1;
                break;
            case 0xe619:
                _genId = 2;
                break;
            case 0xf20d:
                _genId = 3;
                break;
            case 0xf30c:
                _genId = 4;
                break;
            default:
                return;
        }

        esp_logi(app, "ir-recv: action: %d", _genId);
        LedStrip *led = getRegistry().getService<LedStripService<Service_App_LedCircle, 2, 12>>();
        if (led) {
            _generators[_genId]->generate(*led);
        }
        _timer.fire<AppTid_MagicLamp>(_generators[_genId]->getDelay(), true);
    }

    void onEvent(const TimerEvent<AppTid_MagicLamp> &) {
        LedStrip *led = getRegistry().getService<LedStripService<Service_App_LedCircle, 2, 12>>();
        if (led) {
            _generators[_genId]->generate(*led);
        }
    }

    void onEvent(const SystemEventChanged &msg) {
        LedStrip *led = getRegistry().getService<LedStripService<Service_App_LedStatus, 3, 4>>();
        if (led) {
            switch (msg.status) {
                case SystemStatus::Wifi_Connected:
                    led->setColor(0, 0, 0x00, 0xff, 0x00);
                    break;
                case SystemStatus::Wifi_Disconnected:
                    led->setColor(0, 0, 0xff, 0x00, 0x00);
                    break;
                case SystemStatus::Mqtt_Connected:
                    led->setColor(1, 1, 0x00, 0xff, 0x00);
                    break;
                case SystemStatus::Mqtt_Disconnected:
                    led->setColor(1, 1, 0xff, 0x00, 0x00);
                    break;
                default:
                    break;
            }

            led->refresh();
        }
    }

    void onEvent(const Command &msg) {
        esp_logi(app, "handle cmd: [%s] [%s]", msg.cmd, msg.params);
//#ifdef  CONFIG_BT_ENABLED
//        if (strcasecmp(msg.cmd, "bt") == 0) {
//            if (strcasecmp(msg.params, "scan") == 0) {
//                esp_logi(app, "handle req scan");
//                BTGapDiscoveryRequest req{};
//                getRegistry().getEventBus().send(req);
//            }
//        } else if (strcasecmp(msg.cmd, "bthid") == 0) {
//            if (strncasecmp(msg.params, "conn", 4) == 0 && strlen(msg.params) > 5) {
//                esp_logi(app, "handle hid req conn: %s", msg.params);
//                BTHidConnRequest req{};
//                req.bdAddr.assign(msg.params + 5);
//                getRegistry().getEventBus().send(req);
//            }
//        } else if (strcasecmp(msg.cmd, "btspp") == 0) {
//            if (strncasecmp(msg.params, "conn", 4) == 0 && strlen(msg.params) > 5) {
//                esp_logi(app, "handle spp req conn: %s", msg.params);
//                BTSppConnRequest req{};
//                req.bdAddr.assign(msg.params + 5);
//                getRegistry().getEventBus().send(req);
//            }
//        }
//#endif
    }

};

extern "C" void app_main() {
    esp_logi(mon, "\tfree-heap: %lu", esp_get_free_heap_size());
    esp_logi(mon, "\tstack-watermark: %d", uxTaskGetStackHighWaterMark(nullptr));

    auto app = std::make_shared<MagicLampApplication>();
    app->setup();

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    app->destroy();
}
