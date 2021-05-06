
#ifndef OSC_H
#define OSC_H

#include "main.h"


#ifdef __cplusplus
    extern "C" {
#endif

// void test_route(OscMessage * msg_in);
void init_osc();
void osc_handler(BCAST_CMD cmd, uint8_t type);

#ifdef __cplusplus
  }
#endif


#endif // OSC_H