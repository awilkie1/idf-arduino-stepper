

#include "Strand.hpp"
#include <HardwareSerial.h>
#include <TMCStepper.h>
// #include "AccelStepper.h"
#include <AccelStepper.h>
#include "net.h"
#include "parameters.h"
#include <driver/adc.h>

static const char *TAG = "STEPPER";

QueueHandle_t xQueue_stepper_command; // Must redefine here

TaskHandle_t sensor_task_handle = NULL;

stepper_command_t stepper_commands;

HardwareSerial SerialPort(2);
// #define STALL_VALUE     100 // [0..255]

const int uart_buffer_size = (1024 * 2);
#define RXD2             16  //UART
#define TXD2             17  //UART
#define EN_PIN           5   // Enable
#define DIR_PIN          18  // Direction
#define STEP_PIN         19  // Step
// #define DIR_PIN          14  // Direction
// #define STEP_PIN         12  // Step
// #define DIR_PIN          19 // Direction (Oliver)
// #define STEP_PIN         14 // Step  (Oliver)
#define R_SENSE 0.11f
// #define R_SENSE 0.27f
// #define R_SENSE 0.08f
#define DRIVER_ADDRESS  0b00       // TMC2209 Driver address according to MS1 and MS2
#define MICROSTEPPING         00000// MICROSTEPPING 8
// #define MICROSTEPPING         2// MICROSTEPPING 8
//homiing buttion stuff
// #define HOME_PIN         32 // HOME (Oliver)

// StallGuard
#define STALL_VALUE     70 // 140  70 (85)
#define HOME_PIN         21 // HOME

// CoolStep 
// TODO: keep reducing tcool value
#define TCOOL_VALUE    100 // 150 > TPWMTHRS_THR  42 (higher value == lower speed) 130 80 (120) 60 good

// StealthChop
#define TPWMTHRS_THR    42 // 140 Threshold where stealthchop switches to spreadcycle

//TMC2208Stepper driver(&SerialPort, R_SENSE); 
TMC2209Stepper driver(&SerialPort, R_SENSE , DRIVER_ADDRESS);

AccelStepper stepper = AccelStepper(stepper.DRIVER, STEP_PIN, DIR_PIN);
constexpr uint32_t steps_per_mm = 80;
extern bool homing_active = false;

// struct {
//     uint8_t blank_time = 0;        // [16, 24, 36, 54]
//     uint8_t off_time = 5;           // [1..15]
//     uint8_t hysteresis_start = 7;   // [1..8]
//     uint8_t hysteresis_end = 15;     // [0..15] 15 very smooth
// } config;

struct {
    uint8_t blank_time = 0;        // [16, 24, 36, 54]
    uint8_t off_time = 2;           // [1..15]
    uint8_t hysteresis_start = 0;   // [1..8]
    uint8_t hysteresis_end = 0;     // [0..15] 15 very smooth
} config;

struct Button {
  const uint8_t PIN;
  uint32_t numberKeyPresses;
  bool pressed;
};
Button button1 = {HOME_PIN, 0, false};



void IRAM_ATTR isr() {
    if (stepper_task_handle != NULL) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        button1.numberKeyPresses += 1;
        button1.pressed = true;
        // xTaskNotifyFromISR(stepper_task_handle, 2, eSetValueWithoutOverwrite, &xHigherPriorityTaskWoken);
        xTaskNotifyFromISR(stepper_task_handle, HOME_BIT, eSetBits, &xHigherPriorityTaskWoken);
    }
}

int currentPosition;
//float factor = 11.8; // wheel ratio steps per mm
// float factor = 22.6; // wheel ratio steps per mm
float factor = 24.0; // wheel ratio steps per mm

inline void clear_command_queue() {
    stepper_command_t cmd_rcv;
    while (xQueueReceive(xQueue_stepper_command, &cmd_rcv, 0)) {
        ESP_LOGI(TAG, "clear command queue");
    }
}

void go_slack() {
    driver.toff(0); // turn off compeletely (for safety)
    // driver.ihold(0);     // Disable hold current to enable freewheel
    // driver.freewheel(0); // Normal operation
    // driver.freewheel(1); // Freewheeling
    // driver.freewheel(2); // Coil shorted using LS drivers
    // driver.freewheel(3); // Coil shorted using HS drivers
}

