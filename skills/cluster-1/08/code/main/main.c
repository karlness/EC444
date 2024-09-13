#include <stdio.h>
#include "driver/i2c.h"
#include "esp_vfs_dev.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include <string.h>


#define SLAVE_ADDR 0x70
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
#define SCROLL_DELAY_MS 1000 // Adjust the delay to control the scrolling speed


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


uint16_t char_to_segments(char c) {
    uint16_t segments;
    switch (c) {
        case '0': segments = 0x3F; break;
        case '1': segments = 0x06; break;
        case '2': segments = 0x5B; break;
        case '3': segments = 0x4F; break;
        case '4': segments = 0x66; break;
        case '5': segments = 0x6D; break;
        case '6': segments = 0x7D; break;
        case '7': segments = 0x07; break;
        case '8': segments = 0x7F; break;
        case '9': segments = 0x6F; break;
        case 'a': case 'A': segments = 0x77; break;
        case 'b': case 'B': segments = 0x7C; break;
        case 'c': case 'C': segments = 0x39; break;
        case 'd': case 'D': segments = 0x5E; break;
        case 'e': case 'E': segments = 0x79; break;
        case 'f': case 'F': segments = 0x71; break;
        case 'g': case 'G': segments = 0x7D; break; // or 0x3D for a 'g' that does not close at the top
        case 'h': case 'H': segments = 0x76; break;
        case 'i': case 'I': segments = 0x06; break; // or 0x30 for a capital 'I'
        case 'j': case 'J': segments = 0x1E; break;
        case 'k': case 'K': segments = 0x76; break; // 'K' is not well represented
        case 'l': case 'L': segments = 0x38; break;
        case 'm': case 'M': segments = 0x37; break; // 'M' cannot be accurately represented
        case 'n': case 'N': segments = 0x54; break;
        case 'o': case 'O': segments = 0x5C; break;
        case 'p': case 'P': segments = 0x73; break;
        case 'q': case 'Q': segments = 0x67; break;
        case 'r': case 'R': segments = 0x50; break;
        case 's': case 'S': segments = 0x6D; break;
        case 't': case 'T': segments = 0x78; break;
        case 'u': case 'U': segments = 0x3E; break;
        case 'v': case 'V': segments = 0x3E; break; // 'V' is like 'U'
        case 'w': case 'W': segments = 0x2A; break; // 'W' cannot be accurately represented
        case 'x': case 'X': segments = 0x76; break; // using 'H' as 'X'
        case 'y': case 'Y': segments = 0x6E; break;
        case 'z': case 'Z': segments = 0x5B; break;
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
    
    // Send the I2C slave address with the write operation bit
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


void process_and_scroll_chars(const char* str) {
    int len = strlen(str);
    if (len <= 4) {
        // If the string is 4 or fewer characters, just display it normally.
        process_and_display_chars(str);
    } else {
        // If the string is longer than 4 characters, scroll it.
        char buf[5] = {0}; // Buffer for the characters to display

        for (int i = 0; i < len + 4; i++) { // Loop to scroll through characters
            // Fill the buffer with up to 4 characters from the string
            for (int j = 0; j < 4; j++) {
                int idx = i + j; // Calculate the index in the string
                if (idx < len) {
                    buf[j] = str[idx];
                } else {
                    // After the end of the string, pad with spaces for smooth scrolling off
                    buf[j] = ' ';
                }
            }

            buf[4] = '\0'; // Ensure the buffer is null-terminated
            process_and_display_chars(buf); // Display the current window of characters

            vTaskDelay(pdMS_TO_TICKS(SCROLL_DELAY_MS)); 
        }
    }
}


void process_and_display_chars(const char* buf) {
    for (int i = 0; i < 4 && buf[i] != '\0'; i++) {
        alpha_display_send_char(i, buf[i]);
    }
}


void test_alpha_display(void *pvParameters) {
    // Initialization routines for the display
    alpha_display_init();
    alpha_display_set_blink_rate(BLINK_OFF);
    alpha_display_set_brightness(0xF);

    // Main loop to receive input and display
    while (1) {
        char buf[64]; // Larger buffer for longer input strings
        printf("Enter text: ");
        fgets(buf, sizeof(buf), stdin);

        // Remove the newline character if present
        if (buf[strlen(buf) - 1] == '\n') {
            buf[strlen(buf) - 1] = '\0';
        }
        printf("You entered: %s\n", buf);

        // Now using process_and_scroll_chars to handle the input
        process_and_scroll_chars(buf);
    }
}


void app_main() {
    // Initialization code
    uart_driver_install(UART_NUM_0, 256, 0, 0, NULL, 0);
    esp_vfs_dev_uart_use_driver(UART_NUM_0);
    i2c_master_init();
    i2c_scan();

    // Start alphanumeric display test
    xTaskCreate(test_alpha_display, "test_alpha_display", 4096, NULL, 5, NULL);
}



