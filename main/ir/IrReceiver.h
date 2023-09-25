//
// Created by Ivan Kishchenko on 11/09/2023.
//

#pragma once

#include "core/Registry.h"
#include "AppConfig.h"
#include <driver/rmt_rx.h>

class IrReceiver : public TService<IrReceiver, Service_App_IrReceiver> {
    TaskHandle_t _taskHandle{nullptr};
    rmt_channel_handle_t _rxChan{nullptr};
public:
    void rxCallback();
public:
    explicit IrReceiver(Registry &registry, gpio_num_t pin);
    ~IrReceiver() override;
};
