#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "nvs_flash.h"
#include <string.h>
#include <sys/param.h>

#include "driver/uart.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "mavlink/v2.0/common/mavlink.h"
#include <lwip/netdb.h>
#include "mavlink.h"
#include "state.h"

// UART
#define UART_PORT UART_NUM_1
#define UART_TX_PIN 17
#define UART_RX_PIN 18
#define UART_BUF_SIZE 2048

// UDP
#define HOST_IP_ADDR CONFIG_MAVLINK_IP_ADDR
#define PORT CONFIG_MAVLINK_PORT

static const char *TAG = "mavlink";

static void handle_mavlink_message(mavlink_message_t msg) {
  if (msg.compid != MAV_COMP_ID_AUTOPILOT1) {
    return;
  }

  switch (msg.msgid) {
  case MAVLINK_MSG_ID_HEARTBEAT: {
    mavlink_heartbeat_t hb;
    mavlink_msg_heartbeat_decode(&msg, &hb);
    ESP_LOGI(TAG, "HEARTBEAT: type=%d mode=%d", hb.type, hb.custom_mode);
    if (hb.autopilot != MAV_AUTOPILOT_INVALID) {
      state_set_heartbeat(msg.sysid,
                          (hb.base_mode & MAV_MODE_FLAG_SAFETY_ARMED) != 0);
    }
    break;
  }

  case MAVLINK_MSG_ID_SYS_STATUS: {
    mavlink_sys_status_t sys;
    mavlink_msg_sys_status_decode(&msg, &sys);
    ESP_LOGI(TAG, "SYS_STATUS: battery=%d%%", sys.battery_remaining);
    state_set_battery(sys.battery_remaining);
    break;
  }

  case MAVLINK_MSG_ID_GLOBAL_POSITION_INT: {
    mavlink_global_position_int_t pos;
    mavlink_msg_global_position_int_decode(&msg, &pos);
    ESP_LOGI(TAG, "POS: lat=%ld lon=%ld alt=%ld", pos.lat, pos.lon, pos.alt);
    state_set_position(pos.lat, pos.lon, pos.relative_alt, pos.vx, pos.vy,
                       pos.vz);
    break;
  }

  default:
    break;
  }
}

#if CONFIG_MAVLINK_UDP
static void udp_mavlink_task(void *pvParameters) {
  struct sockaddr_in dest_addr = {
      .sin_family = AF_INET,
      .sin_port = htons(PORT),
  };
  if (inet_pton(AF_INET, HOST_IP_ADDR, &dest_addr.sin_addr) != 1) {
    ESP_LOGE(TAG, "Invalid CONFIG_MAVLINK_IP_ADDR: %s", HOST_IP_ADDR);
    vTaskDelete(NULL);
    return;
  }

  int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
  if (sock < 0) {
    ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
    vTaskDelete(NULL);
    return;
  }
  struct timeval timeout = {.tv_sec = 0, .tv_usec = 100 * 1000};
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

  ESP_LOGI(TAG, "UDP link to %s:%d", HOST_IP_ADDR, PORT);

  uint8_t rx_buffer[1024];
  uint8_t tx_buffer[MAVLINK_MAX_PACKET_LEN];
  mavlink_message_t msg;
  mavlink_status_t status;
  int64_t next_heartbeat_us = 0;

  while (1) {
    int64_t now_us = esp_timer_get_time();
    if (now_us >= next_heartbeat_us) {
      
      next_heartbeat_us = now_us + 1000000;

      //TODO: 254 for now 
      mavlink_msg_heartbeat_pack(254, MAV_COMP_ID_ONBOARD_COMPUTER, &msg,
                                 MAV_TYPE_ONBOARD_CONTROLLER,
                                 MAV_AUTOPILOT_INVALID, 0, 0, MAV_STATE_ACTIVE);
      uint16_t tx_len = mavlink_msg_to_send_buffer(tx_buffer, &msg);
      if (sendto(sock, tx_buffer, tx_len, 0, (struct sockaddr *)&dest_addr,
                 sizeof(dest_addr)) < 0) {
        ESP_LOGW(TAG, "Heartbeat sendto failed: errno %d (no IP yet?)", errno);
      }
    }

    int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer), 0, NULL, NULL);
    for (int i = 0; i < len; i++) {
      if (mavlink_parse_char(MAVLINK_COMM_0, rx_buffer[i], &msg, &status)) {
        handle_mavlink_message(msg);
      }
    }
  }
}

#else
static void uart_init(void) {

  uart_config_t uart_config = {
      .baud_rate = 57600, // match firmware SERIALX_BAUD
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .source_clk = UART_SCLK_DEFAULT,
  };

  uart_driver_install(UART_PORT, UART_BUF_SIZE, 0, 0, NULL, 0);
  uart_param_config(UART_PORT, &uart_config);
  uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE,
               UART_PIN_NO_CHANGE);
}

static void uart_mavlink_task(void *pvParameters) {
  ESP_LOGI(TAG, "Start uart_mavlink_task");

  uart_init();
  uint8_t data[1024];
  mavlink_message_t msg;
  mavlink_status_t status;

  while (1) {
    int len =
        uart_read_bytes(UART_PORT, data, sizeof(data), 20 / portTICK_PERIOD_MS);
    if (len > 0) {
      for (int i = 0; i < len; i++) {
        if (mavlink_parse_char(MAVLINK_COMM_0, data[i], &msg, &status)) {
          handle_mavlink_message(msg);
        }
      }
    }
  }
}

#endif

void init_mavlink_client(void) {
#if CONFIG_MAVLINK_UDP
  xTaskCreate(udp_mavlink_task, "udp_mavlink_client", 4096, NULL, 5, NULL);
#else
  xTaskCreate(uart_mavlink_task, "uart_mavlink_client", 4096, NULL, 5, NULL);
#endif
}