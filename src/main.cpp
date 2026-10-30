#include <cstdio>
#include <cstdint>
#include <unistd.h>
#include <iostream>
#include <cstring>
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/uart.h"

#define BUF_SIZE (1024)

constexpr gpio_num_t MOTOR_IN1_PIN = GPIO_NUM_32;
constexpr gpio_num_t MOTOR_IN2_PIN = GPIO_NUM_33;
constexpr uint32_t INPUT_TIMEOUT_MS = 100;
constexpr uint32_t CONTROL_LOOP_DELAY_MS = 10;
constexpr uint32_t SERIAL_INPUT_DELAY_MS = 2;

enum class MotorDirection : int8_t {
    STOP = 0,
    FORWARD = 1,
    REVERSE = -1
};

class MotorController {
private:
    gpio_num_t pin_in1;
    gpio_num_t pin_in2;

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

    void status(){

        int pin_32_state = gpio_get_level(MOTOR_IN1_PIN);
        int pin_33_state = gpio_get_level(MOTOR_IN2_PIN);

        printf("===============================MOTOR CONTROLLER================================== \n");
        printf("PIN 32 %d; PIN 33 %d \n", pin_32_state, pin_33_state);
        printf("================================================================================= \n");
    }
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
        mutex = xSemaphoreCreateMutex();
    }

    ~ControlState() {
        if (mutex != NULL) {
            vSemaphoreDelete(mutex);
        }
    }

    void update(MotorDirection direction) {
        if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1)) == pdTRUE) {
            state.direction = direction;
            state.timestamp_ms = xTaskGetTickCount() / portTICK_PERIOD_MS;
            xSemaphoreGive(mutex);
        }
    }

    MotorDirection get_direction() {
        MotorDirection dir = MotorDirection::STOP;
        if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1)) == pdTRUE) {
            uint32_t current_time_ms = xTaskGetTickCount() / portTICK_PERIOD_MS;
            uint32_t time_since_input = current_time_ms - state.timestamp_ms;

            if (time_since_input > INPUT_TIMEOUT_MS) {
                dir = MotorDirection::STOP;
            } else {
                dir = state.direction;
            }
            xSemaphoreGive(mutex);
        }
        return dir;
    }
};

static MotorController* motor_controller = nullptr;
static ControlState* control_state = nullptr;

void init_uart(){

    // 1. Configure UART0 parameters (matching your serial monitor settings)
    uart_config_t uart_config = {}; // Zero-initialize every single field first
    uart_config.baud_rate = 115200;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity    = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk = UART_SCLK_DEFAULT;
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_0, &uart_config));

    // 2. Set default pins for UART0 (GPIO 1 and GPIO 3 are the hardwired bridge lines)
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_0, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    // 3. Install the driver 
    // Note: We do not pass an event queue (NULL) for simple polling
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, BUF_SIZE * 2, 0, 0, NULL, 0));

}

void process_serial_input(char c) {
    if (c == 'w' || c == 'W') {
        control_state->update(MotorDirection::FORWARD);
        printf("F\n");
    } else if (c == 's' || c == 'S') {
        control_state->update(MotorDirection::REVERSE);
        printf("R\n");
    } else {
        control_state->update(MotorDirection::STOP);
        printf("S\n");
    } 
    std::cout << c;
    fflush(stdout);
}

void control_loop_task(void* pvParameters) {
    esp_task_wdt_add(NULL);
    MotorDirection last_direction = MotorDirection::STOP;

    printf("Control loop started (timeout: %lu ms)\n", (unsigned long)INPUT_TIMEOUT_MS);

    while (1) {
        MotorDirection current_direction = control_state->get_direction();

        if (current_direction != last_direction && current_direction == MotorDirection::STOP) {
            printf("Motor stopped (input timeout)\n");
        }

        motor_controller->set_direction(current_direction);
        last_direction = current_direction;

        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(CONTROL_LOOP_DELAY_MS));
    }
}

void serial_input_task(void* pvParameters) {
    esp_task_wdt_add(NULL);
    printf("Serial input task started\n");
    printf("Controls: W=forward, S=reverse, SPACE=stop\n> ");
    fflush(stdout);

    const uart_port_t uart_num = UART_NUM_0;
    size_t length = 1;
    //int num_bytes = 0;

    while (1) {
        int c = 0;
        //num_bytes = uart_read_bytes(uart_num, &c, length, pdMS_TO_TICKS(SERIAL_INPUT_DELAY_MS));
        uart_read_bytes(uart_num, &c, length, pdMS_TO_TICKS(SERIAL_INPUT_DELAY_MS));
        process_serial_input(c);
        

        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(SERIAL_INPUT_DELAY_MS));
    }
}

void status_report(){

    printf("\n");

}

extern "C" void app_main() {
    int init_cnt = 0;
    while (init_cnt < 5) {
        printf("Wake count: %d\n", init_cnt);
        init_cnt++;
        sleep(1);
    }

    printf("Installing UART Driver...\n");
    init_uart();
    printf("UART Succesfully installed\n");

    printf("Initializing L298N motor controller (C++)\n");

    motor_controller = new MotorController(MOTOR_IN1_PIN, MOTOR_IN2_PIN);
    motor_controller->init();

    control_state = new ControlState();

    printf("Creating control loop task\n");
    xTaskCreate(control_loop_task, "control_loop", 2048, NULL, 5, NULL);

    printf("Creating serial input task\n");
    xTaskCreate(serial_input_task, "serial_input", 2048, NULL, 4, NULL);

    printf("Creating status monitor task\n");
    //xTaskCreate

    vTaskDelete(NULL);
}
