
#include "main.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_spi_flash.h"

#include <Arduino.h>

#include "net.h"

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

#include <getopt.h> /* getopt */

#ifdef __cplusplus
  extern "C" {
#endif


uint64_t multicast_id = 1;
char multicast_queue_value[COMMAND_ITEM_SIZE];
char broadcast_queue_value[COMMAND_ITEM_SIZE];

char command_line[COMMAND_ITEMS][50];

QueueHandle_t xQueue_broadcast_task;
QueueHandle_t xQueue_multicast_task;
QueueHandle_t xQueue_tcp_task;
QueueHandle_t xQueue_tcp_respond;

QueueHandle_t xQueue_wave_task;
wave_t wave = {0};
QueueHandle_t xQueue_sine_task;
sine_t sine = {0};
QueueHandle_t xQueue_sine_wave_task;
sine_wave_t sine_wave = {0};

tcp_task_action_t tcp_queue_value;

location_t device_location;
stepper_t device_stepper;

//NVS

uint32_t my_handle;

esp_err_t nvs_err;

static EventGroupHandle_t wifi_event_group;

// ESP_ERROR_CHECK( err );

//OTA
static const char *TAG = "NET";
extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");


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

//PARAMTER SAVING
void nvs_init(){
    // Initialize NVS.
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // 1.OTA app partition table has a smaller NVS partitionb size than the non-OTA
        // partition table. This size mismatch may cause NVS initialization to fail.
        // 2.NVS partition contains data in new format and cannot be recognized by this version of code.
        // If this happens, we erase NVS partition and initialize NVS again.
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_err = nvs_flash_init();
    }
}
int32_t nvs_get_value(char* name){

    int32_t value = 0;

    nvs_err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (nvs_err != ESP_OK) {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(nvs_err));
    } else {
        printf("Done\n");

        // Read
        printf("Reading restart counter from NVS ... ");
        nvs_err = nvs_get_i32(my_handle, name, &value);
        switch (nvs_err) {
            case ESP_OK:
                ESP_LOGI(TAG,"Done\n");
                ESP_LOGI(TAG,"Retrieved value counter = %d\n", value);
                break;
            case ESP_ERR_NVS_NOT_FOUND:
                ESP_LOGI(TAG,"The value is not initialized yet!\n");
                break;
            default :
                ESP_LOGI(TAG,"Error (%s) reading!\n", esp_err_to_name(nvs_err));
        }
    }
     // Close
     nvs_close(my_handle);
     return value;

}
void nvs_set_value(char* name, int32_t value){

    nvs_err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (nvs_err != ESP_OK) {
        ESP_LOGI(TAG,"Error (%s) opening NVS handle!\n", esp_err_to_name(nvs_err));
    } else {
        ESP_LOGI(TAG,"Done\n");

    // Write
        nvs_err = nvs_set_i32(my_handle, name, value);

        // Commit written value.
        // After setting any values, nvs_commit() must be called to ensure changes are written
        // to flash storage. Implementations may write to storage at other times,
        // but this is not guaranteed.
        ESP_LOGI(TAG,"Committing updates in NVS ... ");
        nvs_err = nvs_commit(my_handle);
        // Close
        nvs_close(my_handle);

    }

}
//LOCATION SAVING
location_t command_init_location(){

  location_t location;

  ESP_LOGI(TAG, "LOAD LOCATION");

  location.x = nvs_get_value("location_x");
  location.y = nvs_get_value("location_y");
  location.z = nvs_get_value("location_z");
  return location;
}
esp_err_t command_set_location(location_t location){

    nvs_set_value("location_x",location.x);
    nvs_set_value("location_y",location.y);
    nvs_set_value("location_z",location.z);
    device_location = command_init_location();

    return ESP_OK;
}
//STEPPER SAVING
stepper_t command_init_stepper(){

  ESP_LOGI(TAG, "LOAD STEPPER");
  stepper_t stepper;
  stepper.current = nvs_get_value("stepper_current");
  stepper.min = nvs_get_value("stepper_min");
  stepper.max = nvs_get_value("stepper_max");
  stepper.target = nvs_get_value("stepper_target");
  stepper.number = nvs_get_value("stepper_number");
  return stepper;
}
esp_err_t command_set_stepper(stepper_t stepper){

    nvs_set_value("stepper_current",stepper.current);
    nvs_set_value("stepper_min",stepper.min);
    nvs_set_value("stepper_max",stepper.max);
    nvs_set_value("stepper_target",stepper.target);
    nvs_set_value("stepper_number",stepper.number);
    device_stepper = command_init_stepper();

    return ESP_OK;
}
void setPramamter(int type, int value){

    ESP_LOGI(TAG, "SET PARAMTER %d : %d", type, value);

    if (type==1) device_stepper.current = value;
    if (type==2) device_stepper.min = value;
    if (type==3) device_stepper.max = value;
    if (type==4) device_stepper.target = value;
    if (type==5) device_stepper.number = value;
    
}
void saveParamters(){
    stepper_t step;
    step.current = device_stepper.current;
    step.min = device_stepper.min;
    step.max = device_stepper.max;
    step.target = device_stepper.target;
    step.number = device_stepper.number;
    command_set_stepper(step);

    ESP_LOGI(TAG, "SET PARAMTER %d : %d : %d : %d : %d", step.current, step.min, step.max, step.target,step.number);
}
//WIFI
esp_err_t event_handler(void *ctx, system_event_t *event)

