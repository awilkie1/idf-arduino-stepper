
#include <esp_log.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>      /* printf */
#include <stdlib.h>     /* strtol */

#ifdef __cplusplus
    extern "C" {
#endif

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