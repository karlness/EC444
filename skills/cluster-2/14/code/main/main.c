#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "driver/i2c.h"
#include "driver/uart.h"
#include "esp_vfs_dev.h"
#include "sdkconfig.h"

#define DEFAULT_VREF    1100        //Use adc2_vref_to_gpio() to obtain a better estimate
#define NO_OF_SAMPLES   64          // Multisampling for averaging

static esp_adc_cal_characteristics_t *adc_characteristics;
static const adc_channel_t adc_channel = ADC_CHANNEL_6; // GPIO34 if ADC1, GPIO14 if ADC2
static const adc_atten_t attenuation = ADC_ATTEN_DB_11;
static const adc_unit_t adc_unit = ADC_UNIT_1;

static void check_efuse(void) {
    // Check if Two Point or Vref are burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
        printf("eFuse Two Point: Supported\n");
    } else {
        printf("eFuse Two Point: NOT supported\n");
    }

    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) {
        printf("eFuse Vref: Supported\n");
    } else {
        printf("eFuse Vref: NOT supported\n");
    }
}

static void print_calibration_type(esp_adc_cal_value_t val_type) {
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        printf("Characterized using Two Point Value\n");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        printf("Characterized using eFuse Vref\n");
    } else {
        printf("Characterized using Default Vref\n");
    }
}

void app_main(void) {
    check_efuse();

    // Configure ADC
    if (adc_unit == ADC_UNIT_1) {
        adc1_config_width(ADC_WIDTH_BIT_12);
        adc1_config_channel_atten(adc_channel, attenuation);
    } else {
        adc2_config_channel_atten((adc2_channel_t)adc_channel, attenuation);
    }

    // Characterize ADC
    adc_characteristics = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t calibration_type = esp_adc_cal_characterize(adc_unit, attenuation, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_characteristics);
    print_calibration_type(calibration_type);

    uint32_t adc_sample_sum;
    int samples[10];

    while (1) {
        for (int i = 0; i < 10; i++) {
            uint32_t adc_reading = 0;
            for (int j = 0; j < NO_OF_SAMPLES; j++) {
                if (adc_unit == ADC_UNIT_1) {
                    adc_reading += adc1_get_raw((adc1_channel_t)adc_channel);
                } else {
                    int raw;
                    adc2_get_raw((adc2_channel_t)adc_channel, ADC_WIDTH_BIT_12, &raw);
                    adc_reading += raw;
                }
            }
            adc_reading /= NO_OF_SAMPLES; // Average
            samples[i] = adc_reading;
            vTaskDelay(pdMS_TO_TICKS(100)); // 100 ms delay
        }

        adc_sample_sum = 0;
        for (int i = 0; i < 10; i++) {
            adc_sample_sum += samples[i];
        }
        uint32_t average_adc_value = adc_sample_sum / 10;
        float voltage = average_adc_value; 
        float distance_mm = voltage * 5 + 300; // Convert to distance with calibration offset

        printf("Distance: %.1fmm\n", distance_mm);
    }
}
