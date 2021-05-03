
#ifndef OSC_H
#define OSC_H

#include "main.h"


#ifdef __cplusplus
    extern "C" {
#endif


void init_osc();
void osc_handler(BCAST_CMD cmd, uint8_t type);

#ifdef __cplusplus
  }
#endif


#endif // OSC_H