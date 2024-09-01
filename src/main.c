/* UCM Timing System Embedded Software
*/

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "esp_adc/adc_continuous.h"
#include "driver/gptimer.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wifi.h"
#include "esp_timer.h"
#include "esp_system.h"

#define STAT0_LED         18
#define IR_PIN            4 
#define STACK_DEPTH       2048

static bool ir_beam_cut = 0;
static esp_timer_handle_t timer_handle;

static const char *TAG = "Parent";

typedef struct {
    bool timer_enable;
    uint64_t start_time;
    float lap_time;
    esp_timer_handle_t gptimer;
} gptimer_args;

static gptimer_args params;


static void install_isr()
{
    static bool isr_init = 0;
    if (!isr_init) {
        ESP_ERROR_CHECK(gpio_install_isr_service(0));
        isr_init = true;
    }
}


void setPinISR(gpio_num_t pin_num, gpio_int_type_t intr_type, gpio_isr_t interrupt, void* args) 
{
    install_isr();

    gpio_set_intr_type(pin_num, intr_type);
    ESP_ERROR_CHECK(gpio_isr_handler_remove(pin_num));
    ESP_ERROR_CHECK(gpio_isr_handler_add(pin_num, interrupt, args));
}


void ISRHandler(void* args)
{
  ir_beam_cut = 1;
  // Had to cast to double instead of float as using the FPU does not work during an ISR
  // I think it is generally unwise to be doing this here, will move later
  params.lap_time = ((double)(esp_timer_get_time() - params.start_time)) / (double)1000000.0; 
}


void sendOnInterrupt(void *pvParameters)
{
  gptimer_args *params = (gptimer_args *)pvParameters;
  bool toggle_led = 0;
  char msg[21];

  while(1) {
    if (ir_beam_cut) {
      toggle_led = !toggle_led;
      gpio_set_level(STAT0_LED, toggle_led);
      ir_beam_cut = 0;
      
      params->start_time = esp_timer_get_time();
      sprintf(msg, "%.4f", params->lap_time);
      ESP_LOGI(TAG, "Laptime %s", msg);
      send_data(0, msg); // Send laptime over Wifi
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}


void timer_callback(void* args) {
  return;
}


void app_main(void) 
{
  init_wifi();
 
  const esp_timer_create_args_t timer_args = {
    .callback = timer_callback,
    .name = "UCM Timing Gate",
    .arg = NULL,
    .dispatch_method = ESP_TIMER_TASK
  };

  ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer_handle));

  params.timer_enable = 0;
  params.gptimer = timer_handle;

  setPinISR(IR_PIN, GPIO_INTR_NEGEDGE, ISRHandler, NULL); 

  params.start_time = esp_timer_get_time();
  ESP_ERROR_CHECK(esp_timer_start_periodic(params.gptimer, 1000));

  xTaskCreate(sendOnInterrupt,"Read ADC", STACK_DEPTH, &params, 1, NULL);
}