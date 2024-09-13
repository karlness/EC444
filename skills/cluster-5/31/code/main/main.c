// Michael Waetzman, mwae@bu.edu, 04/09/24
// Group Quest
// Adapted by Karl Carisme

#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include <math.h>
#include <time.h>

#define THRESHOLD 4800          //! CALIBRATE THIS
#define CIRCUMFERENCE 0.22294     //! Wheel diameter in meter

static esp_adc_cal_characteristics_t adc1_chars;
float voltage;     // Voltage (mV)

int speed_calc() {
    int lastState = 0;
    int count = 0;
    clock_t lastTime = clock();
    float speed = 0.0;

    // Set up reads
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_DEFAULT, 0, &adc1_chars);

    ESP_ERROR_CHECK(adc1_config_width(ADC_WIDTH_BIT_DEFAULT));
    ESP_ERROR_CHECK(adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11));


    while(1) {
        voltage = esp_adc_cal_raw_to_voltage(adc1_get_raw(ADC1_CHANNEL_6), &adc1_chars);
        int currentState = (voltage > THRESHOLD) ? 1 : 0;

        // Check for a state change
        if (currentState != lastState) {
            count++;
            lastState = currentState;
        }

        // Calculate speed every second
        if ((clock() - lastTime) / CLOCKS_PER_SEC >= 1) {
            float distance = (count / 2) * CIRCUMFERENCE;   // Divide by 2 for both black and white segments
            speed = distance;
            printf("Speed: %.2f m/s\n", speed);
            count = 0;                                      // Reset count for the next second
            lastTime = clock();
        }
        printf("Converted: %f m/s\n", speed);
        printf("RAW: %fmV\n\n", voltage);
        vTaskDelay(pdMS_TO_TICKS(100));

    }
}



float voltage_to_speed(float voltage) {
 return 3.14;
}

void read_voltage() {

    // Set up reads
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_DEFAULT, 0, &adc1_chars);

    ESP_ERROR_CHECK(adc1_config_width(ADC_WIDTH_BIT_DEFAULT));
    ESP_ERROR_CHECK(adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11));

    // Constantly read voltage, outputs to global "voltage"
    while (1) {
        voltage = esp_adc_cal_raw_to_voltage(adc1_get_raw(ADC1_CHANNEL_6), &adc1_chars);

        printf("Converted: %f m/s\n", voltage_to_speed(voltage));
        printf("RAW: %fmV\n\n", voltage);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void app_main(void) {
    xTaskCreate(speed_calc, "speed_calc", 1024*2, NULL, 5, NULL);

}
