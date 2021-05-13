#include "ArduinoOSC.h" // include this first otherwise the linker gets hella confused with multiple definitions okay
#include "net_osc.hpp"
#include "strand.hpp"
#include "parameters.h"
#include "net.h"
#include "main.h"

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
        return;
    }

    if ( ArduinoOSC::match("/slack", msg->address()) ) {
        // command_move(REL,0,0,0,device_stepper.min,device_stepper.min);
        go_slack();
        return;
        // xTaskNotify(stepper_task_handle, SLACK_BIT, eSetBits); 
        
    }

    if ( ArduinoOSC::match("/reset", msg->address()) ) {
        command_reset();
        return;
    }

    if ( ArduinoOSC::match("/ota", msg->address()) ) {
        command_ota();
        return;
    }

    if ( ArduinoOSC::match("/stepperWave", msg->address()) ) {
        uint32_t x = msg->getArgAsInt32(0);
        uint32_t y = msg->getArgAsInt32(1);
        uint32_t z = msg->getArgAsInt32(2);
        uint32_t speed = msg->getArgAsInt32(3);
        uint32_t type = msg->getArgAsInt32(4);
        uint32_t move = msg->getArgAsInt32(5);
        uint32_t stepper_speed = msg->getArgAsInt32(6);
        uint32_t accel = msg->getArgAsInt32(7);
        uint32_t min = device_stepper.min;
        uint32_t max = device_stepper.max;

        wave_command(x, y, z, speed, type, move, stepper_speed, accel, min, max);
        
        return;
    }

    if ( ArduinoOSC::match("/stall", msg->address()) ) {
        stepper_cfg_t stepper_cfg;
        stepper_cfg.stall = msg->getArgAsInt32(0);
        stepper_cfg.tcool = msg->getArgAsInt32(1);
        stepper_cfg.tpwm = msg->getArgAsInt32(2);

        command_set_stall(stepper_cfg);
        return;
    } 

    // TODO... USE a pre-saved set of speeds to that this doesn't fuck up big time
    if ( ArduinoOSC::match("/home", msg->address()) ) {
        // int move = msg->getArgAsInt32(0);
        // int speed = msg->getArgAsInt32(1);
        // int accel = msg->getArgAsInt32(2);
        int move = HOME_DISTANCE;
        int speed = HOME_SPEED;
        int accel = HOME_ACCEL;
        int min = device_stepper.min;
        int max = device_stepper.max;

        command_move(REL, move, speed, accel, min, max);
        return;
    }

    // Relative movement
    if ( ArduinoOSC::match("/stepperMove", msg->address()) ) {
        int move = msg->getArgAsInt32(0);
        int speed = msg->getArgAsInt32(1);
        int accel = msg->getArgAsInt32(2);
        int min = device_stepper.min;
        int max = device_stepper.max;

        command_move(REL, move, speed, accel, min, max);
        return;
    }

    // Absolute movement (set position)
    if ( ArduinoOSC::match("/stepperTranslate", msg->address()) ) {
        int move = msg->getArgAsInt32(0);
        int speed = msg->getArgAsInt32(1);
        int accel = msg->getArgAsInt32(2);
        int min = device_stepper.min;
        int max = device_stepper.max;

        command_move(ABS, move, speed, accel, min, max);
        return;
    }

    if ( ArduinoOSC::match("/stepperNumTranlate", msg->address()) ) {
        int move = msg->getArgAsInt32(0);
        int speed = msg->getArgAsInt32(1);
        int accel = msg->getArgAsInt32(2);
        int min = device_stepper.min;
        int max = device_stepper.max;

        command_move(ABS, move, speed, accel, min, max);
        return;
    }

    if ( ArduinoOSC::match("/setMin", msg->address()) ) {
        uint32_t min = msg->getArgAsInt32(0);
        setParameter(2, min);
        saveParameter();
        updateUdp();
        return;
    }

    if ( ArduinoOSC::match("/setMax", msg->address()) ) {
        uint32_t max = msg->getArgAsInt32(0);
        setParameter(3, max);
        saveParameter();
        updateUdp();
        return;
    }

    if ( ArduinoOSC::match("/setNumber", msg->address()) ) {
        uint32_t number = msg->getArgAsInt32(0);
        setParameter(5, number);
        saveParameter();
        updateUdp();
        return;
    }

    if ( ArduinoOSC::match("/setLocation", msg->address()) ) {
        location_t loc;
        loc.x = msg->getArgAsInt32(0);
        loc.y = msg->getArgAsInt32(1);
        loc.z = msg->getArgAsInt32(2);
        updateUdp();
        return;
    }


    if ( ArduinoOSC::match("/update", msg->address()) ) {
        updateUdp();
        return;
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