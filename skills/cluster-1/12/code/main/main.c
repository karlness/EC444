//References
//Example code was used and modified 
//used the previous alphanumerical from quest 1 08
//used wickipedia for fourteen segment display charater
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include <string.h>
#include "sdkconfig.h"
#include "esp_vfs_dev.h"
#include "driver/i2c.h"
#include "driver/uart.h"



#define DEFAULT_VREF    1100        //Use adc2_vref_to_gpio() to obtain a better estimate
#define NO_OF_SAMPLES   64          //Multisampling
#define SLAVE_ADDR      0x70
#define OSC_CMD 0x21

#define BLINK_DISPLAYON 0x01
#define BLINK_OFF 0x00
#define BLINK_CMD 0x80
#define CMD_BRIGHTNESS 0xE0
#define MASTER_SCL_IO 22
#define MASTER_SDA_IO 23
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 100000
#define WRITE_BIT I2C_MASTER_WRITE
#define READ_BIT I2C_MASTER_READ
#define ACK_CHECK_EN true
#define ACK_CHECK_DIS false
#define ACK_VAL 0x00
#define NACK_VAL 0xFF

// Function prototypes
void i2c_master_init(void);
int i2c_test_connection(uint8_t devAddr, int32_t timeout);
void i2c_scan(void);
void alpha_display_init(void);
void alpha_display_set_blink_rate(uint8_t blinkRate);
void alpha_display_set_brightness(uint8_t brightness);
void alpha_display_send_char(uint8_t position, char c);
void process_and_display_chars(const char* buf);
uint16_t char_to_segments(char c);  // Prototype for the character to segments conversion function



// Initialization of the I2C master
void i2c_master_init(void) {
    i2c_config_t i2c_config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = MASTER_SDA_IO,
        .scl_io_num = MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(I2C_MASTER_NUM, &i2c_config);
    i2c_driver_install(I2C_MASTER_NUM, i2c_config.mode,
                       0, 0, 0);
    i2c_set_data_mode(I2C_MASTER_NUM, I2C_DATA_MODE_MSB_FIRST, I2C_DATA_MODE_MSB_FIRST);
}


int i2c_test_connection(uint8_t devAddr, int32_t timeout) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (devAddr << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    int err = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, timeout / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return err;
}


void i2c_scan(void) {
    printf("I2C scanning...\n");
    uint8_t count = 0;
    for (uint8_t i = 1; i < 127; i++) {
        if (i2c_test_connection(i, 1000) == ESP_OK) {
            printf("- Device found at address: 0x%X\n", i);
            count++;
        }
    }
    if (count == 0) printf("- No I2C devices found!\n");
}


void alpha_display_init(void) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (SLAVE_ADDR << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, OSC_CMD, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
}


void alpha_display_set_blink_rate(uint8_t blinkRate) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (SLAVE_ADDR << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, BLINK_CMD | BLINK_DISPLAYON | (blinkRate << 1), ACK_CHECK_EN);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
}


void alpha_display_set_brightness(uint8_t brightness) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (SLAVE_ADDR << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, CMD_BRIGHTNESS | brightness, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
}



