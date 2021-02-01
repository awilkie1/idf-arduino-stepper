/* Hello World Example
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
   Adapted by Oliver Copleston - March 2020
   
   ----README----
   - Put arduino libraries in the Libraries folder
   - All additional source files in the src folder
   - All additional header files in the include folder
*/

#include "main.h"

#include "sdkconfig.h"

#include "esp_spi_flash.h"

#include <Arduino.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <inttypes.h>

#include "nvs.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "netdb.h"
#include "driver/uart.h"

// Solenoid includes
#include "driver/gpio.h"
#include <driver/dac.h>
#include "driver/ledc.h"

extern "C" {
    // Any libraries written in  C++ should be included here
   //  #include "Stepper.h"
}
#include "net.h"
#include "strand.hpp"

#define LEDC_HS_TIMER          LEDC_TIMER_0
#define LEDC_HS_MODE           LEDC_HIGH_SPEED_MODE
#define LEDC_HS_CH0_GPIO       (25)
#define LEDC_HS_CH0_CHANNEL    LEDC_CHANNEL_0
#define LEDC_HS_CH1_GPIO       (19)
#define LEDC_HS_CH1_CHANNEL    LEDC_CHANNEL_1

#define LEDC_LS_TIMER          LEDC_TIMER_1
#define LEDC_LS_MODE           LEDC_LOW_SPEED_MODE
#define LEDC_LS_CH2_GPIO       (25)
#define LEDC_LS_CH2_CHANNEL    LEDC_CHANNEL_2
#define LEDC_LS_CH3_GPIO       (5)
#define LEDC_LS_CH3_CHANNEL    LEDC_CHANNEL_3

#define LEDC_TEST_CH_NUM       (4)
#define LEDC_TEST_DUTY         (1023)
#define LEDC_TEST_FADE_TIME    (2000)

#define SOL_PIN 25
const gpio_num_t sol_pin_out = (gpio_num_t) SOL_PIN;

static const char *TAG = "STARTUP";

TaskHandle_t multicast_task_handle = NULL;
TaskHandle_t broadcast_task_handle = NULL;
TaskHandle_t tcp_task_handle = NULL;
TaskHandle_t stepper_task_handle = NULL;
//TaskHandle_t sensor_task_handle = NULL;

TaskHandle_t wave_task_handle = NULL;
TaskHandle_t sine_task_handle = NULL;
TaskHandle_t sine_wave_task_handle = NULL;

QueueSetHandle_t queue_set;
QueueSetMemberHandle_t queue_set_member;

