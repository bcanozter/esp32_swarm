#ifndef __SENSOR_H__
#define __SENSOR_H__

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#define STATE_MAIN_TASK_STACK_SIZE 3 * 1024
#define STATE_MAIN_TASK_PRIORITY 5
#define STATE_BROADCAST_PERIOD_MS 1000

// This vehicle's state as read from the FCU
typedef struct {
  uint8_t sysid;
  uint8_t armed;
  uint8_t position_valid;
  int8_t battery_pct;
  int32_t lat;
  int32_t lon;
  int32_t relative_alt;
  int16_t vx;
  int16_t vy;
  int16_t vz;
} __attribute__((packed)) state_t;

esp_err_t init_state_task(void);

void state_set_heartbeat(uint8_t sysid, bool armed);
void state_set_battery(int8_t battery_pct);
void state_set_position(int32_t lat, int32_t lon, int32_t relative_alt,
                        int16_t vx, int16_t vy, int16_t vz);
#endif
