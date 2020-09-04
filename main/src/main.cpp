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

extern "C" {
    // Any libraries written in  C++ should be included here
   //  #include "Stepper.h"
}
#include "net.h"
#include "strand.hpp"

static const char *TAG = "STARTUP";

TaskHandle_t multicast_task_handle = NULL;
TaskHandle_t broadcast_task_handle = NULL;
TaskHandle_t tcp_task_handle = NULL;
TaskHandle_t stepper_task_handle = NULL;

extern "C" void app_main() {    
      
   Serial.begin(115200);
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set(TAG, ESP_LOG_INFO);
   
    initArduino();

    nvs_init();

   //  ESP_ERROR_CHECK( err );
   // Serial.begin(115200);
    
    //SHORT DELAY TO NOT OVERLOAD NETWORK
   //  srand (time(NULL));
   //  int boot_delay = (esp_random() % RANDOM_BOOT_DELAY_PERIOD + 1) * 1000;
   //  vTaskDelay(boot_delay / portTICK_PERIOD_MS);

   init_wifi();

   //PULL IN FROM NVS THE LOCATION
   device_location = command_init_location();
    
    xTaskCreate(&tcp_task, "tcp_task", 3072, NULL, 3, &tcp_task_handle);
    xTaskCreate(&multicast_task, "multicast_task", 4096, NULL, 10, &multicast_task_handle);
    xTaskCreate(&broadcast_task, "broadcast_task", 4096, NULL, 10, &broadcast_task_handle);
    
   //  server_ping("boot");//Sends the boot up message to the server

    char multicast_queue_value[COMMAND_ITEM_SIZE];
    char broadcast_queue_value[COMMAND_ITEM_SIZE];
    char tcp_queue_value[COMMAND_ITEM_SIZE];
   
   server_ping("boot");//Sends the boot up message to the server

   init_strand(); // Start the stepper motor system
   xTaskCreatePinnedToCore(&stepper_task, "stepper_task", 2*1024, NULL, 2, &stepper_task_handle, 0);

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

   
   
    while(1){
         if (xQueueReceive(xQueue_multicast_task, &multicast_queue_value, 0)){
            ESP_LOGD(TAG,"Recieved multicast command %s\n", multicast_queue_value);
            command_handler(multicast_queue_value, 0);
         }
         if (xQueueReceive(xQueue_broadcast_task, &broadcast_queue_value, 0)){
            ESP_LOGD(TAG,"Recieved broadcast command %s\n", broadcast_queue_value);
            command_handler(broadcast_queue_value, 0);
         }
         if (xQueueReceive(xQueue_tcp_task, &tcp_queue_value, 0)){
            ESP_LOGD(TAG,"Recieved tcp command %s\n", tcp_queue_value.action_value);
            command_handler(tcp_queue_value.action_value, 1);
         }
         vTaskDelay(10);
    }
}