{
    switch (event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_START");
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_GOT_IP");
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
        ESP_LOGI(TAG, "SYSTEM_EVENT_DISSCONNECTED");
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_LOST_IP:
        ESP_LOGI(TAG, "SYSTEM_EVENT_LOST_IP");
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}
void initialise_wifi(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );//WIFI_STORAGE_FLASH
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = _WIFI_SSID,
            .password = _WIFI_PASS,
        },
    };
    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}
void wait_for_ip()
{
    uint32_t bits = CONNECTED_BIT;
    ESP_LOGI(TAG, "Waiting for AP connection...");
    xEventGroupWaitBits(wifi_event_group, bits, false, true, portMAX_DELAY);
    ESP_LOGI(TAG, "Connected to AP");
}
void init_wifi()
{
    initialise_wifi();
    wait_for_ip();//CUT this to ignore boot up looking for ip
}
//OTA
esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
    }
    return ESP_OK;
}
void simple_ota_example_task(void * pvParameter)
{
    ESP_LOGI(TAG, "Starting OTA example...");

    /* Wait for the callback to set the CONNECTED_BIT in the
       event group.
    */
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                        false, true, portMAX_DELAY);
    ESP_LOGI(TAG, "Connect to Wifi ! Start to Connect to Server....");
    
    esp_http_client_config_t config = {
        .url = CONFIG_FIRMWARE_UPGRADE_URL,
        .cert_pem = (char *)server_cert_pem_start,
        .event_handler = _http_event_handler,
    };
    esp_err_t ret = esp_https_ota(&config);
    if (ret == ESP_OK) {
        esp_restart();
    } else {
        ESP_LOGE(TAG, "Firmware Upgrades Failed");
    }
    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
