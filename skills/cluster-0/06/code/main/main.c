#include <stdio.h>
#include <string.h>
#include "driver/uart.h"
#include "esp_vfs_dev.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

#define LED_PIN 13
#define MAX_INPUT_SIZE 10
#define UART_BUFFER_SIZE 256

void initialize_system();
void toggle_led_mode();
void echo_mode();
void dec_to_hex_mode();

void app_main() {
    initialize_system();
    int mode = 0; // Start in toggle LED mode

    while (1) {
        
        switch (mode) {
            case 0:
                toggle_led_mode(&mode);
                break;
            case 1:
                echo_mode(&mode);
                break;
            case 2:
                dec_to_hex_mode(&mode);
                break;
        }
        //vTaskDelay(pdMS_TO_TICKS(50)); 
        vTaskDelay(50 / portTICK_PERIOD_MS);

    }
}

void initialize_system() {
    uart_driver_install(UART_NUM_0, UART_BUFFER_SIZE, 0, 0, NULL, 0);
    esp_vfs_dev_uart_use_driver(UART_NUM_0);
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    printf("System Initialized\n");
}

void toggle_led_mode(int *mode) {
    static bool led_state = false;
    char input[MAX_INPUT_SIZE];

    printf("Toggle LED (enter 't' to toggle, 's' to switch mode): ");
    fgets(input, MAX_INPUT_SIZE, stdin);
    if (input[0] == 't') {
        led_state = !led_state;
        gpio_set_level(LED_PIN, led_state);
    } else if (input[0] == 's') {
        *mode = 1;
        printf("Switching to echo mode\n");
    }
}

void echo_mode(int *mode) {
    char input[MAX_INPUT_SIZE];

    printf("Echo mode (enter 's' to switch mode): ");
    fgets(input, MAX_INPUT_SIZE, stdin);
    if (strncmp(input, "s", 1) == 0) {
        *mode = 2;
        printf("Switching to decimal to hexadecimal mode\n");
    } else {
        printf("Echo: %s", input);
    }
}

void dec_to_hex_mode(int *mode) {
    char input[MAX_INPUT_SIZE];
    char hex[30];

    printf("Decimal to hexadecimal mode (enter 's' to switch mode): ");
    fgets(input, MAX_INPUT_SIZE, stdin);
    if (strncmp(input, "s", 1) == 0) {
        *mode = 0;
        printf("Switching to toggle LED mode\n");
    } else {
        int num = atoi(input);
        printf("Hex: %X\n", num); // Simpler conversion to hex
    }
}