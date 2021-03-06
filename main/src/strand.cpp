

#include "Strand.hpp"
#include <HardwareSerial.h>
#include <TMCStepper.h>
// #include "AccelStepper.h"
#include <AccelStepper.h>
#include "net.h"
#include <driver/adc.h>

static const char *TAG = "STEPPER";

QueueHandle_t xQueue_stepper_command; // Must redefine here

stepper_command_t stepper_commands;

HardwareSerial SerialPort(2);
//#define STALL_VALUE     100 // [0..255]

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
#define DRIVER_ADDRESS  0b00       // TMC2209 Driver address according to MS1 and MS2
#define MICROSTEPPING         8// MICROSTEPPING 8
//homiing buttion stuff
// #define HOME_PIN         32 // HOME (Oliver)
#define HOME_PIN         23 // HOME
#define TCOOL_VALUE     150 // 150
#define STALL_VALUE     105 // 150
#define TPWMTHRS_THR    10 // 140

//TMC2208Stepper driver(&SerialPort, R_SENSE); 
TMC2209Stepper driver(&SerialPort, R_SENSE , DRIVER_ADDRESS);

AccelStepper stepper = AccelStepper(stepper.DRIVER, STEP_PIN, DIR_PIN);
constexpr uint32_t steps_per_mm = 80;
bool home = false;

int currentPosition;

struct {
    uint8_t blank_time = 16;        // [16, 24, 36, 54]
    uint8_t off_time = 1;           // [1..15]
    uint8_t hysteresis_start = 8;   // [1..8]
    int8_t hysteresis_end = 12;     // [-3..12]
} config;

struct Button {
  const uint8_t PIN;
  uint32_t numberKeyPresses;
  bool pressed;
};
Button button1 = {HOME_PIN, 0, false};

int solenoidPin = 22;  
 

void IRAM_ATTR isr() {
    if (stepper_task_handle != NULL) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        button1.numberKeyPresses += 1;
        button1.pressed = true;
        // xTaskNotifyFromISR(stepper_task_handle, 2, eSetValueWithoutOverwrite, &xHigherPriorityTaskWoken);
        xTaskNotifyFromISR(stepper_task_handle, HOME_BIT, eSetBits, &xHigherPriorityTaskWoken);
    }
}

//float factor = 11.8; // wheel ratio steps per mm
float factor = 22.6; // wheel ratio steps per mm

void command_move(int type, int move, int speed, int accel, int time, int min, int max){
    //xQueueSendToBack(xQueue_stepper_command, (void *) &move, 0);
    stepper_command_t test_action;
    //int stepper_target = 0;
    if (type >= 1){
        if (move <= min) {
            ESP_LOGI(TAG, "MIN");
            test_action.move = min;
        }
        else if (move  >= max){
            ESP_LOGI(TAG, "MAX"); 
            test_action.move= max;
        }
        else {
            test_action.move = move;
        }
    
        //test_action.move = (stepper_target - currentPosition) * factor;//works out based on mm
    } else if (type == 0){
        test_action.move = move;
    }


    test_action.type = type;

    test_action.speed = speed;
    test_action.accel = accel;
    test_action.time = time;
    test_action.min = min;
    test_action.max = max;

    xQueueSendToBack(xQueue_stepper_command, (void *) &test_action, 20);           
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
    int val = 0;
    
    while(1) {
        val = digitalRead(button1.PIN);
        vTaskDelay(pdMS_TO_TICKS(100));
        ESP_LOGI(TAG, "Sensor Val: %i", val);
    }
}

