#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/mcpwm.h"
#include "soc/mcpwm_reg.h"
#include "soc/mcpwm_struct.h"



#include <math.h>
#include <stdlib.h>
#include "driver/ledc.h"
#include "esp_err.h"
#include "driver/uart.h"
#include "esp_vfs_dev.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "driver/i2c.h"
#include "sdkconfig.h"
#include "soc/uart_struct.h"
#include <string.h>
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "driver/gpio.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"


#define UDP_SERVER_IP   "192.168.1.10" // Replace it with your server's IP address
#define UDP_SERVER_PORT 3334           // Replace it with your server's port number
#define BLINK_GPIO GPIO_NUM_13
#define SAMPLE_NUM 64
#define DEFAULT_VREF 1100
#define DELAY 2000           // Sensor read delays (not used)
static esp_adc_cal_characteristics_t *adc_chars;



#define SERVO_MIN_PULSEWIDTH 500 
#define SERVO_MAX_PULSEWIDTH 1000 
#define SERVO_MAX_DEGREE 90   
//#define SERVO_MIN_DEGREE -90
//for sttering testing 
#define SERVO_CENTER_PULSEWIDTH ((SERVO_MIN_PULSEWIDTH + SERVO_MAX_PULSEWIDTH) / 2)

// Define steering pulse widths
#define STEERING_LEFT_PULSEWIDTH   500
#define STEERING_CENTER_PULSEWIDTH  1500
#define STEERING_RIGHT_PULSEWIDTH  2500



/* The examples use WiFi configuration that you can set via project configuration menu

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_ESP_WIFI_SSID      "FreshTomato" //replace it with your ssid
#define EXAMPLE_ESP_WIFI_PASS      "Smart25$"    //replace it with your wifi password
#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY

#if CONFIG_ESP_WPA3_SAE_PWE_HUNT_AND_PECK
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HUNT_AND_PECK
#define EXAMPLE_H2E_IDENTIFIER ""
#elif CONFIG_ESP_WPA3_SAE_PWE_HASH_TO_ELEMENT
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HASH_TO_ELEMENT
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#elif CONFIG_ESP_WPA3_SAE_PWE_BOTH
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_BOTH
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#endif
#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1



// Function prototypes
static void initialize_mcpwm_servo_gpio(void);
static uint32_t calculate_pulse_width_for_degree(uint32_t degree);
static void esc_calibration_routine(void);
static void control_servo_and_esc_task(void *arg);
static void adjust_speed_task(void *arg);
static void steering_test_task(void *arg); // New function for steering test

void move_backward();
void stop_movement();
void move_forward();
void steer_left();
void steer_right();
void steer_center();

void steer();

static void udp_listener_task(void *pvParameters);

// Global variables for speed control and status
static uint32_t speed_pulse_width = 1400; // Neutral speed pulse width
static int speed_control_status = 0; // Control status flag


/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;


static const char *TAG = "wifi station";

static int s_retry_num = 0;

static void initialize_mcpwm_servo_gpio(void) {
    printf("Initializing MCPWM for servo control on GPIOs...\n");
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, 27); // GPIO 27 as PWM0A for servo
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0B, 12); // GPIO 12 as PWM0B, optional
}

static uint32_t calculate_pulse_width_for_degree(uint32_t degree) {
    return SERVO_MIN_PULSEWIDTH + (SERVO_MAX_PULSEWIDTH - SERVO_MIN_PULSEWIDTH) * degree / SERVO_MAX_DEGREE;
}

//function for calibration
static void esc_calibration_routine(void) {
    printf("Setting ESC to NEUTRAL. Wait for 3 seconds to complete the calibration.\n");
    mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, 1400);
    // Wait for 3 seconds to ensure the ESC registers this calibration step
    vTaskDelay(pdMS_TO_TICKS(3000));
}



static void control_servo_and_esc_task(void *arg) {
    initialize_mcpwm_servo_gpio();

    printf("Configuring initial parameters for MCPWM...\n");
    mcpwm_config_t pwm_config = {.frequency = 50, .cmpr_a = 0, .cmpr_b = 0, .counter_mode = MCPWM_UP_COUNTER, .duty_mode = MCPWM_DUTY_MODE_0};
    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);

    // Calibrate ESC before starting control loop
    esc_calibration_routine();
    speed_control_status = 1;

    while (1) {
        for (uint32_t angle = 0; angle <= SERVO_MAX_DEGREE; angle++) {
            uint32_t pulse_width = calculate_pulse_width_for_degree(angle);
            mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, pulse_width);
            mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, speed_pulse_width);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }

        for (uint32_t angle = SERVO_MAX_DEGREE; angle > 0; angle--) {
            uint32_t pulse_width = calculate_pulse_width_for_degree(angle);
            mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, pulse_width);
            mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, speed_pulse_width);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

//steering function, not relaying on servo angles calculation(this is not working)
static void steering_test_task(void *arg) {
    printf("Starting simplified steering test...\n");
    
    while (1) {
        // Assuming 1500 microseconds is center, 500 is left, and 2500 is right
        uint32_t leftPulse = 500, centerPulse = 1500, rightPulse = 2500;

        printf("Moving to LEFT position\n");
        mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, leftPulse);
        vTaskDelay(pdMS_TO_TICKS(500));

        printf("Moving to CENTER position\n");
        mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, centerPulse);
        vTaskDelay(pdMS_TO_TICKS(500));

        printf("Moving to RIGHT position\n");
        mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, rightPulse);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}


static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        //udp_client_send(); //send a UDP packet after connection

    }
}


void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (pasword len => 8).
             * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
             * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
             * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
             */
            .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
            .sae_pwe_h2e = ESP_WIFI_SAE_MODE,
            .sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}


 //Function to send a UDP command(not used)
