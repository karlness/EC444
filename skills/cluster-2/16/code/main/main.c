<<<<<<< HEAD
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

#define DEFAULT_VREF    1100        //Use adc2_vref_to_gpio() to obtain a better estimate
#define NO_OF_SAMPLES   64          //Multisampling

static esp_adc_cal_characteristics_t *adc_chars;
static const adc_channel_t channel = ADC_CHANNEL_6;     //GPIO34 if ADC1, GPIO14 if ADC2
static const adc_atten_t atten = ADC_ATTEN_DB_11;
static const adc_unit_t unit = ADC_UNIT_1;

static void check_efuse(void)
{
    //Check TP is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
        printf("eFuse Two Point: Supported\n");
    } else {
        printf("eFuse Two Point: NOT supported\n");
    }

    //Check Vref is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) {
        printf("eFuse Vref: Supported\n");
    } else {
        printf("eFuse Vref: NOT supported\n");
    }
}

static void print_char_val_type(esp_adc_cal_value_t val_type)
{
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        printf("Characterized using Two Point Value\n");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        printf("Characterized using eFuse Vref\n");
    } else {
        printf("Characterized using Default Vref\n");
    }
}




int calculate_distance(int voltage) {
    if (voltage > 1500) return 0;
    if (voltage > 1200) return 20;
    if (voltage > 1100) return 25;
    if (voltage > 1000) return 30;
    if (voltage > 850) return 35;
    if (voltage > 700) return 40;
    if (voltage > 650) return 45;
    if (voltage > 600) return 50;
    if (voltage > 550) return 55;
    if (voltage > 500) return 60;
    if (voltage > 475) return 65;
    if (voltage > 450) return 70;
    if (voltage > 425) return 75;
    if (voltage > 400) return 80;
    if (voltage > 375) return 90;
    if (voltage > 350) return 100;
    if (voltage > 325) return 110;
    if (voltage > 300) return 120;
    if (voltage > 275) return 130;
    if (voltage > 250) return 140;
    if (voltage > 225) return 150;
    return 160; // Default case for voltage <= 225
}


void app_main(void)
{
    //Check if Two Point or Vref are burned into eFuse
    check_efuse();

    //Configure ADC
    if (unit == ADC_UNIT_1) {
        adc1_config_width(ADC_WIDTH_BIT_12);
        adc1_config_channel_atten(channel, atten);
    } else {
        adc2_config_channel_atten((adc2_channel_t)channel, atten);
    }

    //Characterize ADC
    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_chars);
    print_char_val_type(val_type);

    //Continuously sample ADC1
    while (1) {
        uint32_t adc_reading = 0;
        //Multisampling
        for (int i = 0; i < NO_OF_SAMPLES; i++) {
            if (unit == ADC_UNIT_1) {
                adc_reading += adc1_get_raw((adc1_channel_t)channel);
            } else {
                int raw;
                adc2_get_raw((adc2_channel_t)channel, ADC_WIDTH_BIT_12, &raw);
                adc_reading += raw;
            }
        }
        adc_reading /= NO_OF_SAMPLES;
        //Convert adc_reading to voltage in mV
        uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, adc_chars);
        int dist = calculate_distance(voltage);
        printf("Distance: %dcm\n", dist);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
=======
#include "driver/i2c.h"
#include "esp_log.h"

// LIDAR device I2C addresses
#define LIDAR_I2C_ADDRESS 0x62
#define COMMAND_REGISTER 0x00
#define STATUS_REGISTER 0x01
#define DISTANCE_HIGH 0x0f
#define DISTANCE_LOW 0x10
#define MEASURE_WITHOUT_CORRECTION 0x03


#define I2C_PORT I2C_NUM_0
#define SCL_GPIO 22
#define SDA_GPIO 23
#define I2C_FREQ 100000

// Buffer sizes for I2C master
#define I2C_NO_RX_BUFFER 0
#define I2C_NO_TX_BUFFER 0

// Log tag
static const char *const TAG = "LidarSensor";

// Initializes I2C as master
static void i2c_master_setup(void) {
    i2c_config_t i2c_config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = SDA_GPIO,
        .scl_io_num = SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ,
    };
    i2c_param_config(I2C_PORT, &i2c_config);
    i2c_driver_install(I2C_PORT, i2c_config.mode, I2C_NO_RX_BUFFER, I2C_NO_TX_BUFFER, 0);
}

// Writes a byte to a specified register
static esp_err_t write_register_byte(uint8_t reg, uint8_t value) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (LIDAR_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, value, true);
    i2c_master_stop(cmd);
    esp_err_t result = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    return result;
}

// Reads a byte from a specified register
static esp_err_t read_register_byte(uint8_t reg, uint8_t *value) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (LIDAR_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd); // Repeated start for read operation
    i2c_master_write_byte(cmd, (LIDAR_I2C_ADDRESS << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, value, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t result = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    return result;
}

// Fetches a 16-bit value from the LIDAR device
static esp_err_t read_16bit_value(uint8_t high_reg, uint8_t low_reg, uint16_t *value) {
    uint8_t high_byte, low_byte;
    esp_err_t result = read_register_byte(high_reg, &high_byte);
    if (result == ESP_OK) {
        result = read_register_byte(low_reg, &low_byte);
        if (result == ESP_OK) {
            *value = ((uint16_t)high_byte << 8) | low_byte;
        }
    }
    return result;
}

// Retrieves the distance measurement from the LIDAR sensor
static esp_err_t get_lidar_distance(uint16_t *distance) {
    uint8_t status;
    esp_err_t result = write_register_byte(COMMAND_REGISTER, MEASURE_WITHOUT_CORRECTION);
    if (result == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(20)); // Wait for measurement to complete
        result = read_register_byte(STATUS_REGISTER, &status);
        if (result == ESP_OK && (status & 0x01)) { // Check if measurement is valid
            result = read_16bit_value(DISTANCE_HIGH, DISTANCE_LOW, distance);
        } else {
            result = ESP_FAIL; // Measurement invalid
        }
    }
    return result;
}

void app_main(void) {
    uint16_t distance;
    esp_err_t result;

    i2c_master_setup();

    while (1) {
        result = get_lidar_distance(&distance); 
        if (result == ESP_OK) {
            ESP_LOGI(TAG, "Distance: %d cm", distance);
        } else {
            ESP_LOGE(TAG, "Error obtaining distance: %s", esp_err_to_name(result));
        }
        vTaskDelay(pdMS_TO_TICKS(1000)); // Wait for 1 second before next measurement
    }
}

>>>>>>> ca9d9f3 (add READme files)
