#include "ArduinoOSC.h" // include this first otherwise the linker gets hella confused with multiple definitions okay
#include "net_osc.hpp"

static const char *TAG = "NET_OSC";

// #ifdef __cplusplus
//     extern "C" {
// #endif
OscDecoder pr;

void init_osc() {
    OscDecoder osc_reader;
    
    const OscMessage * msg;   
}

void osc_handler(BCAST_CMD cmd, uint8_t type) {
    pr.init(cmd.data, cmd.len); // load data into packet reader
    OscMessage* msg = pr.decode(); // decode message

    ESP_LOGI(TAG, "Address: %s, Value1: %s", msg->address().c_str(), msg->getArgAsString(0).c_str());
    

    // Osc
    // for (int i=0; i<)
    
}

// #ifdef __cplusplus
//   }
// #endif