void command_move(int type, int move, int speed, int accel, int min, int max){
    //xQueueSendToBack(xQueue_stepper_command, (void *) &move, 0);
    ESP_LOGI(TAG, "Command Move called");
    stepper_command_t test_action;
    test_action.move = move;
    test_action.type = type;

    test_action.speed = speed;
    test_action.accel = accel;
    test_action.min = min;
    test_action.max = max;

    ESP_LOGI(TAG, "About to add to queue");

    xQueueSendToBack(xQueue_stepper_command, (void *) &test_action, 10);           
    ESP_LOGW(TAG, "STEPPER MOVING");
    // vTaskDelay(pdMS_TO_TICKS(500));
    // BaseType_t ret =  xTaskNotify(stepper_task_handle, STOP_BIT, eSetBits); 
    // ESP_LOGW(TAG, "Notify send: %i", (int) ret);
    // BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    // xTaskNotifyFromISR(stepper_task_handle, STOP_BIT, eSetBits, &xHigherPriorityTaskWoken);
}

void sensor_task(void *args) {
    // adc1_config_width(ADC_WIDTH_BIT_12);
    // adc1_config_channel_atten(ADC1_CHANNEL_0,ADC_ATTEN_DB_0);
    // int val = 0;
    
    while(1) {
        // val = digitalRead(button1.PIN);
        ESP_LOGI(TAG, "Velocity: %i SG_RESULT: %i CS_ACTUAL: %i", driver.TSTEP(), driver.SG_RESULT(), driver.cs_actual());
        vTaskDelay(pdMS_TO_TICKS(100));
        // ESP_LOGI(TAG, "Sensor Val: %i", val);
    }
}

