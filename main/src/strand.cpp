

#include "Strand.hpp"
#include <HardwareSerial.h>
#include <TMCStepper.h>
#include <AccelStepper.h>
#include "net.h"
#include <driver/adc.h>

static const char *TAG = "STEPPER";

QueueHandle_t xQueue_stepper_command; // Must redefine here

stepper_command_t stepper_commands;

HardwareSerial SerialPort(2);
const int uart_buffer_size = (1024 * 2);
#define RXD2             16  //UART
#define TXD2             17  //UART
#define EN_PIN           5   // Enable
#define DIR_PIN          14  // Direction
#define STEP_PIN         12  // Step
// #define DIR_PIN          19 // Direction (Oliver)
// #define STEP_PIN         14 // Step  (Oliver)
#define R_SENSE 0.11f
#define DRIVER_ADDRESS  0b00       // TMC2209 Driver address according to MS1 and MS2
#define MICROSTEPPING         8// MICROSTEPPING 8
//homiing buttion stuff
#define HOME_PIN         34 // HOME

//TMC2208Stepper driver(&SerialPort, R_SENSE); 
TMC2209Stepper driver(&SerialPort, R_SENSE , DRIVER_ADDRESS);

AccelStepper stepper = AccelStepper(stepper.DRIVER, STEP_PIN, DIR_PIN);
constexpr uint32_t steps_per_mm = 80;

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

void IRAM_ATTR isr() {
  button1.numberKeyPresses += 1;
  button1.pressed = true;
}

long currentPosition;
float factor = 11.8; // wheel ratio steps per mm

void command_move(int type, int move, int speed, int accel, int min, int max){
    //xQueueSendToBack(xQueue_stepper_command, (void *) &move, 0);
    stepper_command_t test_action;
    test_action.move = move;
    test_action.type = type;
    test_action.speed = speed;
    test_action.accel = accel;
    test_action.min = min;
    test_action.max = max;

    xQueueSendToBack(xQueue_stepper_command, (void *) &test_action, 0);            
}

void sensor_task(void *args) {
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_0,ADC_ATTEN_DB_0);
    int val = 0;
    
    while(1) {
        val = adc1_get_raw(ADC1_CHANNEL_0);
        Serial.println(val);
        vTaskDelay(pdMS_TO_TICKS(10));
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

    driver.rms_current(1200);               // Set motor RMS current
    driver.microsteps(MICROSTEPPING);       // Set microsteps to 1/16th
    driver.en_spreadCycle(false);           // Toggle spr
    driver.VACTUAL(0);                      // make sure velocity is set to 0

    // Stealthchop Config
    driver.pwm_autoscale(true);             // Needed for stealthChop
    driver.pwm_autograd(true);

    // Spreadcycle Config
    driver.toff(4);                         // Enables driver in software
    driver.tbl(1);
    driver.hstrt(0);
    driver.hend(0);
    // driver.pwm_lim(10);

    // Stepper Library Setup
    stepper.setMaxSpeed(1400*MICROSTEPPING); // 100mm/s @ 80 steps/mm
    stepper.setAcceleration(1000*MICROSTEPPING); // 2000mm/s^2
    stepper.setEnablePin(EN_PIN);
    stepper.setPinsInverted(false, false, true);
    stepper.enableOutputs();

    currentPosition = bootPosition;

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
        case 1: ESP_LOGW(TAG,"loose connection"); break;
        case 2: ESP_LOGW(TAG,"no power"); break;
        default: ESP_LOGW(TAG,"Default. result: %i", result); break;
    }
    ESP_LOGI(TAG,"Fix the problem and reset board.");
    // We need this delay or messages above don't get fully printed out
    delay(100);
    server_ping("ERROR");//Sends the boot up message to the server

    }

    /* ----THRESHOLD----
        * Changing the 'thr' variable raises or lowers the velocity at which the stepper motor switches between StealthChop and SpreadCycle
        * - Low values results in SpreadCycle being activated at lower velocities
        * - High values results in SpreadCycle being activated at higher velocities
        * - If SpreadCycle is active while too slow, there will be noise
        * - If StealthChop is active while too fast, there will also be noise
        * For the 15:1 stepper, values between 70-120 is optimal 
    */
    uint32_t thr = 140; // 70-120 is optimal
    driver.TPWMTHRS(thr);


    pinMode(button1.PIN, INPUT);
    attachInterrupt(button1.PIN, isr, FALLING);
}

void stepper_task(void *args) {
    ESP_LOGI(TAG, "Init Stepper Queue");
    // Setup the data structure to store and retrieve stepper commands
    xQueue_stepper_command = xQueueCreate(10, sizeof(stepper_command_t));
    if (xQueue_stepper_command == NULL) ESP_LOGE(TAG, "Unable to create stepper command queue");

    long stepper_move = 0; // storage for incoming stepper command
    int stepper_target = 0;
    // uint32_t thr = 0; // 70-120 is optimal

    ESP_LOGI(TAG, "Start Stepper Task");
    while(1) {
        vTaskDelay(10);

        if (xQueueReceive(xQueue_stepper_command, &stepper_commands, portMAX_DELAY)) {

            // driver.TPWMTHRS(thr);
            // thr+=20;
            // ESP_LOGW(TAG, "Threshold: %i", thr);
            //if type 0 DONT record the position (relative)
            //ESP_LOGI(TAG, "Stepper Type %d", stepper_commands.type);

            stepper.setMaxSpeed(stepper_commands.speed); // 100mm/s @ 80 steps/mm
            stepper.setAcceleration(stepper_commands.accel); // 100mm/s @ 80 steps/mm


            if (stepper_commands.type == 1){

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

                ESP_LOGI(TAG, "Stepper Move To : %ld Dif %ld : Current : %ld",stepper_commands.move, stepper_move, currentPosition);

                currentPosition = stepper_target;
                 //save out and back to main = currentPosition;
            }
            if (stepper_commands.type == 0){
                stepper_move = stepper_commands.move;
                ESP_LOGI(TAG, "Stepper Move %ld : %ld", stepper_move, currentPosition);
            }
            //if type 1 record the position 
            //Print
            ESP_LOGI(TAG, "Stepper Move %ld", stepper_move);
            // Set distance to move from comand variable
            stepper.move(stepper_move);
            // Run the stepper loop until we get to our destination
            while(stepper.distanceToGo() != 0) {
                // if (!button1.pressed){
                // if (button1.pressed) {
                //     Serial.printf("Button 1 has been pressed %u times\n", button1.numberKeyPresses);
                //     button1.pressed = false;
                //     //stepper.stop();
                //     // stepper.currentPosition(0)
                //     currentPosition = stepper_commands.min;
                //     server_ping("home");//Sends the boot up message to the server
                // }

                // }
                stepper.run();
                // vTaskDelay(1);
            }
            vTaskDelay(0);
        }
        
    }

}