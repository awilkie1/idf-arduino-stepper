#include "ArduinoOSC.h" // include this first otherwise the linker gets hella confused with multiple definitions okay
#include "net_osc.hpp"
#include "strand.hpp"
#include "parameters.h"

static const char *TAG = "NET_OSC";

// #ifdef __cplusplus
//     extern "C" {
// #endif
OscDecoder pr;

void init_osc() {
    OscDecoder osc_reader;
    
    const OscMessage * msg;   
}

// void test_route(OscMessage * msg_in) {
//     ESP_LOGW(TAG,"Routing");
//     ESP_LOGI(TAG, "Address: %s, Value1: %s", msg_in->address().c_str(), msg_in->getArgAsString(0).c_str());
// }

void osc_handler(BCAST_CMD cmd, uint8_t type) {
    pr.init(cmd.data, cmd.len); // load data into packet reader
    // ESP_LOGI(TAG, "Command: %s, Length: %i", cmd.data, cmd.len);
    OscMessage* msg = pr.decode(); // decode message
    ESP_LOGW(TAG, "%s", msg->address().c_str());

    // ESP_LOGI(TAG, "Address: %s, Value1: %s", msg->address().c_str(), msg->getArgAsString(0).c_str());

    // STOP 
    if ( ArduinoOSC::match("/stop", msg->address()) ) {
        xTaskNotify(stepper_task_handle, STOP_BIT, eSetBits); 
    }

    if ( ArduinoOSC::match("/slack", msg->address()) ) {
        // command_move(REL,0,0,0,device_stepper.min,device_stepper.min);
        go_slack();
        // xTaskNotify(stepper_task_handle, SLACK_BIT, eSetBits); 
        
    }

    if ( ArduinoOSC::match("/reset", msg->address()) ) {
        command_reset();
    }

    if ( ArduinoOSC::match("/ota", msg->address()) ) {
        command_ota();
    }

    // TODO... USE a pre-saved set of speeds to that this doesn't fuck up big time
    if ( ArduinoOSC::match("/home", msg->address()) ) {
        int move = msg->getArgAsInt32(0);
        int speed = msg->getArgAsInt32(1);
        int accel = msg->getArgAsInt32(2);
        int min = device_stepper.min;
        int max = device_stepper.max;

        command_move(REL, move, speed, accel, min, max);
    }

    // Relative movement
    if ( ArduinoOSC::match("/stepperMove", msg->address()) ) {
        int move = msg->getArgAsInt32(0);
        int speed = msg->getArgAsInt32(1);
        int accel = msg->getArgAsInt32(2);
        int min = device_stepper.min;
        int max = device_stepper.max;

        command_move(REL, move, speed, accel, min, max);
    }

    // Absolute movement (set position)
    if ( ArduinoOSC::match("/stepperTranslate", msg->address()) ) {
        int move = msg->getArgAsInt32(0);
        int speed = msg->getArgAsInt32(1);
        int accel = msg->getArgAsInt32(2);
        int min = device_stepper.min;
        int max = device_stepper.max;

        command_move(ABS, move, speed, accel, min, max);
    }

    if ( ArduinoOSC::match("/stepperNumTranlate", msg->address()) ) {
        int move = msg->getArgAsInt32(0);
        int speed = msg->getArgAsInt32(1);
        int accel = msg->getArgAsInt32(2);
        int min = device_stepper.min;
        int max = device_stepper.max;

        command_move(ABS, move, speed, accel, min, max);
    }

    if ( ArduinoOSC::match("/setMin", msg->address()) ) {
        int move = msg->getArgAsInt32(0);
        int speed = msg->getArgAsInt32(1);
        int accel = msg->getArgAsInt32(2);
        int min = device_stepper.min;
        int max = device_stepper.max;

        command_move(ABS, move, speed, accel, min, max);
    }






    // if ( ArduinoOSC::match() ) {

    // }

    // ArduinoOSC::route("/stepperMove", (OscMessage *) msg, test_route);
    

    // Osc
    // for (int i=0; i<)
    
}



// #ifdef __cplusplus
//   }
// #endif