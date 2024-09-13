#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "driver/uart.h"
#include "esp_vfs_dev.h"

// Define constants for LED Controller (LEDC) configurations
#define LEDC_HS_TIMER         LEDC_TIMER_0
#define LEDC_HS_MODE          LEDC_HIGH_SPEED_MODE
#define LEDC_HS_CH0_GPIO      (13)
#define LEDC_HS_CH0_CHANNEL   LEDC_CHANNEL_0
#define LEDC_LS_TIMER         LEDC_TIMER_1
#define LEDC_LS_MODE          LEDC_LOW_SPEED_MODE

// Function prototypes
void initialize_uart();
void initialize_ledc();
void adjust_led_intensity(int duty);
void cycle_led_intensity();

// Main application entry point
void app_main(void)
{
    // Initialize UART and LEDC
    initialize_uart();
    initialize_ledc();

    char buf[10];
    int duty;

    // Main loop
    while (1) {
        printf("Led Intensity (0-9) or 'cycle'): ");
        gets(buf); // Read user input

        // Check if user entered "cycle" to activate cycling mode
        if (strcmp(buf, "cycle") == 0) {
            cycle_led_intensity();
        } else {
            // Convert input to integer and adjust LED intensity accordingly
            int intensity_level = atoi(buf);
            if (intensity_level >= 0 && intensity_level <= 9) {
                duty = intensity_level * 500;
                adjust_led_intensity(duty);
            } else {
                printf("Enter a value between 0 and 9 or cycle.\n");
            }
        }
    }
}

// Initialize UART for communication
void initialize_uart() {
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 256, 0, 0, NULL, 0));
    esp_vfs_dev_uart_use_driver(UART_NUM_0);
}

// Initialize LEDC for controlling LED brightness
void initialize_ledc() {
    // Configure timer for high speed mode
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_13_BIT,
        .freq_hz = 5000,
        .speed_mode = LEDC_HS_MODE,
        .timer_num = LEDC_HS_TIMER,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&ledc_timer);

    // Configure timer for low speed mode
    ledc_timer.speed_mode = LEDC_LS_MODE;
    ledc_timer.timer_num = LEDC_LS_TIMER;
    ledc_timer_config(&ledc_timer);

    // Set up LEDC channel configuration
    ledc_channel_config_t ledc_channel = {
        .channel = LEDC_HS_CH0_CHANNEL,
        .duty = 0,
        .gpio_num = LEDC_HS_CH0_GPIO,
        .speed_mode = LEDC_HS_MODE,
        .hpoint = 0,
        .timer_sel = LEDC_HS_TIMER
    };
    ledc_channel_config(&ledc_channel);

    ledc_fade_func_install(0);
}

// Adjust LED intensity based on duty value
void adjust_led_intensity(int duty) {
    ledc_channel_config_t ledc_channel = {
        .channel = LEDC_HS_CH0_CHANNEL,
        .duty = duty,
        .gpio_num = LEDC_HS_CH0_GPIO,
        .speed_mode = LEDC_HS_MODE,
        .hpoint = 0,
        .timer_sel = LEDC_HS_TIMER
    };
    ledc_set_duty(ledc_channel.speed_mode, ledc_channel.channel, duty);
    ledc_update_duty(ledc_channel.speed_mode, ledc_channel.channel);
    printf("LEDC set duty = %d\n", duty);
}

// Cycle LED intensity up and down
void cycle_led_intensity() {
    int duty = 0;
    int intensity = 0;

    // Increase the intensity
    while (duty < 5000) {
        printf("Intensity %d\n", intensity);
        adjust_led_intensity(duty);
        vTaskDelay(25 / portTICK_PERIOD_MS);
        duty += 500;
        intensity++;
    }

    // Decrease the intensity
    vTaskDelay(25 / portTICK_PERIOD_MS);
    duty = 4000;
    intensity = 8;
    while (duty >= 0) {
        printf("Intensity %d\n", intensity);
        adjust_led_intensity(duty);
        vTaskDelay(25 / portTICK_PERIOD_MS);
        duty -= 500;
        intensity--;
    }
}
