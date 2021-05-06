#ifndef _PARAMETERS_H
#define _PARAMETERS_H

#include "main.h"

#ifdef __cplusplus
  extern "C" {
#endif

extern location_t device_location;

esp_err_t command_reset();
void command_ota(void);
void nvs_init(void);
int32_t nvs_get_value(char* name);
void nvs_set_value(char* name, int32_t value);
location_t command_init_location();
esp_err_t command_set_location(location_t location);
stepper_t command_init_stepper();

void setParameter(int type, int value);
void saveParameter();

#ifdef __cplusplus
  }
#endif

#endif // _PARAMETERS_H