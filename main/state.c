#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "espnow.h"
#include "sdkconfig.h"
#include "state.h"
#include "time.h"

static const char *TAG = "state";
static TaskHandle_t state_main_task_handle = NULL;
state_t state;
static void broadcast_state(void) {
  packet_t *msg_buffer = (packet_t *)malloc(sizeof(packet_t));
  msg_buffer->id = state.id;
  msg_buffer->x = state.x;
  msg_buffer->y = state.y;
  msg_buffer->z = state.z;
  esp_now_send_broadcast((const uint8_t *)msg_buffer, sizeof(packet_t), true);
  free(msg_buffer);
  msg_buffer = NULL;
}

static void update_state(void) {
  state.id = 70;
  state.x = ((float)rand() / RAND_MAX) * 10.0f;
  state.y = ((float)rand() / RAND_MAX) * 10.0f;
  state.z = ((float)rand() / RAND_MAX) * 5.0f;
}

static void state_main_task(void *pvParameter) {

  ESP_LOGI(TAG, "Start state_main_task");

  while (1) {
    update_state();
    broadcast_state();
    vTaskDelay(10000 / portTICK_PERIOD_MS);
  }
}

esp_err_t init_state_task(void) {
  esp_err_t ret = ESP_OK;
  xTaskCreate(state_main_task, "state_main_task", STATE_MAIN_TASK_STACK_SIZE,
              NULL, STATE_MAIN_TASK_PRIORITY, &state_main_task_handle);
  return ret;
}