#include "main.h"

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_spi_flash.h"

#include <Arduino.h>

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"

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

// extern "C" {
//     // Any libraries written in  C++ should be included here
//     #include "Stepper.h"
// }

extern "C" {
    // Any libraries written in  C++ should be included here
    #include "stepping.h"
}
#include "net.h"

static const char *TAG = "STARTUP";

TaskHandle_t multicast_task_handle = NULL;
TaskHandle_t broadcast_task_handle = NULL;
TaskHandle_t tcp_task_handle = NULL;

TaskHandle_t stepper_task_handle = NULL;


extern "C" void app_main()
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set(TAG, ESP_LOG_INFO);

    nvs_init();

   //  ESP_ERROR_CHECK( err );
    
    //SHORT DELAY TO NOT OVERLOAD NETWORK
    srand (time(NULL));
    int boot_delay = (esp_random() % RANDOM_BOOT_DELAY_PERIOD + 1) * 1000;
    vTaskDelay(boot_delay / portTICK_PERIOD_MS);

    //initialise_wifi();
    init_wifi();

    //PULL IN FROM NVS THE LOCATION
    device_location = command_init_location();
    
    xTaskCreate(&tcp_task, "tcp_task", 3072, NULL, 3, &tcp_task_handle);
    xTaskCreate(&multicast_task, "multicast_task", 4096, NULL, 10, &multicast_task_handle);
    xTaskCreate(&broadcast_task, "broadcast_task", 4096, NULL, 10, &broadcast_task_handle);

    xTaskCreate(&stepper_task, "stepper_task", 3072, NULL, 3, &stepper_task_handle);
    
    server_ping("boot");//Sends the boot up message to the server

    char multicast_queue_value[COMMAND_ITEM_SIZE];
    char broadcast_queue_value[COMMAND_ITEM_SIZE];


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
         if (xQueueReceive(xQueue_stepper_task, &stepper_queue_value, 0)){
            ESP_LOGD(TAG,"Recieved Stepper command");
         }
         vTaskDelay(10);
    }
}