#ifndef _UTILS_H
#define _UTILS_H

#include "main.h"

#ifdef __cplusplus
    extern "C" {
#endif

// ----VARIABLES----
#define HOME_BIT    0x01
#define STOP_BIT    0x02

extern QueueHandle_t xQueue_stepper_command; // This external reference has to be defined again in Strand.c

extern TaskHandle_t stepper_task_handle;


// ----FUNCTIONS----
void init_strand(int bootPosition);
void stepper_task(void *args);
void sensor_task(void *args);
void command_move(int move, int type, int speed, int accel, int time, int min, int max); 

#ifdef __cplusplus
  }
#endif

#endif // _UTILS_H