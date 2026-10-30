#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "espnow.h"
#include "sdkconfig.h"
#include "state.h"

static const char *TAG = "state";
static TaskHandle_t state_main_task_handle = NULL;

static portMUX_TYPE s_state_lock = portMUX_INITIALIZER_UNLOCKED;
static state_t s_state = {.battery_pct = -1};

void state_set_heartbeat(uint8_t sysid, bool armed) {
  portENTER_CRITICAL(&s_state_lock);
  s_state.sysid = sysid;
  s_state.armed = armed;
  portEXIT_CRITICAL(&s_state_lock);
}

void state_set_battery(int8_t battery_pct) {
  portENTER_CRITICAL(&s_state_lock);
  s_state.battery_pct = battery_pct;
  portEXIT_CRITICAL(&s_state_lock);
}

void state_set_position(int32_t lat, int32_t lon, int32_t relative_alt,
                        int16_t vx, int16_t vy, int16_t vz) {
  portENTER_CRITICAL(&s_state_lock);
  s_state.lat = lat;
  s_state.lon = lon;
  s_state.relative_alt = relative_alt;
  s_state.vx = vx;
  s_state.vy = vy;
  s_state.vz = vz;
  s_state.position_valid = 1;
  portEXIT_CRITICAL(&s_state_lock);
}

static void broadcast_state(void) {
  portENTER_CRITICAL(&s_state_lock);
  state_t state = s_state;
  portEXIT_CRITICAL(&s_state_lock);

  if (state.sysid == 0) {
    return; // no fcu
  }
  esp_now_send_broadcast((const uint8_t *)&state, sizeof(state), false);
}

static void state_main_task(void *pvParameter) {

  ESP_LOGI(TAG, "Start state_main_task");

  while (1) {
    broadcast_state();
    vTaskDelay(pdMS_TO_TICKS(STATE_BROADCAST_PERIOD_MS));
  }
}

esp_err_t init_state_task(void) {
  esp_err_t ret = ESP_OK;
  xTaskCreate(state_main_task, "state_main_task", STATE_MAIN_TASK_STACK_SIZE,
              NULL, STATE_MAIN_TASK_PRIORITY, &state_main_task_handle);
  return ret;
}
