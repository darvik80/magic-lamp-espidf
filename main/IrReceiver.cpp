//
// Created by Ivan Kishchenko on 11/09/2023.
//

#include "IrReceiver.h"
#include "AppEvent.h"

static bool rmtRxDoneCallback(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *edata, void *user_data) {
    BaseType_t high_task_wakeup = pdFALSE;
    auto receive_queue = (QueueHandle_t) user_data;
    // send the received RMT symbols to the parser task
    xQueueSendFromISR(receive_queue, edata, &high_task_wakeup);
    // return whether any task is woken up
    return high_task_wakeup == pdTRUE;
}

#define IR_NEC_DECODE_MARGIN 200     // Tolerance for parsing RMT symbols into bit stream

#define NEC_LEADING_CODE_DURATION_0  9000
#define NEC_LEADING_CODE_DURATION_1  4500
#define NEC_PAYLOAD_ZERO_DURATION_0  560
#define NEC_PAYLOAD_ZERO_DURATION_1  560
#define NEC_PAYLOAD_ONE_DURATION_0   560
#define NEC_PAYLOAD_ONE_DURATION_1   1690
#define NEC_REPEAT_CODE_DURATION_0   9000
#define NEC_REPEAT_CODE_DURATION_1   2250


class NecCodec {
    uint16_t _codeAddress{0};
    uint16_t _codeCommand{0};
private:
    static inline bool checkInRange(uint32_t signal_duration, uint32_t spec_duration) {
        return (signal_duration < (spec_duration + IR_NEC_DECODE_MARGIN)) &&
               (signal_duration > (spec_duration - IR_NEC_DECODE_MARGIN));
    }

    static bool parseLogic0(rmt_symbol_word_t *rmt_nec_symbols) {
        return checkInRange(rmt_nec_symbols->duration0, NEC_PAYLOAD_ZERO_DURATION_0) &&
               checkInRange(rmt_nec_symbols->duration1, NEC_PAYLOAD_ZERO_DURATION_1);
    }

    static bool parseLogic1(rmt_symbol_word_t *rmt_nec_symbols) {
        return checkInRange(rmt_nec_symbols->duration0, NEC_PAYLOAD_ONE_DURATION_0) &&
               checkInRange(rmt_nec_symbols->duration1, NEC_PAYLOAD_ONE_DURATION_1);
    }

public:
    bool parseFrameRepeat(rmt_symbol_word_t *rmt_nec_symbols) {
        return checkInRange(rmt_nec_symbols->duration0, NEC_REPEAT_CODE_DURATION_0) &&
                checkInRange(rmt_nec_symbols->duration1, NEC_REPEAT_CODE_DURATION_1);
    }

    bool parseFrame(rmt_symbol_word_t *rmt_nec_symbols) {
        rmt_symbol_word_t *cur = rmt_nec_symbols;
        uint16_t address = 0;
        uint16_t command = 0;
        bool valid_leading_code = checkInRange(cur->duration0, NEC_LEADING_CODE_DURATION_0) &&
                                  checkInRange(cur->duration1, NEC_LEADING_CODE_DURATION_1);
        if (!valid_leading_code) {
            return false;
        }
        cur++;
        for (int i = 0; i < 16; i++) {
            if (parseLogic1(cur)) {
                address |= 1 << i;
            } else if (parseLogic0(cur)) {
                address &= ~(1 << i);
            } else {
                return false;
            }
            cur++;
        }
        for (int i = 0; i < 16; i++) {
            if (parseLogic1(cur)) {
                command |= 1 << i;
            } else if (parseLogic0(cur)) {
                command &= ~(1 << i);
            } else {
                return false;
            }
            cur++;
        }
        // save address and command
        _codeAddress = address;
        _codeCommand = command;
        return true;
    }

    [[nodiscard]] uint16_t getCodeAddress() const {
        return _codeAddress;
    }

    [[nodiscard]] uint16_t getCodeCommand() const {
        return _codeCommand;
    }
};

static void rxCallback(void *args) {
    ((IrReceiver *) args)->rxCallback();
}

void IrReceiver::rxCallback() {
    QueueHandle_t receive_queue = xQueueCreate(1, sizeof(rmt_rx_done_event_data_t));
    rmt_rx_event_callbacks_t cbs = {
            .on_recv_done = rmtRxDoneCallback,
    };
    ESP_ERROR_CHECK(rmt_rx_register_event_callbacks(_rxChan, &cbs, receive_queue));

    // the following timing requirement is based on NEC protocol
    rmt_receive_config_t receive_config = {
            .signal_range_min_ns = 1250,     // the shortest duration for NEC signal is 560 µs, 1250 ns < 560 µs, valid signal is not treated as noise
            .signal_range_max_ns = 12000000, // the longest duration for NEC signal is 9000 µs, 12000000 ns > 9000 µs, the receive does not stop early
    };

    rmt_symbol_word_t raw_symbols[64]; // 64 symbols should be sufficient for a standard NEC frame
    ESP_ERROR_CHECK(rmt_receive(_rxChan, raw_symbols, sizeof(raw_symbols), &receive_config));
    // wait for the RX-done signal
    rmt_rx_done_event_data_t rx_data;

    NecCodec codec{};
    while (xQueueReceive(receive_queue, &rx_data, portMAX_DELAY)) {
        if (rx_data.num_symbols == 34) {
            if (codec.parseFrame(rx_data.received_symbols)) {
                getBus().post(
                        IrReceiverEvent{
                                .addr = codec.getCodeAddress(),
                                .cmd = codec.getCodeCommand(),
                                .repeat = false
                        }
                );
            }
        } else if (rx_data.num_symbols == 2) {
            if (codec.parseFrameRepeat(rx_data.received_symbols)) {
                getBus().post(
                        IrReceiverEvent{
                                .addr = codec.getCodeAddress(),
                                .cmd = codec.getCodeCommand(),
                                .repeat = true
                        }
                );
            }
        }
        ESP_ERROR_CHECK(rmt_receive(_rxChan, raw_symbols, sizeof(raw_symbols), &receive_config));
    }
}

IrReceiver::IrReceiver(Registry &registry, gpio_num_t pin) : TService(registry) {
    rmt_rx_channel_config_t rx_chan_config = {
            .gpio_num = pin,                    // GPIO number
            .clk_src = RMT_CLK_SRC_DEFAULT,   // select source clock
            .resolution_hz = 1 * 1000 * 1000, // 1 MHz tick resolution, i.e., 1 tick = 1 µs
            .mem_block_symbols = 64,          // memory block size, 64 * 4 = 256 Bytes
            .flags {
                    .invert_in = false,         // do not invert input signal
                    .with_dma = false,          // do not need DMA backend
            }
    };
    ESP_ERROR_CHECK(rmt_new_rx_channel(&rx_chan_config, &_rxChan));
    ESP_ERROR_CHECK(rmt_enable(_rxChan));

    xTaskCreate(::rxCallback, "ir-recv", 4096, this, tskIDLE_PRIORITY, &_taskHandle);
}

IrReceiver::~IrReceiver() {
    vTaskDelete(_taskHandle);
    rmt_disable(_rxChan);
    rmt_del_channel(_rxChan);
}