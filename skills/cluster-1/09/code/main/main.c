#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/mcpwm.h"
#include "soc/mcpwm_periph.h"

// Constants defining servo pulse width parameters and maximum rotation degree
#define SERVO_MIN_PULSEWIDTH 450 // Minimum pulse width in microseconds
#define SERVO_MAX_PULSEWIDTH 2450 // Maximum pulse width in microseconds
#define SERVO_MAX_DEGREE 180 // Maximum rotation angle in degrees

 //Initializes MCPWM GPIO for servo control.
 
static void initialize_mcpwm_servo_gpio(void) {
    printf("Initializing MCPWM servo control GPIO...\n");
    // Initialize GPIO 12 as MCPWM0A for servo control
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, 12);
}

/**
 * Calculates the pulse width for a given servo rotation angle.
 @param degree_of_rotation //Angle in degrees for the servo rotation.
  @return //Calculated pulse width in microseconds.
 */
static uint32_t calculate_pulse_width_per_degree(uint32_t degree_of_rotation) {
    return SERVO_MIN_PULSEWIDTH + (((SERVO_MAX_PULSEWIDTH - SERVO_MIN_PULSEWIDTH) * degree_of_rotation) / SERVO_MAX_DEGREE);
}

/**
 Configures and controls the servo motor using MCPWM.
 * @param arg User-defined argument
 */
void servo_control_task(void *arg) {
    uint32_t angle;

    // Initialize MCPWM GPIO
    initialize_mcpwm_servo_gpio();

    // Configure MCPWM
    printf("Configuring initial parameters of MCPWM...\n");
    mcpwm_config_t pwm_config = {
        .frequency = 50, // Set frequency to 50Hz (20ms period)
        .cmpr_a = 0, // Initial duty cycle for PWMxA
        .cmpr_b = 0, // Initial duty cycle for PWMxB
        .counter_mode = MCPWM_UP_COUNTER,
        .duty_mode = MCPWM_DUTY_MODE_0
    };
    
    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);

    //Rotate servo within its range continuously
    while (1) {
        for (angle = 0; angle <= SERVO_MAX_DEGREE; angle++) {
            printf("Angle of rotation: %lu\n", angle);
            uint32_t pulse_width = calculate_pulse_width_per_degree(angle);
            printf("Pulse width: %luus\n", pulse_width);
            mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, pulse_width);
            vTaskDelay(pdMS_TO_TICKS(200)); // Delay to allow servo time to complete its rotation
        }
    }
}

void app_main(void) {
    printf("Testing servo motor...\n");
    // Create a FreeRTOS task for servo control
    xTaskCreate(servo_control_task, "servo_control_task", 4096, NULL, 5, NULL);
}