void init_strand(int bootPosition) {
    // Start UART and TMC2208
   pinMode(EN_PIN, OUTPUT);
   pinMode(STEP_PIN, OUTPUT);
   pinMode(DIR_PIN, OUTPUT);
   digitalWrite(EN_PIN, LOW);      // Enable driver in hardware

   pinMode(solenoidPin, OUTPUT); //enable solinoid
   
   // Driver Setup
   SerialPort.begin(115200);
   driver.begin();
   driver.pdn_disable(true);               // Use PDN/UART pin for communication
   driver.I_scale_analog(false);           // Use internal voltage reference
   driver.mstep_reg_select(1);             // necessary for TMC2208 to set microstep register with UART
   driver.toff(3);                         // Enables driver in software
   driver.rms_current(1200);               // Set motor RMS current
   driver.microsteps(MICROSTEPPING);       // Set microsteps to 1/16th
   driver.en_spreadCycle(false);           // Toggle spr
   driver.VACTUAL(0);                      // make sure velocity is set to 0
   driver.pwm_autoscale(true);             // Needed for stealthChop
//    driver.SGTHRS(STALL_VALUE);

    //STALLGUARDING was miking some funny sounds 
    // driver.TCOOLTHRS(0xFFFFF); // 20bit max
    //driver.THIGH(0);
    // driver.TCOOLTHRS(0xFFFFF); // 20bit max
    
    driver.TCOOLTHRS(TCOOL_VALUE); // 20bit max
    driver.semin(1);
    driver.semax(5);
    driver.sedn(0b01);
    driver.SGTHRS(STALL_VALUE);

   // Stepper Library Setup
   stepper.setMaxSpeed(1400*MICROSTEPPING); // 100mm/s @ 80 steps/mm
   stepper.setAcceleration(1000*MICROSTEPPING); // 2000mm/s^2
   stepper.setEnablePin(EN_PIN);
   stepper.setPinsInverted(false, false, true);
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
        case 0: ESP_LOGW(TAG,"SUCCESS");break;
        case 1: ESP_LOGW(TAG,"loose connection"); server_ping("ERROR : Lose-Connection");break;
        case 2: ESP_LOGW(TAG,"no power"); server_ping("ERROR : No-Power"); break;
        default: ESP_LOGW(TAG,"Default. result: %i", result); break;
    }
    ESP_LOGI(TAG,"Fix the problem and reset board.");
    // We need this delay or messages above don't get fully printed out
    delay(100);
    //server_ping("ERROR");//Sends the boot up message to the server

    }

    /* ----THRESHOLD----
        * Changing the 'thr' variable raises or lowers the velocity at which the stepper motor switches between StealthChop and SpreadCycle
        * - Low values results in SpreadCycle being activated at lower velocities
        * - High values results in SpreadCycle being activated at higher velocities
        * - If SpreadCycle is active while too slow, there will be noise
        * - If StealthChop is active while too fast, there will also be noise
        * For the 15:1 stepper, values between 70-120 is optimal 
    */

    driver.TPWMTHRS(TPWMTHRS_THR);

    // Set stepper interrupt

//    pinMode(button1.PIN, INPUT_PULLUP);
//    attachInterrupt(digitalPinToInterrupt(button1.PIN), isr, FALLING);
   pinMode(button1.PIN, INPUT_PULLDOWN);
   attachInterrupt(digitalPinToInterrupt(button1.PIN), isr, RISING);

   driver.VACTUAL(6400);
}

