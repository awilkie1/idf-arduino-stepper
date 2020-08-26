

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_spi_flash.h"

#include <Arduino.h>

// #include "Stepper.h"
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


#ifdef __cplusplus
  extern "C" {
#endif


uint64_t multicast_id = 1;
char multicast_queue_value[COMMAND_ITEM_SIZE];
char broadcast_queue_value[COMMAND_ITEM_SIZE];

char command_line[COMMAND_ITEMS][50];

//NVS

uint32_t my_handle;

esp_err_t nvs_err;

//OTA
static const char *TAG = "simple_ota_example";
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

typedef struct location {
    int32_t x;
    int32_t y;
    int32_t z;
} location_t;

location_t device_location;

void nvs_init(){
    // Initialize NVS.
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // 1.OTA app partition table has a smaller NVS partition size than the non-OTA
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
location_t command_init_location(){

  location_t location;

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

//WIFI
static esp_err_t event_handler(void *ctx, system_event_t *event)

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
static void initialise_wifi(void)
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
static void wait_for_ip()
{
    uint32_t bits = CONNECTED_BIT;
    ESP_LOGI(TAG, "Waiting for AP connection...");
    xEventGroupWaitBits(wifi_event_group, bits, false, true, portMAX_DELAY);
    ESP_LOGI(TAG, "Connected to AP");
}
void init_wifi(void)
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
    
    ESP_LOGI(TAG, "SENT MESSAGE %s : %s: %d", udp_message, ip_address, port);
    

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
// add socket to listten on
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

        xQueue_multicast_task = xQueueCreate( 5, sizeof(char[COMMAND_ITEM_SIZE]));
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
    //xTaskCreate(&ota_task, "ota_task", 16384, NULL, 3, NULL);
    xTaskCreate(&simple_ota_example_task, "ota_example_task", 8192, NULL, 5, NULL);
}
static esp_err_t command_reset(){
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
    sprintf(mac_ip_data,"%s %s - %s",cmd,mac_string,ip_string);
    send_udp(mac_ip_data,SERVER_IP_ADDRESS,SERVER_PORT);
}

//MESSAGE QUE
void command_handler(char * queue_value, int type){

        int command_count =  get_command_line(queue_value, type);  // command line parser

        // ----- Standard Functions ----- //
        if (strncmp(command_line[0], "duplicate" ,9) == 0){
                //heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);
                ESP_LOGI(TAG, "[DUPLICATE]");
                return;
        }
        if (strncmp(command_line[0], "ota" ,3) == 0){
            command_ota();
            return;
        }
        if (strncmp(command_line[0], "reset" ,5) == 0){
            command_reset();
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