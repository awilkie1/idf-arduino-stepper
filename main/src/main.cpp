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
#include "net.h"


//DELAYS 
#define RANDOM_BOOT_DELAY_PERIOD 1 // seconds over which boot can be randomly delayed to avoid flooding the network at the same time
#define RANDOM_OTA_DELAY_PERIOD 5 // seconds over which OTA can be randomly delayed to avoid flooding the network at the same time

// WIFI Network Connection
#define _WIFI_SSID "_bloom"
#define _WIFI_PASS "sqU1d0ak"

#define SERVER_IP_ADDRESS "10.0.2.10"
#define SERVER_PORT 9999

// MULTICAST Communication
#define MULTICAST_IPV4_ADDR "224.1.1.10"
#define MULTICAST_PORT 2704
#define MULTICAST_TTL 32

// BROADCAST Communication
#define PORT 20002

// Path to software upgrade file
#define CONFIG_FIRMWARE_UPGRADE_URL "http://10.0.2.10:8888/bloom/code/firmware.bin"

// number of individual commands allowed in command line
#define COMMAND_ITEMS 35

// maximum command line length in chars
#define COMMAND_ITEM_SIZE 400

static const char *TAG = "STARTUP";

TaskHandle_t multicast_task_handle = NULL;
TaskHandle_t broadcast_task_handle = NULL;
TaskHandle_t tcp_task_handle = NULL;


extern "C" {
    // Any libraries written in C (not C++) should be included here
}

extern "C" void app_main()
{
    nvs_init();

    ESP_ERROR_CHECK( err );
    
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
    
    server_ping("boot");//Sends the boot up message to the server
   
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