void stepper_task(void *args) {
    // esp_task_wdt_feed();
    ESP_LOGI(TAG, "Init Stepper Queue");
    // Setup the data structure to store and retrieve stepper commands
    xQueue_stepper_command = xQueueCreate(40, sizeof(stepper_command_t));
    if (xQueue_stepper_command == NULL) ESP_LOGE(TAG, "Unable to create stepper command queue");

    int stepper_move = 0; // storage for incoming stepper command
    
    BaseType_t xResult;
    uint32_t notify = 0;

    ESP_LOGI(TAG, "Start Stepper Task");
    while(1) {

        if (xQueueReceive(xQueue_stepper_command, &stepper_commands, portMAX_DELAY)) {

            // driver.(thr);
            // thr+=20;
            // ESP_LOGW(TAG, "Threshold: %i", thr);
            //if type 0 DONT record the position (relative)
            //ESP_LOGI(TAG, "Stepper Type %d", stepper_commands.type);
            
            float speed = 1.0;
            
            if (stepper_commands.type == 2){
                
                stepper_move = (stepper_commands.move - currentPosition) * factor;//works out based on mm
                speed =  abs(stepper_move /float(stepper_commands.time));
                float stepperSpeed = float(stepper_commands.speed) * speed;

                ESP_LOGI(TAG, "Stepper Move To : %d Dif %d : Current : %d",stepper_commands.move, stepper_move, currentPosition);
                ESP_LOGI(TAG, "Stepper Speed %f - %d : %f",speed, stepper_commands.time, stepperSpeed);

                currentPosition = stepper_commands.move;
                stepper.setAcceleration(stepper_commands.accel); // 100mm/s @ 80 steps/mm
                //Speed
                if (stepper_commands.speed <10000 && stepperSpeed >= 100){
                    stepper.setMaxSpeed(int(stepperSpeed)); // 100mm/s @ 80 steps/mm
                    //stepper.setAcceleration(int(stepper_commands.accel*speed)); // 100mm/s @ 80 steps/mm
                } else if (stepperSpeed<100){
                    stepper.setMaxSpeed(100); // 100mm/s @ 80 steps/mm
                    //stepper.setAcceleration(100); // 100mm/s @ 80 steps/mm
                } else {
                    stepper.setMaxSpeed(10000); // 100mm/s @ 80 steps/mm
                    //stepper.setAcceleration(10000); // 100mm/s @ 80 steps/mm
                }
            }
            if (stepper_commands.type == 1){

                stepper_move = (stepper_commands.move - currentPosition) * factor;//works out based on mm

                ESP_LOGI(TAG, "Stepper Move To : %d Dif %d : Current : %d",stepper_commands.move, stepper_move, currentPosition);

                currentPosition = stepper_commands.move;
                //save out and back to main = currentPosition;
                stepper.setMaxSpeed(stepper_commands.speed); // 100mm/s @ 80 steps/mm
                stepper.setAcceleration(stepper_commands.accel); // 100mm/s @ 80 steps/mm
            }
            if (stepper_commands.type == 0){
                stepper_move = stepper_commands.move;
                ESP_LOGI(TAG, "Stepper Move %d : %d", stepper_move, currentPosition);
                stepper.setMaxSpeed(stepper_commands.speed); // 100mm/s @ 80 steps/mm
                stepper.setAcceleration(stepper_commands.accel); // 100mm/s @ 80 steps/mm
            }

            digitalWrite(solenoidPin, HIGH);      //Switch Solenoid ON

            //if type 1 record the position 
            //Print
            ESP_LOGI(TAG, "Stepper Move %d", stepper_move);
            // Set distance to move from comand variable
            
            stepper.move(stepper_move);
            // Run the stepper loop until we get to our destination
            while(stepper.distanceToGo() != 0) {
                static uint32_t last_time=0;
                uint32_t ms = millis();
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
                        driver.toff(0); // turn off compeletely (for safety)
                        driver.toff(4); // and back on again
                        stepper.setCurrentPosition(0);
                        stepper.runToNewPosition(4000);
                        home=false;
                        setPramamter(1, 0);
                        currentPosition = 0;
                        saveParamters();
                        notify = 0;
                    }
                    if (notify & STOP_BIT) {
                        // Check if we have received a notificaiton value to overrid the stepper task
                        //ESP_LOGI(TAG, "Stepper STOP");
                        //ESP_LOGW(TAG, "Notify receive", ulTaskNotifyTake(pdTRUE, 0););

                        //Distance Shift Calculations 
                        int pos = stepper.currentPosition();
                        float to = stepper.distanceToGo() / factor;
                        // ESP_LOGI(TAG, "Target %d", currentPosition);
                        // ESP_LOGI(TAG, "Current Device %d", pos);
                        // ESP_LOGI(TAG, "To Go %f", to);
                        if (stepper_move <= 0 ){
                            currentPosition = currentPosition + to;
                        } else {
                            currentPosition = currentPosition - to;
                        }
                        int t = currentPosition * factor;
                        ESP_LOGI(TAG, "Updated Target %d - %d", t, pos);
                    
                        stepper.stop();

                        // to = stepper.distanceToGo() / factor;
                        // ESP_LOGI(TAG, "To Go %f", to);
                        // if (stepper_move <= 0 ){
                        //     currentPosition = currentPosition + to;
                        // } else {
                        //     currentPosition = currentPosition - to;
                        // }

                        notify = 0;
                        // break;
                    }
                }

                // while (Serial.available() > 0) {
                //     int8_t read_byte = Serial.read();

                    // if (driver.SG_RESULT() < 200) {
                    //     driver.toff(0);
                    //     Serial.println("Motor stop");
                    //     break;
                    // }
                // }
                stepper.run();
                // if((ms-last_time) > 100) { //run every 0.1s
                //     last_time = ms;

                //     ESP_LOGI(TAG, "Velocity: %i", driver.TSTEP());
                // }
                
                // vTaskDelay(1);
            }

            if (stepper_commands.type >= 1){//saving position once moved 
                setPramamter(1, currentPosition);
            }
            
            digitalWrite(solenoidPin, LOW);       //Switch Solenoid OFF

            
        }
        
    }

}

