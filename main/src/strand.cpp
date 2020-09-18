
// #define SPI_DEVICE_NO_DUMMY

#include "Strand.hpp"
#include <HardwareSerial.h>
#include <SPI.h>
#include <TMCStepper.h>
#include <AccelStepper.h>
#include "net.h"

static const char *TAG = "STEPPER";

QueueHandle_t xQueue_stepper_command; // Must redefine here

stepper_command_t stepper_commands;

//uninitalised pointers to SPI objects
static const int spiClk = 1000000; // 1 MHz
// SPIClass * vspi = NULL;
// VSPI
// SPIClass SPI(VSPI);

//HardwareSerial SerialPort(2);
const int uart_buffer_size = (1024 * 2);
#define RXD2 16
#define TXD2 17
#define EN_PIN           22 // Enable (5)
#define DIR_PIN          16 // Direction (14)
#define STEP_PIN         17 // Step (12)

#define cs_pin              5   // Chip select
#define mosi_pin            23 // Software Master Out Slave In (MOSI) 23
#define miso_pin            19 // Software Master In Slave Out (MISO) 19
#define sck_pin            18 // Software Slave Clock (SCK)
#define DRIVER_ADDRESS 0b00

// #define CS_PIN              SS   // Chip select
// #define MOSI_PIN            MOSI // Software Master Out Slave In (MOSI)
// #define MISO_PIN            MISO // Software Master In Slave Out (MISO)
// #define SCK_PIN             SCK // Software Slave Clock (SCK)
// #define writeMOSI_H digitalWrite(mosi_pin, HIGH)
// #define writeMOSI_L digitalWrite(mosi_pin, LOW)
// #define writeSCK_H digitalWrite(sck_pin, HIGH)
// #define writeSCK_L digitalWrite(sck_pin, LOW)
// #define readMISO digitalRead(miso_pin)

// #define DIR_PIN          19 // Direction (Oliver)
// #define STEP_PIN         14 // Step  (Oliver)
//#define R_SENSE 0.11f //TMC2208
#define R_SENSE 0.075f//TMC5160
//homiing buttion stuff
#define HOME_PIN         34 // HOME

////TMC2208Stepper driver(&SerialPort, R_SENSE); 
// SPIClass vspi = SPIClass(VSPI);;
// SW_SPIClass SW_SPI(MOSI_PIN, MISO_PIN, SCK_PIN);
TMC5160Stepper driver(cs_pin, R_SENSE);
// TMC5160Stepper driver(cs_pin, R_SENSE, mosi_pin, miso_pin, sck_pin);
// TMC5160Stepper driver(vspi, CS_PIN, R_SENSE);
// TMC5160Stepper driver(SW_SPI, CS_PIN, R_SENSE);
// TMC5160Stepper driver(CS_PIN, R_SENSE);
// TMC5160Stepper driver = TMC5160Stepper(CS_PIN, R_SENSE);
// TMC5160Stepper driver = TMC5160Stepper(CS_PIN, R_SENSE, MOSI_PIN, MOSI_PIN, SCK_PIN);

AccelStepper stepper = AccelStepper(stepper.DRIVER, STEP_PIN, DIR_PIN);
constexpr uint32_t steps_per_mm = 80;

struct {
    uint8_t blank_time = 16;        // [16, 24, 36, 54]
    uint8_t off_time = 1;           // [1..15]
    uint8_t hysteresis_start = 8;   // [1..8]
    int8_t hysteresis_end = 12;     // [-3..12]
} config;

long currentPosition;
float factor = 11.8; // wheel ratio steps per mm

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

