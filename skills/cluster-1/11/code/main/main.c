#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_timer.h"

// Constants for the I2C master and display
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_SDA_IO 23
#define I2C_MASTER_SCL_IO 22
#define I2C_MASTER_FREQ_HZ 100000
#define SLAVE_ADDR 0x70 
#define WRITE_BIT I2C_MASTER_WRITE
#define ACK_CHECK_EN true
#define ACK_VAL 0x00
#define NACK_VAL 0xFF
#define BLINK_OFF 0x00
#define SLAVE_ADDR 0x70
#define OSC_CMD 0x21
#define BLINK_DISPLAYON 0x01
#define BLINK_OFF 0x00
#define BLINK_CMD 0x80
#define CMD_BRIGHTNESS 0xE0
#define MASTER_SCL_IO 22
#define MASTER_SDA_IO 23

// Constants for the button
#define BUTTON_GPIO 32
#define DEBOUNCE_TIME_MS 200

// Global variables for the timer
static volatile int seconds = 0;
static volatile int minutes = 0;
static volatile bool resetTimer = false;
static volatile bool stopTimer = false;
static volatile int buttonPressCount = 0;
static volatile int64_t lastButtonPressTime = 0;



// Function prototypes
void i2c_master_init(void);
void alpha_display_init(void);
void button_gpio_init(void);
static void IRAM_ATTR button_isr_handler(void* arg);
static void check_button_task(void *arg);
void app_main(void);

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
        case '8': segments = 0xFF & ~(1 << 15); break; 
        case '9': segments = 0x6F; break;
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




// Button GPIO initialization
void button_gpio_init(void) {
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .pull_up_en = 1,
        .pull_down_en = 0,
    };
    gpio_config(&io_conf);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_GPIO, button_isr_handler, NULL);
}

// Button ISR handler
static void IRAM_ATTR button_isr_handler(void* arg) {
    int64_t currentTime = esp_timer_get_time();
    if (currentTime - lastButtonPressTime > DEBOUNCE_TIME_MS * 1000) {
        buttonPressCount++;
        lastButtonPressTime = currentTime;
    }
}

// Task to check button state
static void check_button_task(void *arg) {
    while (1) {
        if (buttonPressCount > 0) {
            int count = buttonPressCount;
            buttonPressCount = 0;
            if (count == 1) {
                resetTimer = true;
            } else if (count >= 2) {
                stopTimer = !stopTimer;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

     


void display_time_on_alpha_display(int minutes, int seconds) {
    char timeStr[5]; // Buffer to hold the time string "MMSS"

    // Format time into MMSS format
    sprintf(timeStr, "%02d%02d", minutes, seconds);

    // Send each character to the display
    for (int i = 0; i < 4; i++) {
        alpha_display_send_char(i, timeStr[i]);
    }
}

void app_main(void) {
    // Initialize button GPIO
    button_gpio_init();

    // Initialize I2C master interface
    i2c_master_init();

    // Initialize the alphanumeric display
    alpha_display_init();

    // Set display brightness and blink rate (optional, adjust as needed)
    alpha_display_set_brightness(15); // Max brightness
    alpha_display_set_blink_rate(BLINK_OFF); // No blinking

    // Create a task to check the button state
    xTaskCreate(check_button_task, "check_button_task", 2048, NULL, 10, NULL);

    // Main loop
    while (1) {
        if (resetTimer) {
            seconds = 0;
            minutes = 0;
            resetTimer = false;
        }

        if (!stopTimer) {
            // Update display with the current time
            display_time_on_alpha_display(minutes, seconds);

            ESP_LOGI("TIMER", "Timer: %02d:%02d", minutes, seconds);

            vTaskDelay(1000 / portTICK_PERIOD_MS);
            seconds++;
            if (seconds >= 60) {
                minutes++;
                seconds = 0;
            }
        }
    }
}


