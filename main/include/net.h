#ifndef NET_H
#define NET_H
#ifdef __cplusplus
  extern "C" {
#endif
// all your includes, data structures and definitions go below this


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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_spi_flash.h"

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


char multicast_queue_value[COMMAND_ITEM_SIZE];
char broadcast_queue_value[COMMAND_ITEM_SIZE];

char command_line[COMMAND_ITEMS][50];

QueueHandle_t xQueue_broadcast_task;
QueueHandle_t xQueue_multicast_task;
QueueHandle_t xQueue_tcp_task;
QueueHandle_t xQueue_tcp_respond;

TaskHandle_t multicast_task_handle = NULL;
TaskHandle_t broadcast_task_handle = NULL;
TaskHandle_t tcp_task_handle = NULL;


//tcp stuff 
typedef struct tcp_task_actions {
   int   action;
   char  action_value[COMMAND_ITEM_SIZE];
} tcp_task_action_t;

tcp_task_action_t tcp_queue_value;

//NVS

typedef struct location {
    int32_t x;
    int32_t y;
    int32_t z;
} location_t;

location_t device_location;

uint32_t my_handle;

esp_err_t nvs_err;

//OTA
static const char *TAG = "simple_ota_example";
extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;

//TCP Stuff
char rx_buffer[128];
char addr_str[128];
int addr_family;
int ip_protocol;

int listen_sock;
int err;
char respond_value[COMMAND_ITEM_SIZE];

// all of your header goes above this
#endif // NET_H