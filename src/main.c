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
#include "esp_system.h"


#define SENSOR_POLL_RATE  50
#define STATUS_LED_RATE   1
#define PACER_RATE        5
#define BLUETOOTH_RATE    50
#define STAT0_LED         18
#define STAT1_LED         19
#define CPU_FREQ          160000000 
#define ADC_READ_LEN      2
#define STACK_DEPTH       2048

#define ADC_UNIT            ADC_UNIT_1
#define ADC_CHANNEL         ADC_CHANNEL_4  
#define ADC_BUFFER_SIZE     1024


static const char *TAG = "MY_TAG";

typedef struct {
    uint8_t result;
    bool timer_enable;
    uint64_t start_time;
    uint64_t finish_time;
    gptimer_handle_t gptimer;
    adc_continuous_handle_t handle;
} adc_params;


// static void adc_init()
// {
//   adc_continuous_handle_t adc_handle;

//   adc_continuous_handle_cfg_t adc_config = {
//     .max_store_buf_size = ADC_BUFFER_SIZE,
//     .conv_frame_size = ADC_BUFFER_SIZE,
//   };

//   //   adc_continuous_new_handle(&adc_config, &adc_handle);
    
//     adc_continuous_config_t adc_chan_config = {
//         .pattern_num = 1,
//         .sample_freq_hz = 1 * 1000,  // 1 kHz sampling frequency

//     };

//     adc_digi_pattern_config_t digi_cfg = {
//       .channel = ADC_CHANNEL,
//     };
//     adc_continuous_config(adc_handle, &adc_chan_config);

//   // //adc_digi_pattern_config_t adc_pattern = {0};
//   // adc_chan_config.pattern_num = ADC_CHANNEL;
//   // //adc_chan_config.adc_pattern = adc_pattern;

// }




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
  adc_params *params = (adc_params *)pvParameters;

  while(1)
  {
    //adc_continuous_read(params->handle, &(params->result), 256, 0, 0);
    //ESP_LOGI(TAG, "Result: %u", params->result);
    if (params->result > 2000) {
      if (params->timer_enable) {
        !(params->timer_enable);
        gptimer_stop(params->gptimer);
        params->finish_time = gptimer_get_raw_count(params->gptimer, &(params->finish_time));
        uint64_t lap_time = params->finish_time - params->start_time;
        char msg[21];
        sprintf(msg, "%" PRIu64, lap_time);
        ESP_LOGI(TAG, "Laptime %u", msg);
        send_data(0, &msg); // Send laptime over Wifi
      } else {
        params->start_time = gptimer_get_raw_count(params->gptimer, &(params->start_time));
        gptimer_start(params->gptimer);
        
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
    
    
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




  /* Configuring the ADC */
  uint8_t result = 0;
  //adc_init();

  adc_continuous_handle_t adc_handle = NULL;

  adc_continuous_handle_cfg_t adc_config = {
    .max_store_buf_size = ADC_BUFFER_SIZE,
    .conv_frame_size = ADC_BUFFER_SIZE,
  };

  //   adc_continuous_new_handle(&adc_config, &adc_handle);
    
    adc_continuous_config_t adc_chan_config = {
        .pattern_num = 1,
        .sample_freq_hz = 1 * 1000,  // 1 kHz sampling frequency

    };

    adc_digi_pattern_config_t digi_cfg = {
      .channel = ADC_CHANNEL,
    };
    adc_continuous_config(adc_handle, &adc_chan_config);

  // //adc_digi_pattern_config_t adc_pattern = {0};
  // adc_chan_config.pattern_num = ADC_CHANNEL;
  // //adc_chan_config.adc_pattern = adc_pattern;

  adc_continuous_start(adc_handle);





  gptimer_handle_t gptimer = NULL;
  gptimer_config_t timer_config = {
      .clk_src = GPTIMER_CLK_SRC_DEFAULT,
      .direction = GPTIMER_COUNT_UP,
      .resolution_hz = 1 * 1000 * 1000, // 1MHz, 1 tick = 1us
  };
  gptimer_new_timer(&timer_config, &gptimer);
  gptimer_enable(gptimer);


  adc_params params;
  params.timer_enable = 0;
  params.result = result;
  params.handle = adc_handle;
  params.gptimer = gptimer;



  xTaskCreate(task1,"Blink LED 1", STACK_DEPTH, NULL, 1, NULL);
  xTaskCreate(task2,"Blink LED 2", STACK_DEPTH, NULL, 1, NULL);
  xTaskCreate(task3,"Read ADC", STACK_DEPTH, &params, 1, NULL);

  

}


