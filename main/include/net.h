#ifndef _NET_H
#define _NET_H
// all your includes, data structures and definitions go below this

#include "main.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_spi_flash.h"

#include "main.h"

#include "strand.hpp"

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

#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"

#ifdef __cplusplus
  extern "C" {
#endif

extern QueueHandle_t xQueue_broadcast_task;
extern QueueHandle_t xQueue_multicast_task;
extern QueueHandle_t xQueue_tcp_task;
extern QueueHandle_t xQueue_tcp_respond;

extern tcp_task_action_t tcp_queue_value;

extern location_t device_location;
extern stepper_t device_stepper;
/* FreeRTOS event group to signal when we are connected & ready to make a request */

//tcp stuff 
void nvs_init(void);
int32_t nvs_get_value(char* name);
void nvs_set_value(char* name, int32_t value);
location_t command_init_location();
stepper_t command_init_stepper();

void setPramamter(int type, int value);
void saveParamters();

void initialise_wifi(void);
void wait_for_ip();
void init_wifi();

void simple_ota_example_task(void * pvParameter);
int get_command_line(char* a, int type);
void send_udp(char* udp_message, char* ip_address, int port);
void broadcast_task(void *pvParameters);

static int socket_add_ipv4_multicast_group(int sock, bool assign_source_if);
static int create_multicast_ipv4_socket();
void multicast_task(void *pvParameters);

void tcp_task(void *pvParameters);
void tcp_server_run();
void tcp_task_init();

void command_handler(char * queue_value, int type);
void server_ping(char* command);
void server_message(char* command,char* message);
void command_ota(void);

void wave_task(void *args);
void wave_command(int x, int y, int z, int speed, int type, int move, int stepper_speed, int accel, int min, int max);
int deviceDistanceSpeed(int x, int y, int z, int s);
// all of your header goes above this
void sine_task(void *args);
void sine_command(int type, int move, int stepper_speed, int accel, int min, int max, int loops, int offset);
void sine_wave_task(void *args);
void sine_wave_command(int x, int y, int z, int speed, int type, int move, int stepper_speed, int accel, int min, int max, int loops, int offset);
#ifdef __cplusplus
  }
#endif

#endif // _NET_H