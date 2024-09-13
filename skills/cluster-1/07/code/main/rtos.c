#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

#define Red_LED 12
#define White_LED 27
#define Blue_LED 33
#define Superbright_LED 15

void set_led_state(uint8_t state) {
    gpio_set_level(Superbright_LED, state & 0x01);
    gpio_set_level(Blue_LED, (state & 0x02) >> 1);
    gpio_set_level(White_LED, (state & 0x04) >> 2);
    gpio_set_level(Red_LED, (state & 0x08) >> 3);
}

//test the LEDs
void test_led(int ledPin) {
    gpio_reset_pin(ledPin);
    gpio_set_direction(ledPin, GPIO_MODE_OUTPUT);
    gpio_set_level(ledPin, 1); 
    vTaskDelay(2000 / portTICK_PERIOD_MS); 
    gpio_set_level(ledPin, 0); 
    vTaskDelay(1000 / portTICK_PERIOD_MS); 
}

void app_main(void) {
    // Test each LED individually
    test_led(Red_LED);
    test_led(White_LED);
    test_led(Blue_LED);
    test_led(Superbright_LED);

    // Initialize all LEDs as output.
    const int leds[] = {Red_LED, White_LED, Blue_LED, Superbright_LED};
    for (int i = 0; i < sizeof(leds) / sizeof(leds[0]); ++i) {
        gpio_reset_pin(leds[i]);
        gpio_set_direction(leds[i], GPIO_MODE_OUTPUT);
    }

    while (1) {
        // Count up from 0 to 15
        for (uint8_t i = 0; i < 16; ++i) {
            set_led_state(i);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }

        // Count down from 15 to 1
        for (uint8_t i = 15; i > 0; --i) {
            set_led_state(i);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }

        // Complete the count down by setting state to 0
        set_led_state(0);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}