void init_strand(int bootPosition) {
    // Start UART and TMC2208
   pinMode(EN_PIN, OUTPUT);
   pinMode(STEP_PIN, OUTPUT);
   pinMode(DIR_PIN, OUTPUT);
   digitalWrite(EN_PIN, LOW);      // Enable driver in hardware
   
   // Driver Setup
   SerialPort.begin(115200);
   driver.begin();
   driver.pdn_disable(true);               // Use PDN/UART pin for communication
   driver.I_scale_analog(false);           // Use internal voltage reference
   driver.mstep_reg_select(1);             // necessary for TMC2208 to set microstep register with UART
   driver.toff(2);                         // Enables driver in software
   driver.rms_current(1700);               // Set motor RMS current 1700
   driver.microsteps(MICROSTEPPING);       // Set microsteps to 1/16th
   driver.en_spreadCycle(false);           // Toggle spr
   driver.VACTUAL(0);                      // make sure velocity is set to 0

   // Stealthchop
   driver.pwm_freq(0); // 2 1 00000
   driver.pwm_autoscale(true);             // Needed for stealthChop
//    driver.pwm_grad(0);
   driver.pwm_ofs(0);
   driver.pwm_autograd(true);
   driver.TPOWERDOWN(4);                   // Minimum of 2 is required for auto tuning of stealth chop

   driver.pwm_lim(10); //8                      // Limit for PWM_SCALE_AUTO when switching back from SpreadCycle to StealthChop. keep above 5
   driver.pwm_reg(8); // 8

   driver.multistep_filt(false);
//    driver.SGTHRS(STALL_VALUE);

    // Spreadcylce
    driver.blank_time(config.blank_time);
    // driver.toff(config.off_time);
    driver.hysteresis_start(config.hysteresis_start);
    driver.hysteresis_end(config.hysteresis_end);

    //STALLGUARDING was miking some funny sounds 
    // driver.TCOOLTHRS(0xFFFFF); // 20bit max
    // driver.THIGH(0);
    // driver.TCOOLTHRS(0xFFFFF); // 20bit max

    // Get stepper config from NVS
    stepper_cfg_t stepper_cfg = command_get_stall();
    ESP_LOGW(TAG, "stall: %i tcool: %i tpwm: %i", stepper_cfg.stall, stepper_cfg.tcool, stepper_cfg.tpwm);
    if ( !(stepper_cfg.stall && stepper_cfg.tcool && stepper_cfg.tpwm) ) { // if any of these are 0, use default values instead
        ESP_LOGW(TAG, "No Stepper config in NVS, use default instead");
        // ESP_LOGW(TAG, "Stepper CFG is null %i", (uint8_t) (stepper_cfg.stall || stepper_cfg.tcool || stepper_cfg.tpwm || 5) );
        stepper_cfg.stall = STALL_VALUE;
        stepper_cfg.tcool = TCOOL_VALUE;
        stepper_cfg.tpwm = TPWMTHRS_THR;    
    }

        /* ----THRESHOLD----
        * Changing the 'thr' variable raises or lowers the velocity at which the stepper motor switches between StealthChop and SpreadCycle
        * - Low values results in SpreadCycle being activated at lower velocities
        * - High values results in SpreadCycle being activated at higher velocities
        * - If SpreadCycle is active while too slow, there will be noise
        * - If StealthChop is active while too fast, there will also be noise
        * For the 15:1 stepper, values between 70-120 is optimal 
    */

    driver.TCOOLTHRS(stepper_cfg.stall); // 20bit max
    // driver.semin(1);
    // driver.semax(5);
    // driver.sedn(0b01);
    driver.SGTHRS((uint8_t) stepper_cfg.stall);

    driver.TPWMTHRS(stepper_cfg.tpwm);
   // Stepper Library Setup

   // Disable this when mirostepping is disabled
//    stepper.setMaxSpeed(1400*MICROSTEPPING); // 100mm/s @ 80 steps/mm
//    stepper.setAcceleration(1000*MICROSTEPPING); // 2000mm/s^2

   stepper.setEnablePin(EN_PIN);
   stepper.setPinsInverted(true, false, true);
   stepper.enableOutputs();

   currentPosition = bootPosition;
    ESP_LOGI(TAG,"current Position %d",currentPosition);

    //Driver Tests 
    if (driver.drv_err()) {
        ESP_LOGW(TAG, "Driver ERROR");
    }

    ESP_LOGI(TAG,"\nTesting connection...");
    uint8_t result = driver.test_connection();

    if (result) {
    ESP_LOGI(TAG,"failed!");
    ESP_LOGI(TAG,"Likely cause: ");

    switch(result) {
        case 0: ESP_LOGW(TAG,"SUCCESS"); break;
        case 1: ESP_LOGW(TAG,"loose connection"); server_ping("ERROR : Lose Connection");break;
        case 2: ESP_LOGW(TAG,"no power"); server_ping("ERROR : No Power"); break;
        default: ESP_LOGW(TAG,"Default. result: %i", result); break;
    }
    ESP_LOGI(TAG,"Fix the problem and reset board.");
    // We need this delay or messages above don't get fully printed out
    delay(100);
    //server_ping("ERROR");//Sends the boot up message to the server

    }

    

    // Set stepper interrupt

//    pinMode(button1.PIN, INPUT_PULLUP);
//    attachInterrupt(digitalPinToInterrupt(button1.PIN), isr, FALLING);
   pinMode(button1.PIN, INPUT_PULLDOWN);
   attachInterrupt(digitalPinToInterrupt(button1.PIN), isr, RISING);

    // Used to monitor stallguard value
    // xTaskCreatePinnedToCore(&sensor_task, "sensor_task", 2*1024, NULL, 3, &sensor_task_handle, 0);
   //driver.VACTUAL(6400);
}

