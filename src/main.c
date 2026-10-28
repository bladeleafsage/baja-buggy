#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MOTOR_IN1_PIN 32
#define MOTOR_IN2_PIN 33

void motor_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << MOTOR_IN1_PIN) | (1ULL << MOTOR_IN2_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
}

void motor_set_direction(int direction) {
    if (direction > 0) {
        gpio_set_level(MOTOR_IN1_PIN, 1);
        gpio_set_level(MOTOR_IN2_PIN, 0);
    } else if (direction < 0) {
        gpio_set_level(MOTOR_IN1_PIN, 0);
        gpio_set_level(MOTOR_IN2_PIN, 1);
    } else {
        gpio_set_level(MOTOR_IN1_PIN, 0);
        gpio_set_level(MOTOR_IN2_PIN, 0);
    }
}

void print_menu(void) {
    printf("\n=== L298N Motor Controller ===\n");
    printf("Commands:\n");
    printf("  forward - Run motor forward\n");
    printf("  reverse - Run motor backward\n");
    printf("  stop - Stop motor\n");
    printf("  help - Show this menu\n");
    printf("==============================\n> ");
}

void process_command(char *cmd_line) {
    char cmd[32] = {0};
    sscanf(cmd_line, "%31s", cmd);

    printf("\nProcessing command: '%s'\n", cmd);

    if (strcmp(cmd, "forward") == 0) {
        printf("Motor forward\n");
        motor_set_direction(1);
    }
    else if (strcmp(cmd, "reverse") == 0) {
        printf("Motor reverse\n");
        motor_set_direction(-1);
    }
    else if (strcmp(cmd, "stop") == 0) {
        printf("Motor stopped\n");
        motor_set_direction(0);
    }
    else if (strcmp(cmd, "help") == 0) {
        print_menu();
    }
    else {
        printf("Unknown command: %s\n", cmd);
        print_menu();
    }
}

void app_main(){
    int init_cnt = 0;
    while(init_cnt < 5){
        printf("Wake count: %d \n", init_cnt);
        init_cnt++;
        sleep(1);
    }

    printf("Initializing L298N motor controller\n");
    motor_init();

    printf("Watchdog feeding enabled\n");

    print_menu();

    char input_buffer[64];
    int pos = 0;

    while(1) {
        vTaskDelay(pdMS_TO_TICKS(50));

        int c = getchar();
        if (c == EOF) {
            continue;
        }

        printf("[0x%02X] ", c);
        fflush(stdout);

        if (c == '\n' || c == '\r') {
            printf("\nNewline detected, pos=%d\n", pos);
            if (pos > 0) {
                input_buffer[pos] = '\0';
                process_command(input_buffer);
                pos = 0;
                printf("> ");
                fflush(stdout);
            }
        }
        else if (c >= 32 && c < 127 && pos < sizeof(input_buffer) - 1) {
            input_buffer[pos++] = c;
            printf("%c", c);
            fflush(stdout);
        }
    }
}