//source of fourteen segment display character 
//https://www.wikiwand.com/en/Fourteen-segment_display
uint16_t char_to_segments(char c) {
    uint16_t segments;
    switch (c) {
        case '0': segments = 0xC3F; break;
        case '1': segments = 0x406; break;
        case '2': segments = 0xDB; break;
        case '3': segments = 0x8F; break;
        case '4': segments = 0xE6; break;
        case '5': segments = 0xED; break;
        case '6': segments = 0xFD; break;
        case '7': segments = 0x1401; break;
        case '8': segments = 0xFF; break;
        case '9': segments = 0xE7; break;
        case 'a': case 'A': segments = 0xF7; break;
        case 'b': case 'B': segments = 0x128F; break;
        case 'c': case 'C': segments = 0x39; break;
        case 'd': case 'D': segments = 0x120F; break;
        case 'e': case 'E': segments = 0xF9; break;
        case 'f': case 'F': segments = 0xF1; break;
        case 'g': case 'G': segments = 0xBD; break; 
        case 'h': case 'H': segments = 0x76; break;
        case 'i': case 'I': segments = 0x1209; break; 
        case 'j': case 'J': segments = 0x1E; break;
        case 'k': case 'K': segments = 0x2470; break; 
        case 'l': case 'L': segments = 0x38; break;
        case 'm': case 'M': segments = 0x536; break; 
        case 'n': case 'N': segments = 0x2136; break;
        case 'o': case 'O': segments = 0x3F; break;
        case 'p': case 'P': segments = 0xF3; break;
        case 'q': case 'Q': segments = 0x203F; break;
        case 'r': case 'R': segments = 0x20F3; break;
        case 's': case 'S': segments = 0x18D; break;
        case 't': case 'T': segments = 0x1201; break;
        case 'u': case 'U': segments = 0x3E; break;
        case 'v': case 'V': segments = 0xC30; break; 
        case 'w': case 'W': segments = 0x2836; break; 
        case 'x': case 'X': segments = 0x2D00; break; 
        case 'y': case 'Y': segments = 0x1500; break;
        case 'z': case 'Z': segments = 0xC09; break;
        default:  segments = 0x00; break; 
    }
    return segments;
}


void alpha_display_send_char(uint8_t position, char c) {
    // Convert the character to segments
    uint16_t segments = char_to_segments(c);

    // Calculate the address for the position. Typically, each position on the display has a unique address.
    // the address is usually position * 2 based on my reaseach.
    uint8_t address = position * 2;

    // Prepare the command for I2C transmission
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);

    i2c_master_write_byte(cmd, (SLAVE_ADDR << 1) | WRITE_BIT, ACK_CHECK_EN);
    
    // Send the address where we want to write the segment data
    i2c_master_write_byte(cmd, address, ACK_CHECK_EN);
    
    // Send the segment data for the character, low byte first
    i2c_master_write_byte(cmd, segments & 0xFF, ACK_CHECK_EN);
    
    // And then the high byte
    i2c_master_write_byte(cmd, segments >> 8, ACK_CHECK_EN);
    
    // End the I2C communication
    i2c_master_stop(cmd);
    
    // Execute the I2C command
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    if (ret != ESP_OK) {
        printf("I2C command failed\n");
    }
    // Clean up by deleting the I2C command link
    i2c_cmd_link_delete(cmd);
}




static esp_adc_cal_characteristics_t *adc_chars;
static const adc_channel_t channel = ADC_CHANNEL_6;     //GPIO34 if ADC1, GPIO14 if ADC2
static const adc_atten_t atten = ADC_ATTEN_DB_0;
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
  




void display_voltage(uint32_t voltage) {
    char voltageStr[5]; 
    snprintf(voltageStr, sizeof(voltageStr), "%lu", voltage); // Convert voltage to string

    // Clear the display or displaying the new value directly
    for (int i = 0; i < strlen(voltageStr); i++) {
        alpha_display_send_char(i, voltageStr[i]);
    }

    // If the display has more digits than the length of voltageStr
    for (int i = strlen(voltageStr); i < 4; i++) {
        alpha_display_send_char(i, ' '); 
    }
}


void app_main(void)
{
    
    check_efuse();

    //Configure ADC
    if (unit == ADC_UNIT_1) {
        adc1_config_width(ADC_WIDTH_BIT_12);
        adc1_config_channel_atten(channel, atten);
    } else {
        adc2_config_channel_atten((adc2_channel_t)channel, atten);
    }

    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_chars);
    print_char_val_type(val_type);

    // Initialize I2C and alphanumeric display
    i2c_master_init();
    alpha_display_init();
    alpha_display_set_brightness(0xF); 
    alpha_display_set_blink_rate(BLINK_OFF);

    while (1) {
        uint32_t adc_reading = 0;
        // Multisampling
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
        // Convert adc_reading to voltage in mV
        uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, adc_chars);
        printf("Raw: %ld\tVoltage: %ldmV\n", adc_reading, voltage);

        // Display the voltage
        display_voltage(voltage);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}