//broadcast
int get_command_line(char* a, int type){

   const char s[2] = " ";
   char *token;

   int i = 0;
   /* get the first token */
   token = strtok(a, s);

    // check for multicast and identify duplicates
    if(type == 0){
        char buff[100];
        strncpy(buff,token,sizeof(buff));
        ESP_LOGD(TAG, "[TOKEN COPY] id %s", buff);
        token = strtok(NULL, s);
        if (atoi(buff) == multicast_id){
                ESP_LOGD(TAG, "[DUPLICATE] id %d", atoi(buff));
                strncpy(token, "duplicate", 10);
        } 
        multicast_id = atoi(buff);
    }

   /* walk through other tokens */
   while( token != NULL ) {
      strncpy(command_line[i], token, sizeof(command_line[i]));
      i++;
      token = strtok(NULL, s);
   }

   return i;

}
void send_udp(char* udp_message, char* ip_address, int port){

    char buffer[256];
    int addr_family = AF_INET;
    int ip_protocol = IPPROTO_IP;
    int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
    struct sockaddr_in sourceAddr; // Large enough for both IPv4 or IPv

    sourceAddr.sin_family = AF_INET;
    sourceAddr.sin_addr.s_addr = inet_addr(ip_address);
    sourceAddr.sin_port =  htons(port);;

    strcpy(buffer, udp_message);
    sendto(sock, buffer, strlen(buffer), 0, (struct sockaddr *)&sourceAddr, sizeof(sourceAddr));
    
    ESP_LOGI(TAG, "SENT MESSAGE %s : %s @ %d", udp_message, ip_address, port);
    
}
void broadcast_task(void *pvParameters)
{
    char rx_buffer[COMMAND_ITEM_SIZE + 1];
    char addr_str[128];
    int addr_family;
    int ip_protocol;

    while (1) {

         xQueue_broadcast_task = xQueueCreate( 5, sizeof(char[COMMAND_ITEM_SIZE]));
         
        if( xQueue_broadcast_task == NULL )
        {
            ESP_LOGI(TAG, "unable to create broadcast command queue");
        }

        struct sockaddr_in destAddr;
        destAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        destAddr.sin_family = AF_INET;
        destAddr.sin_port = htons(PORT);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;
        inet_ntoa_r(destAddr.sin_addr, addr_str, sizeof(addr_str) - 1);

        int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Socket created");

        int err = bind(sock, (struct sockaddr *)&destAddr, sizeof(destAddr));
        if (err < 0) {
            ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        }
        ESP_LOGI(TAG, "Socket binded");

        while (1) {

            ESP_LOGI(TAG, "Waiting for command");
            struct sockaddr_in6 sourceAddr; // Large enough for both IPv4 or IPv6
            socklen_t socklen = sizeof(sourceAddr);
            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&sourceAddr, &socklen);
            
            // Error occured during receiving
            if (len < 0) {
                ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
                break;
            }
            // Data received
            else {
                vTaskDelay(10);

                // Get the sender's ip address as string
                if (sourceAddr.sin6_family == PF_INET) {
                    inet_ntoa_r(((struct sockaddr_in *)&sourceAddr)->sin_addr.s_addr, addr_str, sizeof(addr_str) - 1);
                } else if (sourceAddr.sin6_family == PF_INET6) {
                    inet6_ntoa_r(sourceAddr.sin6_addr, addr_str, sizeof(addr_str) - 1);
                }
                

                rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string...
                ESP_LOGI(TAG, "Received %d bytes from %s:", len, addr_str);
                ESP_LOGI(TAG, "%s", rx_buffer);

                 if( xQueueSendToBack( xQueue_broadcast_task, ( void * ) &rx_buffer, ( TickType_t ) 10 ) != pdPASS )
                    {
                        /* Failed to post the message, even after 10 ticks. */
                        ESP_LOGI(TAG, "Unable to add command to BROADCAST queue");
                        //added here to clear the que when full! didnt clear the error tho
                        //xQueueReset( xQueue_broadcast_task );
                    }

                int err = sendto(sock, rx_buffer, len, 0, (struct sockaddr *)&sourceAddr, sizeof(sourceAddr));
                if (err < 0) {
                    ESP_LOGE(TAG, "Error occured during sending: errno %d", errno);
                    break;
                }
            }
        }

        if (sock != -1) {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }
   
}
static int socket_add_ipv4_multicast_group(int sock, bool assign_source_if)
{
    struct ip_mreq imreq = { 0 };
    struct in_addr iaddr = { 0 };
    int err = 0;
    // Configure source interface

    imreq.imr_interface.s_addr = IPADDR_ANY;

    // Configure multicast address to listen to
    err = inet_aton(MULTICAST_IPV4_ADDR, &imreq.imr_multiaddr.s_addr);
    if (err != 1) {
        ESP_LOGE(TAG, "Configured IPV4 multicast address '%s' is invalid.", MULTICAST_IPV4_ADDR);
        goto err;
    }
    ESP_LOGI(TAG, "Configured IPV4 Multicast address %s", inet_ntoa(imreq.imr_multiaddr.s_addr));
    if (!IP_MULTICAST(ntohl(imreq.imr_multiaddr.s_addr))) {
        ESP_LOGW(TAG, "Configured IPV4 multicast address '%s' is not a valid multicast address. This will probably not work.", MULTICAST_IPV4_ADDR);
    }

    if (assign_source_if) {
        // Assign the IPv4 multicast source interface, via its IP
        // (only necessary if this socket is IPV4 only)
        err = setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF, &iaddr,
                         sizeof(struct in_addr));
        if (err < 0) {
            ESP_LOGE(TAG, "Failed to set IP_MULTICAST_IF. Error %d", errno);
            goto err;
        }
    }

    err = setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                         &imreq, sizeof(struct ip_mreq));
    if (err < 0) {
        ESP_LOGE(TAG, "Failed to set IP_ADD_MEMBERSHIP. Error %d", errno);
        goto err;
    }

 err:
    return err;
}
static int create_multicast_ipv4_socket()
{
    struct sockaddr_in saddr = { 0 };
    int sock = -1;
    int err = 0;

    sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Failed to create socket. Error %d", errno);
        return -1;
    }

    // Bind the socket to any address
    saddr.sin_family = PF_INET;
    saddr.sin_port = htons(MULTICAST_PORT);
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    err = bind(sock, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in));
    if (err < 0) {
        ESP_LOGE(TAG, "Failed to bind socket. Error %d", errno);
        goto err;
    }

    // Assign multicast TTL (set separately from normal interface TTL)
    uint8_t ttl = MULTICAST_TTL;
    setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(uint8_t));
    if (err < 0) {
        ESP_LOGE(TAG, "Failed to set IP_MULTICAST_TTL. Error %d", errno);
        goto err;
    }

    // this is also a listening socket, so add it to the multicast
    // group for listening...
    err = socket_add_ipv4_multicast_group(sock, true);
    if (err < 0) {
        goto err;
    }

    // All set, socket is configured for sending and receiving
    return sock;

    err:
        close(sock);
        return -1;
}
void multicast_task(void *pvParameters)
{
    while (1) {

        xQueue_multicast_task = xQueueCreate( 25, sizeof(char[COMMAND_ITEM_SIZE]));
        if( xQueue_multicast_task == NULL )
        {
            ESP_LOGI(TAG, "unable to create multicast command queue");
        }
        
        int sock;

        sock = create_multicast_ipv4_socket();
        if (sock < 0) {
            ESP_LOGE(TAG, "Failed to create IPv4 multicast socket");
        }

        if (sock < 0) {
            // Nothing to do!
            vTaskDelay(5 / portTICK_PERIOD_MS);
            continue;
        }

        int err = 1;
        while (err > 0) {
            
            // Loop waiting for UDP received, 
      
            struct timeval tv = {
                .tv_sec = 2,
                .tv_usec = 0,
            };
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(sock, &rfds);

            int s = select(sock + 1, &rfds, NULL, NULL, &tv);
            if (s < 0) {
                ESP_LOGE(TAG, "Select failed: errno %d", errno);
                err = -1;
                break;
            }
            else if (s > 0) {
                if (FD_ISSET(sock, &rfds)) {
                    vTaskDelay(10);
                    // Incoming datagram received
                    char recvbuf[COMMAND_ITEM_SIZE + 1];
                    char raddr_name[32] = { 0 };

                    struct sockaddr_in6 raddr; // Large enough for both IPv4 or IPv6
                    socklen_t socklen = sizeof(raddr);
                    int len = recvfrom(sock, recvbuf, sizeof(recvbuf)-1, 0,
                                       (struct sockaddr *)&raddr, &socklen);
                    if (len < 0) {
                        ESP_LOGE(TAG, "multicast recvfrom failed: errno %d", errno);
                        err = -1;
                        break;
                    }
                                                        
                    recvbuf[len] = 0; // Null-terminate whatever we received and treat like a string...
                    ESP_LOGI(TAG, "Recieve buffer %s", recvbuf);

                    if( xQueueSendToBack( xQueue_multicast_task, ( void * ) &recvbuf, ( TickType_t ) 10 ) != pdPASS )
                    {
                        /* Failed to post the message, even after 10 ticks. */
                        ESP_LOGI(TAG, "Unable to add command to MULTICAST queue");
                    }
                }
            }
            else {
                vTaskDelay(10);
            }
        }
        ESP_LOGE(TAG, "Shutting down socket and restarting...");
        shutdown(sock, 0);
        close(sock);
    }
}
void tcp_task_init(){

        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(PORT);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;
        inet_ntoa_r(dest_addr.sin_addr, addr_str, sizeof(addr_str) - 1);

        listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
        if (listen_sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            return;
        }
        ESP_LOGI(TAG, "Socket created");

        // allows socket address to be reused
        int flag = 1;
        setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

        err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err != 0) {
            ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
           return;
        }
    
}
void tcp_server_run(){

    ESP_LOGI(TAG, "Socket bound, port %d", PORT);

        err = listen(listen_sock, 1);
        if (err != 0) {
            ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
            return;
        }
        ESP_LOGI(TAG, "Socket listening");

        struct sockaddr_in6 source_addr; // Large enough for both IPv4 or IPv6
        uint addr_len = sizeof(source_addr);
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
       
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            return;
        }
        ESP_LOGI(TAG, "Socket accepted");

        while (1) {

            int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
            // Error occurred during receiving
            if (len < 0) {
                ESP_LOGE(TAG, "recv failed: errno %d", errno);
                break;
            }
            //Connection closed
            else if (len == 0) {
                ESP_LOGI(TAG, "Connection closed");
                break;
            }
            // Data received
            else {
                // Get the sender's ip address as string
                if (source_addr.sin6_family == PF_INET) {
                    inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr.s_addr, addr_str, sizeof(addr_str) - 1);
                } else if (source_addr.sin6_family == PF_INET6) {
                    inet6_ntoa_r(source_addr.sin6_addr, addr_str, sizeof(addr_str) - 1);
                }

                rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string
                tcp_task_action_t temp;
                temp.action = 0;

                respond_value[0] = '\0';
                strncpy(temp.action_value,rx_buffer,sizeof(temp.action_value));
                if( xQueueSendToBack( xQueue_tcp_task, ( void * ) &temp, ( TickType_t ) 10 ) != pdPASS )
                {
                        /* Failed to post the message, even after 10 ticks. */
                        ESP_LOGI(TAG, "Unable to add command to queue");
                }
                ESP_LOGI(TAG, "Received %d bytes from %s:", len, addr_str);
                ESP_LOGI(TAG, "%s", rx_buffer);
                
                uint64_t eus = esp_timer_get_time();
                uint64_t interval = 1*1000000;
                while((strlen(respond_value) < 1) && ((eus + interval) > esp_timer_get_time())){
                    if (xQueueReceive(xQueue_tcp_respond, &respond_value, 0)){
                        ESP_LOGI(TAG, "[ TCP QUEUE RECIEVE ] %s" , respond_value);
                    }
                     vTaskDelay(10);
                }
                if (strlen(respond_value) < 1) strncpy(respond_value, "timeout", sizeof(respond_value));
                int err = send(sock, respond_value, strlen(respond_value), 0);
                
                if (err < 0) {
                    ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                    break;
                }
                break;
            }



        }
        if (sock != -1) {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }

}
void tcp_task(void *pvParameters)
{
    
    
    xQueue_tcp_task = xQueueCreate( 5, sizeof(tcp_task_action_t));
    if( xQueue_tcp_task == NULL )
    {
        ESP_LOGI(TAG, "unable to create tcp command queue");
    }


    // que for response to send back to the client
    xQueue_tcp_respond = xQueueCreate( 5, sizeof(tcp_task_action_t));
    if( xQueue_tcp_respond == NULL )
    {
        ESP_LOGI(TAG, "unable to create tcp response queue");
    }
    
    
    tcp_task_init();

    while (1) {

      

        tcp_server_run();

    }
    vTaskDelete(NULL);

}
void command_ota(void){
    saveParamters();
    //xTaskCreate(&ota_task, "ota_task", 16384, NULL, 3, NULL);
    xTaskCreate(&simple_ota_example_task, "ota_example_task", 4096, NULL, 5, NULL);
}
static esp_err_t command_reset(){
    saveParamters();
    esp_restart();
    return ESP_OK;
}
//MESSAGING
void server_ping(char* command){
    char mac_ip_data[256];
    char mac_string[256];
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    sprintf(mac_string,"%02x:%02x:%02x:%02x:%02x:%02x",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    tcpip_adapter_ip_info_t ip_info;
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info);
    char ip_string[30];
    strcpy(ip_string, inet_ntoa(ip_info.ip.addr));
    char cmd[10];
    strcpy(cmd, command);
    sprintf(mac_ip_data,"%s %s %s",cmd,mac_string,ip_string);

    send_udp(mac_ip_data,SERVER_IP_ADDRESS,SERVER_PORT);
}
void sendMessage(char* command, char* message){
    char mac_ip_data[256];
    char mac_string[256];
    //MAC
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    sprintf(mac_string,"%02x:%02x:%02x:%02x:%02x:%02x",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    //IP
    tcpip_adapter_ip_info_t ip_info;
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info);
    char ip_string[30];
    strcpy(ip_string, inet_ntoa(ip_info.ip.addr));
    //COMMAND
    char cmd[10];
    strcpy(cmd, command);
    //MESSAGE
    char msg[sizeof(message)];
    strcpy(msg, message);
    //ASSEMBEL & SEND
    sprintf(mac_ip_data,"%s %s %s %s",cmd,mac_string,ip_string, msg);
    send_udp(mac_ip_data,SERVER_IP_ADDRESS,SERVER_PORT);
}
void updateUdp(){
    char cmd[10];
    sprintf(cmd,"update");
    char respond[30];
    sprintf(respond, "%d %d %d %d %d", device_stepper.current, device_stepper.min, device_stepper.max,device_stepper.target,device_stepper.number);
    sendMessage(cmd, respond);
}
int deviceDistanceSpeed(int x, int y, int z, int s){

  int64_t x_Square = device_location.x - x;
  int64_t y_Square = device_location.y - y;
  int64_t z_Square = device_location.z - z;

  int64_t distance = (int64_t)abs(sqrtf(pow(x_Square, 2)  + pow(y_Square, 2) + pow(z_Square, 2)));

  float speed = (float)(s / 100.0);

  ESP_LOGI(TAG, "Speed %f", speed);

  float distanceFromStart = (float)( distance  / speed);

  ESP_LOGI(TAG, "Distance From Start Float %f", distanceFromStart);

  // int64_t distanceFromStart = (int64_t)( distance  / (s / 10));
  // ESP_LOGI(TAG, "Distance From Start %llu", distanceFromStart);

  return distanceFromStart;
}
void wave_task(void *args) {

   xQueue_wave_task = xQueueCreate(10, sizeof(wave_t));
   if (xQueue_wave_task == NULL) ESP_LOGE(TAG, "Unable to create wave command queue");
   
    while(1) {
         if (xQueueReceive(xQueue_wave_task, &wave, portMAX_DELAY)) {
            ESP_LOGI(TAG, "X : %d Y : %d Z : %d SPEED : %d", wave.x, wave.y, wave.z ,wave.speed);
            float delay = deviceDistanceSpeed(wave.x, wave.y, wave.z ,wave.speed);
            ESP_LOGI(TAG, "DELAY : %f", delay);
            vTaskDelay(pdMS_TO_TICKS(delay));
            ESP_LOGI(TAG, "TYPE : %d MOVE : %d SPEED : %d ACCEL : %d MIN : %d MAX : %d", wave.wave_stepper.type, wave.wave_stepper.move, wave.wave_stepper.speed, wave.wave_stepper.accel,wave.wave_stepper.min, wave.wave_stepper.max);
            command_move(wave.wave_stepper.type, wave.wave_stepper.move, wave.wave_stepper.speed, wave.wave_stepper.accel, 0, wave.wave_stepper.min, wave.wave_stepper.max);
            vTaskDelay(pdMS_TO_TICKS(10));

            // OVERWRITE
            // This command sends a task notification with value '3' to the stepper task. Use that block of code to overwrite the stepper loop
            //xTaskNotify(stepper_task_handle, 3, eSetValueWithOverwrite);
         }
    }
    vTaskDelete(NULL); // clean up after ourselves
}
void wave_command(int x, int y, int z, int speed, int type, int move, int stepper_speed, int accel, int min, int max){
    wave_t wave_action;
    wave_action.x = x;
    wave_action.y = y;
    wave_action.z = z;
    wave_action.speed = speed;

    stepper_command_t stepper_action;
    stepper_action.move = move;
    stepper_action.type = type;
    stepper_action.speed = stepper_speed;
    stepper_action.accel = accel;
    stepper_action.min = min;
    stepper_action.max = max;

    wave_action.wave_stepper = stepper_action;

    xQueueSendToBack(xQueue_wave_task, (void *) &wave_action, 0);            

}
void sine_task(void *args) {

   xQueue_sine_task = xQueueCreate(10, sizeof(sine_t));
   if (xQueue_sine_task == NULL) ESP_LOGE(TAG, "Unable to create save command queue");
   
    while(1) {
         if (xQueueReceive(xQueue_sine_task, &sine, portMAX_DELAY)) {
            int top = sine.sine_stepper.move + sine.offset;
            int bottom = sine.sine_stepper.move - sine.offset;
            ESP_LOGI(TAG, "TYPE : %d MOVE : %d SPEED : %d ACCEL : %d MIN : %d MAX : %d", sine.sine_stepper.type, sine.sine_stepper.move, sine.sine_stepper.speed, sine.sine_stepper.accel,sine.sine_stepper.min, sine.sine_stepper.max);
            
            // //from bellow 
            // if (sine.sine_stepper.move <= device_stepper.current){
            //     for (int i = 0; i<sine.loops; i++) {
            //         if (i%2 ==0){
            //             command_move(sine.sine_stepper.type, bottom, sine.sine_stepper.speed, sine.sine_stepper.accel,sine.sine_stepper.min, sine.sine_stepper.max);
            //         } else {
            //             command_move(sine.sine_stepper.type, top, sine.sine_stepper.speed, sine.sine_stepper.accel,sine.sine_stepper.min, sine.sine_stepper.max);
            //         }
            //         vTaskDelay(pdMS_TO_TICKS(10));
            //     }
            // }
            // //from above
            // if (sine.sine_stepper.move > device_stepper.current){
                
            // }

            for (int i = 0; i<sine.loops; i++) {
                    if (i%2 ==0){
                        command_move(sine.sine_stepper.type, top, sine.sine_stepper.speed, sine.sine_stepper.accel, 0, sine.sine_stepper.min, sine.sine_stepper.max);
                    } else { 
                        command_move(sine.sine_stepper.type, bottom, sine.sine_stepper.speed, sine.sine_stepper.accel, 0, sine.sine_stepper.min, sine.sine_stepper.max);
                    }
                    vTaskDelay(pdMS_TO_TICKS(10));
            }
        
            command_move(sine.sine_stepper.type, sine.sine_stepper.move, sine.sine_stepper.speed, sine.sine_stepper.accel, 0, sine.sine_stepper.min, sine.sine_stepper.max);
            vTaskDelay(pdMS_TO_TICKS(10));
            // OVERWRITE
            // This command sends a task notification with value '3' to the stepper task. Use that block of code to overwrite the stepper loop
            //xTaskNotify(stepper_task_handle, 3, eSetValueWithOverwrite);
         }
    }
    vTaskDelete(NULL); // clean up after ourselves
}
void sine_command(int type, int move, int stepper_speed, int accel, int min, int max, int loops, int offset){
    sine_t sine_action;
    sine_action.loops = loops;
    sine_action.offset = offset;

    stepper_command_t stepper_action;
    stepper_action.move = move;
    stepper_action.type = type;
    stepper_action.speed = stepper_speed;
    stepper_action.accel = accel;
    stepper_action.min = min;
    stepper_action.max = max;

    sine_action.sine_stepper = stepper_action;

    xQueueSendToBack(xQueue_sine_task, (void *) &sine_action, 0);            
    ESP_LOGI(TAG, "Sine Task");
}
void sine_wave_task(void *args) {

   xQueue_sine_wave_task = xQueueCreate(10, sizeof(sine_wave_t));
   if (xQueue_sine_wave_task == NULL) ESP_LOGE(TAG, "Unable to create save command queue");
   
    while(1) {
         if (xQueueReceive(xQueue_sine_wave_task, &sine_wave, portMAX_DELAY)) {
            int top = sine_wave.sine_wave_stepper.move + sine_wave.offset;
            int bottom = sine_wave.sine_wave_stepper.move - sine_wave.offset;
            ESP_LOGI(TAG, "TYPE : %d MOVE : %d SPEED : %d ACCEL : %d MIN : %d MAX : %d", sine_wave.sine_wave_stepper.type, sine_wave.sine_wave_stepper.move, sine_wave.sine_wave_stepper.speed, sine_wave.sine_wave_stepper.accel,sine_wave.sine_wave_stepper.min, sine_wave.sine_wave_stepper.max);

            ESP_LOGI(TAG, "X : %d Y : %d Z : %d SPEED : %d", sine_wave.x, sine_wave.y, sine_wave.z ,sine_wave.speed);
            float delay = deviceDistanceSpeed(sine_wave.x, sine_wave.y, sine_wave.z ,sine_wave.speed);
            ESP_LOGI(TAG, "DELAY : %f", delay);
            vTaskDelay(pdMS_TO_TICKS(delay));
            ESP_LOGI(TAG, "TYPE : %d MOVE : %d SPEED : %d ACCEL : %d MIN : %d MAX : %d", sine_wave.sine_wave_stepper.type, sine_wave.sine_wave_stepper.move, sine_wave.sine_wave_stepper.speed, sine_wave.sine_wave_stepper.accel,sine_wave.sine_wave_stepper.min, sine_wave.sine_wave_stepper.max);

            for (int i = 0; i<sine_wave.loops; i++) {
                    if (i%2 ==0){
                        command_move(sine_wave.sine_wave_stepper.type, top, sine_wave.sine_wave_stepper.speed, sine_wave.sine_wave_stepper.accel, 0,sine_wave.sine_wave_stepper.min, sine_wave.sine_wave_stepper.max);
                    } else {
                        command_move(sine_wave.sine_wave_stepper.type, bottom, sine_wave.sine_wave_stepper.speed, sine_wave.sine_wave_stepper.accel,0 ,sine_wave.sine_wave_stepper.min, sine_wave.sine_wave_stepper.max);
                    }
                    vTaskDelay(pdMS_TO_TICKS(10));
            }
        
            command_move(sine_wave.sine_wave_stepper.type, sine_wave.sine_wave_stepper.move, sine_wave.sine_wave_stepper.speed, sine_wave.sine_wave_stepper.accel, 0, sine_wave.sine_wave_stepper.min, sine_wave.sine_wave_stepper.max);
            vTaskDelay(pdMS_TO_TICKS(10));
       }
    }
    vTaskDelete(NULL); // clean up after ourselves
}
void sine_wave_command(int x, int y, int z, int speed, int type, int move, int stepper_speed, int accel, int min, int max, int loops, int offset){
    sine_wave_t sine_wave_action;
    sine_wave_action.loops = loops;
    sine_wave_action.offset = offset;
    sine_wave_action.x = x;
    sine_wave_action.y = y;
    sine_wave_action.z = z;
    sine_wave_action.speed = speed;

    stepper_command_t stepper_action;
    stepper_action.move = move;
    stepper_action.type = type;
    stepper_action.speed = stepper_speed;
    stepper_action.accel = accel;
    stepper_action.min = min;
    stepper_action.max = max;

    sine_wave_action.sine_wave_stepper = stepper_action;

    xQueueSendToBack(xQueue_sine_wave_task, (void *) &sine_wave_action, 0);            
    ESP_LOGI(TAG, "sine_wave Task");
}
//MESSAGE QUE
void command_handler(char * queue_value, int type){

        int command_count =  get_command_line(queue_value, type);  // command line / checks if its a a duplicate
        // ----- Standard Functions ----- //
        if (strcmp(command_line[0], "duplicate") == 0){
            //heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);
            ESP_LOGI(TAG, "[DUPLICATE]");
            return;
        } 

        if (strcmp(command_line[0], "ota") == 0){
            command_ota();
        }
        if (strcmp(command_line[0], "reset") == 0){
            command_reset();
        }
         if (strcmp(command_line[0], "stop") == 0){
            //xTaskNotify(stepper_task_handle, 3, eSetValueWithoutOverwrite);
            // xTaskNotify(stepper_task_handle, 3, eSetValueWithOverwrite);
            ESP_LOGW(TAG,"Stop command");
            xTaskNotify(stepper_task_handle, STOP_BIT, eSetBits); 
        }
        //MOVEMENT TYPES/BEHAVIOURS
        if (strcmp(command_line[0], "home") == 0){//Relative Move
            command_move(0, atoi(command_line[1]), atoi(command_line[2]), atoi(command_line[3]), 0, device_stepper.min, device_stepper.max);
        }
        if (strcmp(command_line[0], "stepperMove") == 0){//Relative Move
            command_move(0, atoi(command_line[1]), atoi(command_line[2]), atoi(command_line[3]), 0,device_stepper.min, device_stepper.max);
        }
        if (strcmp(command_line[0], "stepperTranslate") == 0){//Move to location
            command_move(1, atoi(command_line[1]), atoi(command_line[2]), atoi(command_line[3]), 0, device_stepper.min, device_stepper.max);
        }
        if (strcmp(command_line[0], "stepperNumTranslate") == 0){
            if (device_stepper.number!=0){
                int selectedCommand = device_stepper.number + 2;
                command_move(1, atoi(command_line[selectedCommand]), atoi(command_line[1]), atoi(command_line[2]), 0, device_stepper.min, device_stepper.max);
                ESP_LOGI(TAG,"STEPPER NUMBER MOVE %d : %d", selectedCommand, atoi(command_line[selectedCommand]));
            }
        }

        if (strcmp(command_line[0], "strandTranslate") == 0){
            // if (device_stepper.number!=0){
            //     int selectedCommand = device_stepper.number + 2;
            //     // int selectedStrand = 0; 
            //     // if (selectedCommand % 2 == 0){
            //     //     selectedStrand
            //     // }
            //     command_move(1, atoi(command_line[selectedCommand]), atoi(command_line[1]), atoi(command_line[2]), device_stepper.min, device_stepper.max);
            //     ESP_LOGI(TAG,"STEPPER NUMBER MOVE %d : %d", selectedCommand, atoi(command_line[selectedCommand]));
            // }

            //float speedA =  (atoi(command_line[1]) - currentPosition) / atoi(command_line[4]);

            command_move(2, atoi(command_line[1]), atoi(command_line[2]), atoi(command_line[3]), atoi(command_line[4]), device_stepper.min, device_stepper.max);
        }
        if (strcmp(command_line[0], "stepperWave") == 0){//Move to location
            wave_command(atoi(command_line[1]), atoi(command_line[2]), atoi(command_line[3]), atoi(command_line[4]), atoi(command_line[5]), atoi(command_line[6]), atoi(command_line[7]), atoi(command_line[8]),device_stepper.min, device_stepper.max);       
        }
        if (strcmp(command_line[0], "stepperSine") == 0){//Move to location
            sine_command(1, atoi(command_line[1]), atoi(command_line[2]), atoi(command_line[3]),device_stepper.min, device_stepper.max, atoi(command_line[4]), atoi(command_line[5]));       
        }
        if (strcmp(command_line[0], "stepperNumSine") == 0){
            int selectedCommand = device_stepper.number + 6;
            sine_command(1, atoi(command_line[selectedCommand]), atoi(command_line[1]), atoi(command_line[2]),device_stepper.min, device_stepper.max, atoi(command_line[3]), atoi(command_line[4]));       
            ESP_LOGI(TAG,"STEPPER NUMBER MOVE %d : %d", selectedCommand, atoi(command_line[selectedCommand]));
        }

        if (strcmp(command_line[0], "stepperSineWave") == 0){
            //0 0 0 1 1 2000 2000 2000 5 100
            sine_wave_command(atoi(command_line[1]), atoi(command_line[2]), atoi(command_line[3]), atoi(command_line[4]), 1, atoi(command_line[5]), atoi(command_line[6]), atoi(command_line[7]),device_stepper.min, device_stepper.max, atoi(command_line[8]), atoi(command_line[9]));       

       }

        
        //SETTING PARAMTERS UDP
        if (strcmp(command_line[0], "setCurrent") == 0){
            ESP_LOGI(TAG, "SET CURRENT");
            setPramamter(1, atoi(command_line[1]));
            saveParamters();
            updateUdp();

        }
        if (strcmp(command_line[0], "setMin") == 0){
            ESP_LOGI(TAG, "SET MIN");
            setPramamter(2, atoi(command_line[1]));
            saveParamters();
            updateUdp();

        }
        if (strcmp(command_line[0], "setMax") == 0){
            ESP_LOGI(TAG, "SET MAX");
            setPramamter(3, atoi(command_line[1]));
            saveParamters();
            updateUdp();
            return;
        }
        if (strcmp(command_line[0], "setNumber") == 0){
            ESP_LOGI(TAG, "SET NUMBER");
            setPramamter(5, atoi(command_line[1]));
            saveParamters();
            updateUdp();
            return;
        }
        if (strcmp(command_line[0], "set_location") == 0){
             location_t loc;
            loc.x = atoi(command_line[1]);
            loc.y = atoi(command_line[2]);
            loc.z = atoi(command_line[3]);

            command_set_location(loc);
            //ESP_LOGI(TAG, "LOCATION x:%d y:%d: z:%d", atoi(command_line[1]), atoi(command_line[2]), atoi(command_line[3]));

            char respond[3];
            strncpy(respond, "OK" ,3);
            if( xQueueSendToBack( xQueue_tcp_respond, ( void * ) &respond, ( TickType_t ) 10 ) != pdPASS )
                {
                    //* Failed to post the message, even after 10 ticks. */
                    ESP_LOGW(TAG, "Unable to add command to TCP queue");
                    return;
                }
            return;
        }
        if (strcmp(command_line[0], "update") == 0){
            updateUdp();
            return;
        }

        if (type){ // tcp only command
            if (strncmp(command_line[0], "get_mac" ,strlen("get_mac")) == 0){
                char respond[20];
                uint8_t baseMac[6];
                esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
                sprintf(respond, "%02X:%02X:%02X:%02X:%02X:%02X", baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
                if( xQueueSendToBack( xQueue_tcp_respond, ( void * ) &respond, ( TickType_t ) 10 ) != pdPASS )
                {
                    //* Failed to post the message, even after 10 ticks. */
                    ESP_LOGW(TAG, "Unable to add command to TCP queue");
                    return;
                }
            }
            //SETTING PARAMTERS UDP
            if (strncmp(command_line[0], "set_location" ,strlen("set_location")) == 0){
                location_t loc;
                loc.x = atoi(command_line[1]);
                loc.y = atoi(command_line[2]);
                loc.z = atoi(command_line[3]);

                command_set_location(loc);

                //ESP_LOGI(TAG, "LOCATION x:%d y:%d: z:%d", atoi(command_line[1]), atoi(command_line[2]), atoi(command_line[3]));

                char respond[3];
                strncpy(respond, "OK" ,3);
                if( xQueueSendToBack( xQueue_tcp_respond, ( void * ) &respond, ( TickType_t ) 10 ) != pdPASS )
                {
                    //* Failed to post the message, even after 10 ticks. */
                    ESP_LOGW(TAG, "Unable to add command to TCP queue");
                    return;
                }
  
            }
            if (strncmp(command_line[0], "setMin" ,strlen("setMin")) == 0){
                ESP_LOGI(TAG, "SET MIN");
                setPramamter(2, atoi(command_line[1]));
                saveParamters();
                //ESP_LOGI(TAG, "LOCATION x:%d y:%d: z:%d", atoi(command_line[1]), atoi(command_line[2]), atoi(command_line[3]));
                char respond[3];
                strncpy(respond, "OK" ,3);
                if( xQueueSendToBack( xQueue_tcp_respond, ( void * ) &respond, ( TickType_t ) 10 ) != pdPASS )
                {
                    //* Failed to post the message, even after 10 ticks. */
                    ESP_LOGW(TAG, "Unable to add command to TCP queue");
                    return;
                }

            }   
            if (strncmp(command_line[0], "setMax" ,strlen("setMax")) == 0){
                ESP_LOGI(TAG, "SET MAX");
                setPramamter(3, atoi(command_line[1]));
                saveParamters();
                //ESP_LOGI(TAG, "LOCATION x:%d y:%d: z:%d", atoi(command_line[1]), atoi(command_line[2]), atoi(command_line[3]));
                char respond[3];
                strncpy(respond, "OK" ,3);
                if( xQueueSendToBack( xQueue_tcp_respond, ( void * ) &respond, ( TickType_t ) 10 ) != pdPASS )
                {
                    //* Failed to post the message, even after 10 ticks. */
                    ESP_LOGW(TAG, "Unable to add command to TCP queue");
                    return;
                }

            }   
            if (strncmp(command_line[0], "setNumber" ,strlen("setNumber")) == 0){
                ESP_LOGI(TAG, "SET NUMBER");
                setPramamter(5, atoi(command_line[1]));
                saveParamters();
                //ESP_LOGI(TAG, "LOCATION x:%d y:%d: z:%d", atoi(command_line[1]), atoi(command_line[2]), atoi(command_line[3]));
                char respond[3];
                strncpy(respond, "OK" ,3);
                if( xQueueSendToBack( xQueue_tcp_respond, ( void * ) &respond, ( TickType_t ) 10 ) != pdPASS )
                {
                    //* Failed to post the message, even after 10 ticks. */
                    ESP_LOGW(TAG, "Unable to add command to TCP queue");
                    return;
                }

            }  
            if (strncmp(command_line[0], "update" ,strlen("get_location")) == 0){

                char respond[40];
                sprintf(respond, "location x: 1 y: 2 z: 3");
                if( xQueueSendToBack( xQueue_tcp_respond, ( void * ) &respond, ( TickType_t ) 10 ) != pdPASS )
                {
                    //* Failed to post the message, even after 10 ticks. */
                    ESP_LOGW(TAG, "Unable to add command to TCP queue");
                    return;
                }
            }
            if (strncmp(command_line[0], "get_location" ,strlen("get_location")) == 0){

                char respond[40];
                //sprintf(respond, "{\"location\":{\"x\":%d,\"y\":%d,\"z\":%d}}", device_location.x, device_location.y, device_location.z);
                sprintf(respond, "location x: 1 y: 2 z: 3");
                if( xQueueSendToBack( xQueue_tcp_respond, ( void * ) &respond, ( TickType_t ) 10 ) != pdPASS )
                {
                    //* Failed to post the message, even after 10 ticks. */
                    ESP_LOGW(TAG, "Unable to add command to TCP queue");
                    return;
                }
            }
        }
     
}
#ifdef __cplusplus
  }
#endif