

#include "Strand.hpp"
#include <HardwareSerial.h>
#include <TMCStepper.h>
#include <AccelStepper.h>

static const char *TAG = "STEPPER";

QueueHandle_t xQueue_stepper_command; // Must redefine here

HardwareSerial SerialPort(2);
const int uart_buffer_size = (1024 * 2);
#define RXD2 16
#define TXD2 17
#define EN_PIN           5 // Enable
#define DIR_PIN          19 // Direction
#define STEP_PIN         14 // Step
#define R_SENSE 0.11f

TMC2208Stepper driver(&SerialPort, R_SENSE); 
AccelStepper stepper = AccelStepper(stepper.DRIVER, STEP_PIN, DIR_PIN);
constexpr uint32_t steps_per_mm = 80;

struct {
    uint8_t blank_time = 16;        // [16, 24, 36, 54]
    uint8_t off_time = 1;           // [1..15]
    uint8_t hysteresis_start = 8;   // [1..8]
    int8_t hysteresis_end = 12;     // [-3..12]
} config;

void init_strand() {
    // Start UART and TMC2208
   pinMode(EN_PIN, OUTPUT);
   pinMode(STEP_PIN, OUTPUT);
   pinMode(DIR_PIN, OUTPUT);
   digitalWrite(EN_PIN, LOW);      // Enable driver in hardware
   
   // Driver Setup
   SerialPort.begin(115200);
   driver.begin();
   driver.pdn_disable(true);     // Use PDN/UART pin for communication
   driver.I_scale_analog(false); // Use internal voltage reference
   driver.mstep_reg_select(1);  // necessary for TMC2208 to set microstep register with UART
   driver.toff(5);                 // Enables driver in software
   driver.rms_current(800);        // Set motor RMS current
   driver.microsteps(2);          // Set microsteps to 1/16th
   driver.en_spreadCycle(false);   // Toggle spr
   driver.VACTUAL(0); // make sure velocity is set to 0
   driver.pwm_autoscale(true);     // Needed for stealthChop

   // Stepper Library Setup
   stepper.setMaxSpeed(1600); // 100mm/s @ 80 steps/mm
   stepper.setAcceleration(240); // 2000mm/s^2
   stepper.setEnablePin(EN_PIN);
   stepper.setPinsInverted(false, false, true);
   stepper.enableOutputs();

   // while(SerialPort.available()) {
   //  ESP_LOGI(TAG, "Serial read: %c", char(SerialPort.read()));
   //  vTaskDelay(100);
   // }

   uint8_t result_1 = driver.test_connection();
   ESP_LOGI(TAG, "Driver: %i", result_1);

   uint32_t thr = 80; // 70-120 is optimal
   driver.TPWMTHRS(thr);
}

void stepper_task(void *args) {
    ESP_LOGI(TAG, "Init Stepper Queue");
    // Setup the data structure to store and retrieve stepper commands
    xQueue_stepper_command = xQueueCreate(10, sizeof(long));
    if (xQueue_stepper_command == NULL) ESP_LOGE(TAG, "Unable to create stepper command queue");

    long stepper_command = 0; // storage for incoming stepper command

    ESP_LOGI(TAG, "Start Stepper Task");
    while(1) {
        if (xQueueReceive(xQueue_stepper_command, &stepper_command, portMAX_DELAY)) {
            // Set distance to move from comand variable
            stepper.move(stepper_command);
            // Run the stepper loop until we get to our destination
            while(stepper.distanceToGo() != 0) {
                stepper.run();
            }
        }
    }

}