static void udp_client_send(const char* command) {
    struct sockaddr_in dest_addr = {
        .sin_addr.s_addr = inet_addr(UDP_SERVER_IP),
        .sin_family = AF_INET,
        .sin_port = htons(UDP_SERVER_PORT),
    };

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return;
    }
    ESP_LOGI(TAG, "Socket created");

    int err = sendto(sock, command, strlen(command), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
    } else {
        ESP_LOGI(TAG, "Command sent: %s", command);
    }

    close(sock); // Close the socket now that we're done
    ESP_LOGI(TAG, "Socket closed");
}



static void udp_listener_task(void *pvParameters) {
    char rx_buffer[128];
    char addr_str[128];
    int addr_family = AF_INET;
    int ip_protocol = IPPROTO_IP;

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(UDP_SERVER_PORT);

    int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "Socket created");

    int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        close(sock);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "Socket bound, port %d", UDP_SERVER_PORT);

    while (1) {
        ESP_LOGI(TAG, "Waiting for data...");
        struct sockaddr_in source_addr; // Source address of the sender
        socklen_t socklen = sizeof(source_addr);

        // Clear the buffer and receive data
        memset(rx_buffer, 0, sizeof(rx_buffer));
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

        if (len < 0) {
            ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
            break;
        } else {
            // Null-terminate the received data to make it a string
            rx_buffer[len] = 0;
            inet_ntoa_r(source_addr.sin_addr, addr_str, sizeof(addr_str) - 1);
            ESP_LOGI(TAG, "Received %d bytes from %s: %s", len, addr_str, rx_buffer);

            // Process the received command from the server 
            if (strcmp(rx_buffer, "forward") == 0) {
                ESP_LOGI(TAG, "Command 'forward' received");
                move_forward();
            } else if (strcmp(rx_buffer, "backward") == 0) {
                ESP_LOGI(TAG, "Command 'backward' received");
                move_backward();
            } else if (strcmp(rx_buffer, "stop") == 0) {
                ESP_LOGI(TAG, "Command 'stop' received");
                stop_movement();
            } else if (strcmp(rx_buffer, "left") == 0) {
                ESP_LOGI(TAG, "Command 'left' received");
                steer(-20);
            } else if (strcmp(rx_buffer, "right") == 0) {
                ESP_LOGI(TAG, "Command 'right' received");
                steer(20);
            } else if (strcmp(rx_buffer, "center") == 0) {
                 ESP_LOGI(TAG, "Command 'center' received");
                 steer(0);
           }
        }
    }

    if (sock != -1) {
        ESP_LOGE(TAG, "Shutting down socket and restarting...");
        shutdown(sock, 0);
        close(sock);
    }
    vTaskDelete(NULL);
}



static uint32_t calculate_pulse_width_for_angle(int angle) {
    // Map angle to pulse width based on your servo's specifications
    int pulse_width = SERVO_CENTER_PULSEWIDTH + (angle * (SERVO_MAX_PULSEWIDTH - SERVO_MIN_PULSEWIDTH) / (SERVO_MAX_DEGREE * 2));
    return pulse_width;
}

void steer(int angle) {
    uint32_t pulse_width = calculate_pulse_width_for_angle(angle);
    mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, pulse_width);
    ESP_LOGI(TAG, "Steering with pulse width: %luu for angle: %d", pulse_width, angle);
}

void steer_left() {
    mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, STEERING_LEFT_PULSEWIDTH);
    ESP_LOGI(TAG, "Steering left with pulse width: %uus", STEERING_LEFT_PULSEWIDTH);
}

void steer_right() {
    mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, STEERING_RIGHT_PULSEWIDTH);
    ESP_LOGI(TAG, "Steering right with pulse width: %uus", STEERING_RIGHT_PULSEWIDTH);
}

void steer_center() {
    mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, STEERING_CENTER_PULSEWIDTH);
    ESP_LOGI(TAG, "Steering center with pulse width: %uus", STEERING_CENTER_PULSEWIDTH);
}





// Movement control functions
void move_forward() {
    speed_pulse_width = 400; 
    mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, speed_pulse_width);
    ESP_LOGI(TAG, "Moving forward with pulse width: %ldus", speed_pulse_width);
}

void move_backward() {
    speed_pulse_width = 700;
    mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, speed_pulse_width);
    ESP_LOGI(TAG, "Moving backward with pulse width: %ldus", speed_pulse_width);
}


void stop_movement() {
    speed_pulse_width = 50;
    mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, speed_pulse_width);
    ESP_LOGI(TAG, "Stopping movement with pulse width: %ldus", speed_pulse_width);
}


void app_main(void) {
    ESP_LOGI(TAG, "ESP32 UDP server example");
    ESP_ERROR_CHECK(nvs_flash_init());
    wifi_init_sta();

    // Initialize the MCPWM module for servo control
    initialize_mcpwm_servo_gpio();
    
    
    esc_calibration_routine();

    // allow the systems to stabilize
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Send a neutral signal to ESC to ensure it's ready to receive further commands
    stop_movement();
    vTaskDelay(pdMS_TO_TICKS(1000)); // Wait for ESC to register the neutral signal
    steer_center(); 
    vTaskDelay(pdMS_TO_TICKS(1000));
    steer_left();   
    vTaskDelay(pdMS_TO_TICKS(1000));
    steer_right();  
    vTaskDelay(pdMS_TO_TICKS(1000));
    steer_center();  // Return to center
     

    xTaskCreate(control_servo_and_esc_task, "control_servo_and_esc_task", 4096, NULL, 5, NULL);
    // Start UDP listener after making sure that ESC is calibrated and neutral signal is sent
    xTaskCreate(udp_listener_task, "udp_listener_task", 4096, NULL, 5, NULL);
}