extern "C" void app_main() {    
      
   Serial.begin(115200);
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set(TAG, ESP_LOG_INFO);
   
    initArduino();

    nvs_init();

   //SHORT DELAY TO NOT OVERLOAD NETWORK
   //  srand (time(NULL));
   //  int boot_delay = (esp_random() % RANDOM_BOOT_DELAY_PERIOD + 1) * 1000;
   //  vTaskDelay(boot_delay / portTICK_PERIOD_MS);

   init_wifi();

   esp_wifi_set_ps(WIFI_PS_NONE);

   //PULL IN FROM NVS THE LOCATION
   device_location = command_init_location();
   device_stepper = command_init_stepper();
    
   xTaskCreate(&tcp_task, "tcp_task", 3072, NULL, 3, &tcp_task_handle);
   xTaskCreate(&multicast_task, "multicast_task", 4096, NULL, 3, &multicast_task_handle);
   xTaskCreate(&broadcast_task, "broadcast_task", 4096, NULL, 3, &broadcast_task_handle);
   
   char multicast_queue_value[COMMAND_ITEM_SIZE];
   char broadcast_queue_value[COMMAND_ITEM_SIZE];
   //char tcp_queue_value[COMMAND_ITEM_SIZE];
   
   server_ping("boot");//Sends the boot up message to the server
   
   ESP_LOGI(TAG,"Boot Position %d",device_stepper.current);
   init_strand(device_stepper.current); // Start the stepper motor system

   xTaskCreatePinnedToCore(&stepper_task, "stepper_task", 8*1024, NULL, 4, &stepper_task_handle, 1);

   esp_task_wdt_delete(NULL); // remove from watchdog

   // xTaskCreatePinnedToCore(&sensor_task, "sensor_task", 1024, NULL, 3, &sensor_task_handle, 0);

   xTaskCreatePinnedToCore(&wave_task, "wave_tasks", 2048, NULL, 3, &wave_task_handle, 0);
   xTaskCreatePinnedToCore(&sine_task, "sine_tasks", 2048, NULL, 3, &sine_task_handle, 0);
   xTaskCreatePinnedToCore(&sine_wave_task, "sine_wave_tasks", 2048, NULL, 3, &sine_wave_task_handle, 0);

   String inString = ""; // String to hold input
   int inNum = 0;

   // int commands[] = {5000, -10000, 5000, 0, 20000, 0, -20000, 20000, 15000, 0};
   // for (int i = 0; i<10; i++) {
   //    xQueueSendToBack(xQueue_stepper_command, (void *) &commands[i], 0);
   //    vTaskDelay(pdMS_TO_TICKS(10));
   // }

   // while(1) {
   //    // while (Serial.available() == 0) {
   //    //    vTaskDelay(pdMS_TO_TICKS(100));
   //    // };
   //    if (Serial.available() > 0) {
   //       int val = Serial.parseInt();
   //       ESP_LOGW(TAG, "Number Input: %i", val);
   //       xQueueSendToBack(xQueue_stepper_command, (void *) &val, 0);
   //    }
   //    vTaskDelay(pdMS_TO_TICKS(100));
   // }


   queue_set = xQueueCreateSet(3);                    // Create QueueSet
   vTaskDelay(10);
   // Add all of the networking queue to the set
   xQueueAddToSet(xQueue_multicast_task, queue_set);
   xQueueAddToSet(xQueue_broadcast_task, queue_set);
   xQueueAddToSet(xQueue_tcp_task, queue_set);

   // -------- PCB TESTING --------

   // for (int i=0; i<2; i++){
   //    command_move(0, 10000, 3200, 3000, 0, 10000);
   //    command_move(0, -10000, 3200, 3000, 0, 10000);
   //    // vTaskDelay(pdMS_TO_TICKS(1000));
   // }
   // command_move(0, 40000, 3000, 3000, 0, 10000);
   // gpio_pad_select_gpio(sol_pin_out);
   // gpio_set_direction(sol_pin_out, GPIO_MODE_OUTPUT);
   // dac_output_enable(DAC_CHANNEL_1);
   // while(1) {
   //    printf("start ADC \n");
   //    for (int i=255; i>0; i--) {
   //       printf("ADC value: %i \n", i);
   //       dac_output_voltage(DAC_CHANNEL_1, i);
   //       printf("wait\n");
   //       vTaskDelay(pdMS_TO_TICKS(100));
   //    }
   // }

   /*
   * Prepare and set configuration of timers
   * that will be used by LED Controller
   */
   ledc_timer_config_t ledc_timer;
      ledc_timer.duty_resolution = LEDC_TIMER_10_BIT; // resolution of PWM duty
      ledc_timer.freq_hz = 18000;                      // frequency of PWM signal 18000
      ledc_timer.speed_mode = LEDC_HS_MODE;           // timer mode
      ledc_timer.timer_num = LEDC_HS_TIMER;            // timer index

   // Set configuration of timer0 for high speed channels
   ledc_timer_config(&ledc_timer);
   ledc_channel_config_t ledc_channel = {0};
   ledc_channel.channel    = LEDC_HS_CH0_CHANNEL;
   ledc_channel.duty       = 0;
   ledc_channel.gpio_num   = LEDC_HS_CH0_GPIO;
   ledc_channel.speed_mode = LEDC_HS_MODE;
   ledc_channel.hpoint     = 0;
   ledc_channel.timer_sel  = LEDC_HS_TIMER;

   // Set LED Controller with previously prepared configuration
   ledc_channel_config(&ledc_channel);

   // Initialize fade service.
   ledc_fade_func_install(0);

   while(1) {
      // printf("Fade up\n");
      // ledc_set_fade_with_time(ledc_channel.speed_mode,
      //          ledc_channel.channel, LEDC_TEST_DUTY, LEDC_TEST_FADE_TIME);
      // ledc_fade_start(ledc_channel.speed_mode,
      //          ledc_channel.channel, LEDC_FADE_NO_WAIT);

      // vTaskDelay(LEDC_TEST_FADE_TIME / portTICK_PERIOD_MS);

      // printf("Fade down\n");
      // ledc_set_fade_with_time(ledc_channel.speed_mode,
      //          ledc_channel.channel, 850, LEDC_TEST_FADE_TIME);
      // ledc_fade_start(ledc_channel.speed_mode,
      //          ledc_channel.channel, LEDC_FADE_NO_WAIT);
      
      printf("PWM On\n");
      ledc_set_duty(ledc_channel.speed_mode, ledc_channel.channel, 800);
      ledc_update_duty(ledc_channel.speed_mode, ledc_channel.channel);

      vTaskDelay(LEDC_TEST_FADE_TIME / portTICK_PERIOD_MS);
   }


   // while(1) {
   //    printf("Turning off Pin\n");
   //    gpio_set_level(sol_pin_out, 0);
   //    vTaskDelay(pdMS_TO_TICKS(1000));

   //    printf("Turning on Pin\n");
   //    gpio_set_level(sol_pin_out, 1);
   //    vTaskDelay(pdMS_TO_TICKS(1000));
   // }





   // Block the task until we receive a value from any of the queues
   while ((queue_set_member = xQueueSelectFromSet(queue_set, portMAX_DELAY))) {
      // Determine which queue has values ready to receive, and receive those values
      if ((queue_set_member == xQueue_multicast_task) && (xQueueReceive(xQueue_multicast_task, &multicast_queue_value, 10))) {
         ESP_LOGD(TAG,"Recieved multicast command %s\n", multicast_queue_value);
         command_handler(multicast_queue_value, 0);
      }
      if ((queue_set_member == xQueue_broadcast_task) && (xQueueReceive(xQueue_broadcast_task, &broadcast_queue_value, 10))) {
         ESP_LOGD(TAG,"Recieved broadcast command %s\n", broadcast_queue_value);
         command_handler(broadcast_queue_value, 0);
      }
      if ((queue_set_member == xQueue_tcp_task) && (xQueueReceive(xQueue_tcp_task, &tcp_queue_value, 10))) {
         ESP_LOGD(TAG,"Recieved tcp command %s\n", tcp_queue_value.action_value);
         command_handler(tcp_queue_value.action_value, 1);
      }
   }
   
}