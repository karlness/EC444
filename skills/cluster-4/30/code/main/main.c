#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/mcpwm.h"
#include "soc/mcpwm_reg.h"
#include "soc/mcpwm_struct.h"


#define SERVO_MIN_PULSEWIDTH 500 
#define SERVO_MAX_PULSEWIDTH 2500 
#define SERVO_MAX_DEGREE 90   
//#define SERVO_MIN_DEGREE -90
//for sttering testing 
#define SERVO_CENTER_PULSEWIDTH ((SERVO_MIN_PULSEWIDTH + SERVO_MAX_PULSEWIDTH) / 2)



// Function prototypes
static void initialize_mcpwm_servo_gpio(void);
static uint32_t calculate_pulse_width_for_degree(uint32_t degree);
static void esc_calibration_routine(void);
static void control_servo_and_esc_task(void *arg);
static void adjust_speed_task(void *arg);
static void steering_test_task(void *arg); // New function for steering test

// Global variables for speed control and status
static uint32_t speed_pulse_width = 1400; // Neutral speed pulse width
static int speed_control_status = 0; // Control status flag

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


static void adjust_speed_task(void *arg) {
    while (1) {
        if (speed_control_status) {
            // Transition to FULL BACKWARD
            printf("\n---STOP to FULL BACKWARD---\n");
            for (speed_pulse_width = 1400; speed_pulse_width >= 700; speed_pulse_width -= 100) {
                mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, speed_pulse_width);
                printf("ESC speed pulse width: %ldus (FULL BACKWARD)\n", speed_pulse_width);
                vTaskDelay(pdMS_TO_TICKS(1000));
            }

            // Transition to STOP from FULL BACKWARD
            printf("\n---FULL BACKWARD to STOP---\n");
            for (; speed_pulse_width <= 1400; speed_pulse_width += 100) {
                mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, speed_pulse_width);
                printf("ESC speed pulse width: %ldus (STOP)\n", speed_pulse_width);
                vTaskDelay(pdMS_TO_TICKS(1000));
            }

            // Transition to FULL FORWARD
            printf("\n---STOP to FULL FORWARD---\n");
            for (; speed_pulse_width <= 2100; speed_pulse_width += 100) {
                mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, speed_pulse_width);
                printf("ESC speed pulse width: %ldus (FULL FORWARD)\n", speed_pulse_width);
                vTaskDelay(pdMS_TO_TICKS(1000));
            }

            // Transition to STOP from FULL FORWARD
            printf("\n---FULL FORWARD to STOP---\n");
            for (; speed_pulse_width >= 1400; speed_pulse_width -= 100) {
                mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, speed_pulse_width);
                printf("ESC speed pulse width: %ldus (STOP)\n", speed_pulse_width);
                vTaskDelay(pdMS_TO_TICKS(1000));
            }

            // Ensure the ESC is set to NEUTRAL (STOP) after completing the cycle
            speed_pulse_width = 1400;
            mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, speed_pulse_width);
            printf("ESC speed pulse width reset to NEUTRAL: %ldus\n", speed_pulse_width);
        }
        vTaskDelay(pdMS_TO_TICKS(1000)); // Short delay before starting the loop again
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



void app_main(void) {
    printf("Starting servo motor, ESC, and steering test...\n");
    xTaskCreate(control_servo_and_esc_task, "control_servo_and_esc_task", 4096, NULL, 5, NULL);
    xTaskCreate(adjust_speed_task, "adjust_speed_task", 2048, NULL, 5, NULL);
    //xTaskCreate(steering_test_task, "steering_test_task", 2048, NULL, 5, NULL); // Create the steering test task
}
