#include <cstdio>
#include <cstdint>
#include <unistd.h>
#include <cstring>
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

constexpr gpio_num_t MOTOR_IN1_PIN = GPIO_NUM_32;
constexpr gpio_num_t MOTOR_IN2_PIN = GPIO_NUM_33;
constexpr uint32_t INPUT_TIMEOUT_MS = 100;
constexpr uint32_t CONTROL_LOOP_DELAY_MS = 200;
constexpr uint32_t SERIAL_INPUT_DELAY_MS = 10;
constexpr uart_port_t UART_PORT = UART_NUM_0;
constexpr int UART_BAUD = 115200;
constexpr int UART_RX_BUF_SIZE = 256;
constexpr int UART_TX_BUF_SIZE = 256;

enum class MotorDirection : int8_t {
    STOP = 0,
    FORWARD = 1,
    REVERSE = -1
};

class ControlState {
private:
    struct State {
        MotorDirection direction = MotorDirection::STOP;
        uint32_t timestamp_ms = 0;
    } state;

    SemaphoreHandle_t mutex;

public:
    ControlState() {

    }

    void update(MotorDirection direction) {
        state.direction = direction;
        state.timestamp_ms = xTaskGetTickCount() / portTICK_PERIOD_MS;
    }

    MotorDirection get_direction() {
        return state.direction;
    }

    uint32_t get_timestamp() {
        uint32_t ts = 0;
        ts = state.timestamp_ms;

        return ts;
    }

    const char* get_status() const {
        static char status_buffer[256];
        MotorDirection dir = MotorDirection::STOP;
        uint32_t current_time_ms = xTaskGetTickCount() / portTICK_PERIOD_MS;
        uint32_t time_since_input = 0;

        const char* dir_str = "UNKNOWN";
        if (dir == MotorDirection::FORWARD) {
            dir_str = "FORWARD";
        } else if (dir == MotorDirection::REVERSE) {
            dir_str = "REVERSE";
        } else {
            dir_str = "STOP";
        }

        snprintf(status_buffer, sizeof(status_buffer),
                 "[Control] Direction: %s | Time Since Input: %lu ms",
                 dir_str,
                 (unsigned long)time_since_input);
        return status_buffer;
    }
};


class MotorController {
private:
    gpio_num_t pin_in1;
    gpio_num_t pin_in2;
    MotorDirection current_direction = MotorDirection::STOP;

public:
    MotorController(gpio_num_t in1, gpio_num_t in2) : pin_in1(in1), pin_in2(in2) {}

    void init() {
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << pin_in1) | (1ULL << pin_in2),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&io_conf);
    }

    void set_direction(MotorDirection direction) {
        current_direction = direction;
        if (direction == MotorDirection::FORWARD) {
            gpio_set_level(pin_in1, 1);
            gpio_set_level(pin_in2, 0);
        } else if (direction == MotorDirection::REVERSE) {
            gpio_set_level(pin_in1, 0);
            gpio_set_level(pin_in2, 1);
        } else {
            gpio_set_level(pin_in1, 0);
            gpio_set_level(pin_in2, 0);
        }
    }

    void update_motor_control(ControlState* state){
        set_direction(state->get_direction());
    }

    const char* get_status() const {
        static char status_buffer[128];
        const char* dir_str = "UNKNOWN";
        if (current_direction == MotorDirection::FORWARD) {
            dir_str = "FORWARD";
        } else if (current_direction == MotorDirection::REVERSE) {
            dir_str = "REVERSE";
        } else {
            dir_str = "STOP";
        }
        snprintf(status_buffer, sizeof(status_buffer),
                 "[Motor] Direction: %s | IN1: %u | IN2: %u",
                 dir_str,
                 (unsigned)gpio_get_level(pin_in1),
                 (unsigned)gpio_get_level(pin_in2));
        return status_buffer;
    }
};





extern "C" void app_main() {
    int init_cnt = 0;
    while (init_cnt < 5) {
        printf("Wake count: %d\n", init_cnt);
        init_cnt++;
        sleep(1);
    }
    printf("Initializing L298N motor controller (C++)\n");
    MotorController* motor_controller = new MotorController(MOTOR_IN1_PIN, MOTOR_IN2_PIN);
    motor_controller->init();

    printf("Initializing Control State\n");
    ControlState* state = new ControlState();

    printf("Initialization UART\n");
    uart_config_t uart_config = {}; // Zero-initialize every single field first
    uart_config.baud_rate = 115200;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity    = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk = UART_SCLK_DEFAULT;


    uart_driver_install(UART_PORT, UART_RX_BUF_SIZE, UART_TX_BUF_SIZE, 0, NULL, 0);
    uart_param_config(UART_PORT, &uart_config);
    uart_set_pin(UART_PORT, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    printf("UART Driver initialized (UART %d at %d baud)\n", UART_PORT, UART_BAUD);


    while(1){

        uint8_t buffer[64];
        int length = uart_read_bytes(UART_PORT, buffer, sizeof(buffer), pdMS_TO_TICKS(1));
        char c = 0;

        for (int i = 0; i < length; i++) {
            c = static_cast<char>(buffer[i]);
        }

        if(c == 'W' || c == 'w') {
            state->update(MotorDirection::FORWARD);
            printf("Forward!\n");
        }
        else if(c =='S' || c == 's'){
            state->update(MotorDirection::REVERSE);
            printf("Reverse\n");
        }
        else{
            state->update(MotorDirection::STOP);
            printf("Stop\n");
        }
        motor_controller->update_motor_control(state);
        vTaskDelay(pdMS_TO_TICKS(CONTROL_LOOP_DELAY_MS));
    }

}
