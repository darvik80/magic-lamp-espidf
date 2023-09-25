//
// Created by Ivan Kishchenko on 04/09/2023.
//


#pragma once

#include <core/EventBus.h>
#include <optional>
#include "led/LedColor.h"

extern "C" {
#include <cJSON.h>
}

enum AppTimerId {
    AppTid_MagicLamp = 0x20,
};

enum AppEventId {
    EvtId_MagicAction,
    EvtId_IrReceiver,
};

struct MagicActionEvent : TEvent<EvtId_MagicAction, Sys_User> {
    uint16_t pin{0};
    uint16_t id{0};
    LedColor color{};
};

inline void fromJson(const cJSON *json, MagicActionEvent &action) {
    cJSON *item = json->child;
    while (item) {
        if (!strcmp(item->string, "action-id") && item->type == cJSON_Number) {
            action.id = (uint16_t) item->valuedouble;
        } else if (!strcmp(item->string, "pin") && item->type == cJSON_Number) {
            action.pin = (uint16_t) item->valuedouble;
        } else if (!strcmp(item->string, "color") && item->type == cJSON_Number) {
            sscanf(item->valuestring, "%2" SCNu8 "%2" SCNu8 "%2" SCNu8 , & action.color.red, & action.color.green, & action.color.blue);
        }

        item = item->next;
    }
}

struct IrReceiverEvent : TEvent<EvtId_IrReceiver, Sys_User> {
    uint16_t addr;
    uint16_t cmd;
    bool repeat;
};
