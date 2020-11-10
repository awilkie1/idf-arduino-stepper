#ifndef _MAIN_H
#define _MAIN_H
// ---- add your code below ----


#include <esp_log.h>
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include <string.h>
#include <stdint.h>
#include <stdio.h>      /* printf */
#include <stdlib.h>     /* strtol */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"
#include "esp_task_wdt.h"

#ifdef __cplusplus
    extern "C" {
#endif

//DELAYS 
#define RANDOM_BOOT_DELAY_PERIOD 1 // seconds over which boot can be randomly delayed to avoid flooding the network at the same time
#define RANDOM_OTA_DELAY_PERIOD 5 // seconds over which OTA can be randomly delayed to avoid flooding the network at the same time

// WIFI Network Connection
#define _WIFI_SSID "_bloom_strand"
//#define _WIFI_SSID "_bloom_mesh" // Ollie's test env
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
#define COMMAND_ITEMS 40

// maximum command line length in chars
#define COMMAND_ITEM_SIZE 400

typedef struct location {
    int32_t x;
    int32_t y;
    int32_t z;
} location_t;

typedef struct stepper {
    int32_t current;
    int32_t min;
    int32_t max;
    int32_t target;
    int32_t number;
} stepper_t;

typedef struct stepper_command {
    int min;
    int max;
    int speed;
    int accel;
    int move;
    int type;
    int time;
} stepper_command_t;

typedef struct {
   int   action;
   char  action_value[COMMAND_ITEM_SIZE];
} tcp_task_action_t;

typedef struct wave {
    int32_t x;
    int32_t y;
    int32_t z;
    int32_t speed;

    stepper_command_t wave_stepper;
} wave_t;

typedef struct sine {
    int32_t loops;
    int32_t offset;
    stepper_command_t sine_stepper;
} sine_t;

typedef struct sine_wave {
    int32_t loops;
    int32_t offset;
    int32_t x;
    int32_t y;
    int32_t z;
    int32_t speed;
    stepper_command_t sine_wave_stepper;
} sine_wave_t;

#ifdef __cplusplus
  }
#endif

// ---- add your code above ----
#endif // _MAIN_H