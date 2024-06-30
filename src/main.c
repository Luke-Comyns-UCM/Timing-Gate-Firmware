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


#define SENSOR_POLL_RATE  50
#define STATUS_LED_RATE   1
#define PACER_RATE        5
#define BLUETOOTH_RATE    50
#define STAT0_LED         18
#define STAT1_LED         19
#define IR_PIN            4
#define CPU_FREQ          160000000 
#define ADC_READ_LEN      2
#define STACK_DEPTH       2048

#define ADC_UNIT            ADC_UNIT_1
#define ADC_CHANNEL         ADC_CHANNEL_4  
#define ADC_BUFFER_SIZE     1024

static bool ir_beam_cut = 0;
static esp_timer_handle_t timer_handle;


static const char *TAG = "MY_TAG";

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



void ISR_handler(void* args)
{
  static bool toggle_led1 = 0;
  ir_beam_cut = 1;
  params.lap_time = ((float)(esp_timer_get_time() - params.start_time))/1000000.0;
  toggle_led1 = !toggle_led1;
  gpio_set_level(STAT1_LED, toggle_led1);
}

void task1(void *pvParameters)
{

  
  while(1) 
  {
    gpio_set_level(STAT0_LED, 1);
    vTaskDelay(pdMS_TO_TICKS(500));
    gpio_set_level(STAT0_LED, 0);
    vTaskDelay(pdMS_TO_TICKS(500));
   
  }
}


void task2(void *pvParameters)
{

  while(1) 
  {
    gpio_set_level(STAT1_LED, 1);
    vTaskDelay(pdMS_TO_TICKS(1000));
    gpio_set_level(STAT1_LED, 0);
    vTaskDelay(pdMS_TO_TICKS(1000));
   
  }

}




void task3(void *pvParameters)
{
  gptimer_args *params = (gptimer_args *)pvParameters;
  bool toggle_led = 0;
  char msg[21];
  while(1)
  {
    //adc_continuous_read(params->handle, &(params->result), 256, 0, 0);
    //ESP_LOGI(TAG, "Result: %u", params->result);


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


  //char* msg = "Hello Darkness my old friend";
  init_wifi();
 


  /* Configuring STAT0 LED */
  gpio_config_t stat0_cfg = {
    .mode = GPIO_MODE_OUTPUT,
    .pin_bit_mask = 1ULL<<STAT0_LED,
  };
  gpio_config(&stat0_cfg);




  /* Configuring STAT1 LED */
  gpio_config_t stat1_cfg = {
    .mode = GPIO_MODE_OUTPUT,
    .pin_bit_mask = 1ULL<<STAT1_LED,
  };
  gpio_config(&stat1_cfg);

  const esp_timer_create_args_t timer_args = {
    .callback = timer_callback,
    .name = "UCM Timing Gate",
    .arg = NULL,
    .dispatch_method = ESP_TIMER_TASK
  };

  ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer_handle));

  params.timer_enable = 0;
  params.gptimer = timer_handle;

  setPinISR(IR_PIN, GPIO_INTR_NEGEDGE, ISR_handler, NULL); 


  params.start_time = esp_timer_get_time();
  ESP_ERROR_CHECK(esp_timer_start_periodic(params.gptimer, 1000));


  //xTaskCreate(task1,"Blink LED 1", STACK_DEPTH, NULL, 1, NULL);
  //xTaskCreate(task2,"Blink LED 2", STACK_DEPTH, NULL, 1, NULL);
  xTaskCreate(task3,"Read ADC", STACK_DEPTH, &params, 1, NULL);

  

}


