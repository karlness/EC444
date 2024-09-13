#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "soc/uart_struct.h"
#include "string.h"
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include <math.h>


#define SAMPLE_NUM 64
#define DEFAULT_VREF 1100



static esp_adc_cal_characteristics_t  *adc_chars;
static const adc1_channel_t channel = ADC1_CHANNEL_3;
static const adc_atten_t  atten = ADC_ATTEN_DB_11;
static const adc_unit_t unit = ADC_UNIT_1;

void init_thermistor() {
  //configure ADC
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(channel, atten);
  adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_chars);
    
    float gettemp() {
      // Initialize reading variable
      uint32_t val = 0;
      for (int i = 0; i < SAMPLE_NUM; i++){
        val += adc1_get_raw(channel);
      }
      val /= SAMPLE_NUM; // Average ADC value
      printf("Val: %ld\n", val);

      // Calculate thermistor resistance
      float rtherm;
      const float R_DIVIDER = 2000;
      rtherm = (R_DIVIDER * val) / (4096.0 - val);
      printf("rtherm: %.2f\n", rtherm);
        //b constant value source
          //https://envistiamall.com/products/ntc-thermistor-temperature-sensor-probe-10k-1-3950-waterproof-1m-length
          // Calculate temperature using Steinhart-Hart equation
          const float B_CONSTANT = 3950;
          const float REF_RESISTANCE = 2000.0; // Reference resistance 2000Ω at 25°C
          const float REF_TEMPERATURE = 298.15; // Reference temperature in Kelvin (25°C)
        float temp;
         temp = rtherm / REF_RESISTANCE; // (R/Ro)
         //temp = log(temp); // ln(R/Ro)
         temp /= B_CONSTANT; // 1/B * ln(R/Ro)
         temp += 1.0 / REF_TEMPERATURE; // + (1/To)
         temp = 1.0 / temp;
         temp -= 273.15; // Convert from Kelvin to Celsius

         return temp;
       }
    
    
    void app_main() {
      init_thermistor();
      while (1) {
        float temp = gettemp();
        printf("Temp: %.6f°C\n", temp);
        vTaskDelay(pdMS_TO_TICKS(2000));
      }
