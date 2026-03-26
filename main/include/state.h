#ifndef __SENSOR_H__
#define __SENSOR_H__

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#define STATE_MAIN_TASK_STACK_SIZE 3 * 1024
#define STATE_MAIN_TASK_PRIORITY 5

//TODO, read state from FCU.
//Dummy
typedef struct {
  uint8_t id;
  float x;
  float y;
  float z;
  float vx;
  float vy;
  float vz;
} state_t;

typedef struct {
  uint8_t id;
  float x;
  float y;
  float z;
} packet_t;

esp_err_t init_state_task(void);
#endif