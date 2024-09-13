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
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
