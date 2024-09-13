#include <stdio.h>
#include <math.h>
#include "driver/i2c.h"
#include "./ADXL343.h"


#define MASTER_SCL_IO                22
#define MASTER_SDA_IO                23
#define I2C_MASTER_NUM               I2C_NUM_0
#define I2C_MASTER_FREQ_HZ           100000
#define WRITE_BIT                    I2C_MASTER_WRITE
#define READ_BIT                     I2C_MASTER_READ
#define ACK_CHECK                    true
#define NACK_CHECK                   false
#define ACK_VALUE                    0x00
#define NACK_VALUE                   0xFF
#define SLAVE_ADDRESS                ADXL343_ADDRESS

// Initialize I2C as master
static void init_i2c_master() {
    printf("\n>> I2C Configuration\n");

    i2c_config_t i2c_config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = MASTER_SDA_IO,
        .scl_io_num = MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
        .clk_flags = 0,
    };

    // Setting I2C parameters
    if (i2c_param_config(I2C_MASTER_NUM, &i2c_config) == ESP_OK) {
        printf("- Parameters: OK\n");
    }

    // Installing I2C driver
    if (i2c_driver_install(I2C_MASTER_NUM, i2c_config.mode, 0, 0, 0) == ESP_OK) {
        printf("- Initialized: Yes\n");
    }

    // Set data mode
    i2c_set_data_mode(I2C_MASTER_NUM, I2C_DATA_MODE_MSB_FIRST, I2C_DATA_MODE_MSB_FIRST);
}

// Test connection to I2C device
int test_connection(uint8_t devAddr, int32_t timeout) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (devAddr << 1) | WRITE_BIT, ACK_CHECK);
    i2c_master_stop(cmd);
    int err = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, timeout / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return err;
}

// Scan for I2C devices
static void scan_i2c_devices() {
    printf("\n>> Scanning I2C bus...\n");
    uint8_t count = 0;
    for (uint8_t i = 1; i < 127; ++i) {
        if (test_connection(i, 1000) == ESP_OK) {
            printf("- Device found at address: 0x%X\n", i);
            ++count;
        }
    }
    if (count == 0) {
        printf("- No I2C devices found!\n");
    }
}


int getDeviceID(uint8_t *data) {
  int ret;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, ( SLAVE_ADDRESS << 1 ) | WRITE_BIT, ACK_CHECK);
  i2c_master_write_byte(cmd, ADXL343_REG_DEVID, ACK_CHECK);
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, ( SLAVE_ADDRESS << 1 ) | READ_BIT, ACK_CHECK);
  i2c_master_read_byte(cmd, data, ACK_CHECK);
  i2c_master_stop(cmd);
  ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd);
  return ret;
}

// Write one byte to register
void writeRegister(uint8_t reg, uint8_t data) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, ( SLAVE_ADDRESS << 1 ) | WRITE_BIT, ACK_CHECK);
    i2c_master_write_byte(cmd, reg, ACK_CHECK);
    i2c_master_write_byte(cmd, data, ACK_CHECK);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
}

// Read register
uint8_t readRegister(uint8_t reg) {
    uint8_t value;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    //start command
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, ( SLAVE_ADDRESS << 1 ) | WRITE_BIT, ACK_CHECK);
    //register pointer sent
    i2c_master_write_byte(cmd, reg, ACK_CHECK);
    i2c_master_start(cmd);
    //slave followed by read bit
    i2c_master_write_byte(cmd, ( SLAVE_ADDRESS << 1 ) | READ_BIT, ACK_CHECK);
    //place data from register into bus
    i2c_master_read_byte(cmd, &value, ACK_CHECK);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return value;
}

// Read 16 bits
int16_t read16(uint8_t reg) {
    uint8_t val1;
    uint8_t val2;
    val1 = readRegister(reg);
    if (reg == 41) {
        val2 = 0;
    } else {
        val2 = readRegister(reg+1);
    }
    return (((int16_t)val2 << 8) | val1);
}

void setRange(range_t range) {
  // Read the data format register to preserve bits
  uint8_t format = readRegister(ADXL343_REG_DATA_FORMAT);

  // Update the data rate
  format &= ~0x0F;
  format |= range;

  
  format |= 0x08;

  // Write the register back to the IC
  writeRegister(ADXL343_REG_DATA_FORMAT, format);

}

range_t getRange(void) {
  // Read the data format register to preserve bits
  return (range_t)(readRegister(ADXL343_REG_DATA_FORMAT) & 0x03);
}

dataRate_t getDataRate(void) {
  return (dataRate_t)(readRegister(ADXL343_REG_BW_RATE) & 0x0F);
}

// Get acceleration
void getAccel(float * xp, float *yp, float *zp) {
  *xp = read16(ADXL343_REG_DATAX0) * ADXL343_MG2G_MULTIPLIER * SENSORS_GRAVITY_STANDARD;
  *yp = read16(ADXL343_REG_DATAY0) * ADXL343_MG2G_MULTIPLIER * SENSORS_GRAVITY_STANDARD;
  *zp = read16(ADXL343_REG_DATAZ0) * ADXL343_MG2G_MULTIPLIER * SENSORS_GRAVITY_STANDARD;
 printf("X: %.2f \t Y: %.2f \t Z: %.2f\n", *xp, *yp, *zp);
}

// Calculate print roll and pitch
void calcRP(float x, float y, float z){
  float roll = atan2(y , z) * 57.3;
  float pitch = atan2((-1*x) , sqrt(y*y + z*z)) * 57.3;
  printf("roll: %.2f \t pitch: %.2f \n", roll, pitch);
}

//Pull the acceleration and calculate roll and pitch
static void test_adxl343() {
  printf("\n>> Polling ADAXL343\n");
  while (1) {
    float xVal, yVal, zVal;
    getAccel(&xVal, &yVal, &zVal);
    calcRP(xVal, yVal, zVal);
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}


void app_main() {
    // Initialization and device scanning
    init_i2c_master();
    scan_i2c_devices();

    // Check for ADXL343
    uint8_t deviceID;
    if (getDeviceID(&deviceID) == ESP_OK && deviceID == 0xE5) {
        printf("\n>> ADXL343 detected.\n");

        // Disable interrupts
        writeRegister(ADXL343_REG_INT_ENABLE, 0);

        // Set range
        setRange(ADXL343_RANGE_2_G);
        // Display the range setting
        printf("- Range set to: +/- ");
        switch (getRange()) {
            case ADXL343_RANGE_16_G: printf("16 g\n"); break;
            case ADXL343_RANGE_8_G: printf("8 g\n"); break;
            case ADXL343_RANGE_4_G: printf("4 g\n"); break;
            case ADXL343_RANGE_2_G: printf("2 g\n"); break;
            default: printf("Unknown\n"); break;
        }

        // Display data rate
        printf("- Data Rate set to: ");
        switch (getDataRate()) {
            case ADXL343_DATARATE_3200_HZ: printf("3200 Hz\n"); break;
            case ADXL343_DATARATE_1600_HZ: printf("1600 Hz\n"); break;
            case ADXL343_DATARATE_800_HZ: printf("800 Hz\n"); break;
            default: printf("Unknown\n"); break;
        }

        writeRegister(ADXL343_REG_POWER_CTL, 0x08);

        //Get the sensor and calculate roll and pitch
        xTaskCreate(test_adxl343, "test_adxl343_task", 2048, NULL, 5, NULL);
    } else {
        printf("\n>> ADXL343 is not detected. Check your file.\n");
    }
}