void stepper_task(void *args) {
    // esp_task_wdt_feed();
    ESP_LOGI(TAG, "Init Stepper Queue");
    // Setup the data structure to store and retrieve stepper commands
    xQueue_stepper_command = xQueueCreate(10, sizeof(stepper_command_t));
    if (xQueue_stepper_command == NULL) ESP_LOGE(TAG, "Unable to create stepper command queue");

    int stepper_move = 0; // storage for incoming stepper command
    int stepper_target = 0;
    
    BaseType_t xResult;
    uint32_t notify = 0;
    

    ESP_LOGI(TAG, "Start Stepper Task");
    while(1) {

        // driver.toff(2); // turn stepper back on again

        if (xQueueReceive(xQueue_stepper_command, &stepper_commands, portMAX_DELAY)) {

            driver.toff(2); // turn stepper back on again

            // driver.(thr);
            // thr+=20;
            // ESP_LOGW(TAG, "Threshold: %i", thr);
            //if type 0 DONT record the position (relative)
            //ESP_LOGI(TAG, "Stepper Type %d", stepper_commands.type);
            
            stepper.setMaxSpeed(stepper_commands.speed); // 100mm/s @ 80 steps/mm
            stepper.setAcceleration(stepper_commands.accel); // 100mm/s @ 80 steps/mm

            if (stepper_commands.type == ABS){

                if (stepper_commands.move <= stepper_commands.min) {
                    ESP_LOGI(TAG, "MIN");
                    stepper_target = stepper_commands.min;
                }
                else if (stepper_commands.move  >= stepper_commands.max){
                    ESP_LOGI(TAG, "MAX"); 
                    stepper_target = stepper_commands.max;
                }
                else {
                    stepper_target = stepper_commands.move;
                }
                stepper_move = (stepper_target - currentPosition) * factor;//works out based on mm

                ESP_LOGI(TAG, "Stepper Move To : %d Dif %d : Current : %d",stepper_commands.move, stepper_move, currentPosition);

                currentPosition = stepper_target;
                 //save out and back to main = currentPosition;
            }
            if (stepper_commands.type == REL){
                stepper_move = stepper_commands.move;
                ESP_LOGI(TAG, "Stepper Move %d : %d", stepper_move, currentPosition);
            }
            //if type 1 record the position 
            //Print
            ESP_LOGI(TAG, "Stepper Move %d", stepper_move);
            // Set distance to move from comand variable
            stepper.move(stepper_move);
            // Run the stepper loop until we get to our destination
            while(stepper.distanceToGo() != 0) {
                static uint32_t last_time=0;
                // uint32_t ms = millis();
                // if (ulTaskNotifyTake(pdTRUE, 0) > 1) {
                //     ESP_LOGW(TAG, "Notify receive");
                // }
                // Serial.print(driver.SG_RESULT(), DEC);
                // notify = ulTaskNotifyTake(pdTRUE, 0);
                xResult = xTaskNotifyWait( pdFALSE,    /* Don't clear bits on entry. */
                           ULONG_MAX,        /* Clear all bits on exit. */
                           &notify, /* Stores the notified value. */
                           0 ); // Don't block

                if (xResult  == pdPASS) {
                    ESP_LOGW(TAG, "Notification Received: %i", notify);
                    if (notify & HOME_BIT) {
                        if (homing_active) { // used to prevent accidental homing
                            driver.toff(0); // turn off compeletely (for safety)
                            driver.toff(2); // and back on again
                            stepper.setCurrentPosition(0);
                            // stepper.runToNewPosition(-600); // reel out
                            server_ping("home");
                            // Check if homing is active, then set the current position
                            homing_active = false;
                            currentPosition = 0;
                        clear_command_queue();
                        } else { // got stuck for some other reason. Probably still a good idea to stop
                            driver.toff(0); // turn off compeletely (for safety)
                            driver.toff(2); // and back on again
                            stepper.setAcceleration(10000);
                            stepper.stop();
                        }
                        notify = 0; // reset notification 
                    }
                    if (notify & STOP_BIT) {
                        // Check if we have received a notificaiton value to overrid the stepper task
                        //ESP_LOGI(TAG, "Stepper STOP");
                        //ESP_LOGW(TAG, "Notify receive", ulTaskNotifyTake(pdTRUE, 0););
                        stepper.stop();
                        stepper.setCurrentPosition(stepper.currentPosition());
                        clear_command_queue();
                        // stepper.setCurrentPosition(stepper.targetPosition());
                        
                        notify = 0; // reset notification 
                        // break;
                    }

                    if (notify & SLACK_BIT) {
                        // stepper.setCurrentPosition(stepper.currentPosition());
                        driver.toff(0); // turn off stepper compeletely 
                        stepper.stop();
                        stepper.setCurrentPosition(stepper.currentPosition());
                        clear_command_queue();
                        notify = 0; // reset notification 
                        vTaskDelay(pdMS_TO_TICKS(1000));
                        // break;
                    
                    }
                }

                // while (Serial.available() > 0) {
                //     int8_t read_byte = Serial.read();

                //     if (driver.SG_RESULT() < 200) {
                //         driver.toff(0);
                //         Serial.println("Motor stop");
                //         break;
                //     }
                // }
                stepper.run();
                // if((ms-last_time) > 500) { //run every 0.1s
                //     last_time = ms;
                //     driver.hysteresis_end(config.hysteresis_end++ % 15);
                //     // ESP_LOGI(TAG, "Velocity: %i SG_RESULT: %i", driver.TSTEP(), driver.SG_RESULT());
                //     // Serial.print(driver.SG_RESULT(), DEC);
                // }
                
                // vTaskDelay(1);
            }

            if (stepper_commands.type == 1){//saving position once moved 
                setParameter(1, currentPosition);
            }
            
        }
        
    }

}