void init_strand() {
    // Start UART and TMC2208
   // Driver Setup

    // void SPIClass::begin(int8_t sck, int8_t miso, int8_t mosi, int8_t ss)
//    SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CS_PIN);//TMC5160 SPI Begin

//    vspi = new SPIClass(VSPI);
//    vspi->begin();

   //SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CS_PIN);
//    SPI.begin();
//    SPI.setBitOrder(MSBFIRST);
//     SPI.setDataMode(SPI_MODE0);
//     SPI.setClockDivider(SPI_CLOCK_DIV8); // divide the clock by 8=2 MHz @ESP32

   //vspi = new SPIClass(VSPI);

   //SerialPort.begin(115200);
   pinMode(EN_PIN, OUTPUT);
   pinMode(DIR_PIN, OUTPUT);
   pinMode(STEP_PIN, OUTPUT);
//    pinMode(CS_PIN, OUTPUT);
//    digitalWrite(CS_PIN, HIGH);
//    digitalWrite(EN_PIN, HIGH);      // Enable driver in hardware
    digitalWrite(EN_PIN, LOW); //deactivate driver (LOW active)
    // digitalWrite(DIR_PIN, LOW); //LOW or HIGH
    // digitalWrite(STEP_PIN, LOW);
    // digitalWrite(CS_PIN, HIGH);
//    digitalWrite(EN_PIN, LOW);      // Enable driver in hardware
    


//    pinMode(CS_PIN, OUTPUT);
//    pinMode(SCK_PIN, OUTPUT);
//    pinMode(MOSI_PIN, OUTPUT);

    // SPI.begin();
    // SPI.setFrequency(16000000/8);
    // vspi.begin();
    // vspi.setFrequency(16000000/8);
    //SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CS_PIN);
   SPI.begin(); 
//    SPI.begin(sck_pin, miso_pin, mosi_pin, cs_pin);
//    pinMode(CS_PIN, OUTPUT);
//    digitalWrite(CS_PIN, LOW);
//    SPI.setFrequency(100000);
//    SPI.beginTransaction(SPISettings(100000, MSBFIRST, SPI_MODE0));
//     while (true) {
//         SPI.transfer(0x9F);
//         vTaskDelay(10);
//     }
//    vTaskDelay(pdMS_TO_TICKS(2000));
//    SPI.transfer(0x9F);

//    pinMode(MISO_PIN, INPUT_PULLUP);
   driver.begin();//Begin TMC
    // SW_SPI.init();
    // SW_SPI.begin();
//    driver.push();

//    driver.pdn_disable(true);     // Use PDN/UART pin for communication
   //driver.I_scale_analog(false); // Use internal voltage reference
   //driver.mstep_reg_select(1);  // necessary for TMC2208 to set microstep register with UART
   //driver.toff(5);                 // Enables driver in software
   int err = driver.GSTAT();
   ESP_LOGW(TAG, "Driver stat: %i", err);
//    driver.toff(0);
   driver.reset();
   driver.toff(4);
   driver.blank_time(24);
   driver.rms_current(400);        // Set motor RMS current
   driver.microsteps(16);          // Set microsteps to 1/16th
   //driver.en_spreadCycle(false);   // Toggle spr
   //driver.VACTUAL(0); // make sure velocity is set to 0
   //driver.pwm_autoscale(true);     // Needed for stealthChop
   driver.en_pwm_mode(1);      // Enable extremely quiet stepping
   // driver.pwm_autoscale(1);
    if (driver.drv_err()) {
        ESP_LOGW(TAG, "Driver ERROR");
    }
   
   //Driver Tests 
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

   //    uint8_t result_1 = driver.test_connection();
   //    ESP_LOGI(TAG, "Driver: %i", result_1);

    /* ----THRESHOLD----
     * Changing the 'thr' variable raises or lowers the velocity at which the stepper motor switches between StealthChop and SpreadCycle
     * - Low values results in SpreadCycle being activated at lower velocities
     * - High values results in SpreadCycle being activated at higher velocities
     * - If SpreadCycle is active while too slow, there will be noise
     * - If StealthChop is active while too fast, there will also be noise
     * For the 15:1 stepper, values between 70-120 is optimal 
    */
    uint32_t thr = 100; // 70-120 is optimal
    driver.TPWMTHRS(thr);


   // Stepper Library Setup
   stepper.setMaxSpeed(1600); // 100mm/s @ 80 steps/mm
   stepper.setAcceleration(2000); // 2000mm/s^2
   stepper.setEnablePin(EN_PIN);
   stepper.setPinsInverted(false, false, true);
   stepper.enableOutputs();

   // while(SerialPort.available()) {
   //  ESP_LOGI(TAG, "Serial read: %c", char(SerialPort.read()));
   //  vTaskDelay(100);
   // }

    //SENSOR
    pinMode(button1.PIN, INPUT);
    attachInterrupt(button1.PIN, isr, FALLING);
    // int cmd = 5000;
    
}

void stepper_task(void *args) {
    ESP_LOGI(TAG, "Init Stepper Queue");
    // Setup the data structure to store and retrieve stepper commands
    xQueue_stepper_command = xQueueCreate(10, sizeof(stepper_command_t));
    if (xQueue_stepper_command == NULL) ESP_LOGE(TAG, "Unable to create stepper command queue");

    long stepper_move = 0; // storage for incoming stepper command
    int stepper_target = 0;


    ESP_LOGI(TAG, "Start Stepper Task");
    while(1) {

        if (xQueueReceive(xQueue_stepper_command, &stepper_commands, portMAX_DELAY)) {
            //if type 0 DONT record the position (relative)
            //ESP_LOGI(TAG, "Stepper Type %d", stepper_commands.type);

            //stepper.setMaxSpeed(stepper_commands.speed); // 100mm/s @ 80 steps/mm


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
                if (button1.pressed) {
                    ESP_LOGI(TAG,"Button 1 has been pressed %u times\n", button1.numberKeyPresses);
                    button1.pressed = false;
                    server_ping("home");//Sends the boot up message to the server
                    stepper.stop();
                    
                    //stepper.move(200);
                }
                // }
                stepper.run();
                // vTaskDelay(1);
            }
        }
        
    }

}