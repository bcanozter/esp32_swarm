#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "nvs_flash.h"
#include <string.h>
#include <sys/param.h>

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "mavlink/v2.0/common/mavlink.h"
#include <lwip/netdb.h>
#include "mavlink.h"

#define HOST_IP_ADDR CONFIG_MAVLINK_IP_ADDR
#define PORT CONFIG_MAVLINK_PORT

static const char *TAG = "mavlink";

static void handle_mavlink_message(mavlink_message_t msg) {
  switch (msg.msgid) {
  case MAVLINK_MSG_ID_HEARTBEAT: {
    mavlink_heartbeat_t hb;
    mavlink_msg_heartbeat_decode(&msg, &hb);
    ESP_LOGI(TAG, "HEARTBEAT: type=%d mode=%d", hb.type, hb.custom_mode);
    break;
  }

  case MAVLINK_MSG_ID_SYS_STATUS: {
    mavlink_sys_status_t sys;
    mavlink_msg_sys_status_decode(&msg, &sys);
    ESP_LOGI(TAG, "SYS_STATUS: battery=%d%%", sys.battery_remaining);
    break;
  }

  case MAVLINK_MSG_ID_GLOBAL_POSITION_INT: {
    mavlink_global_position_int_t pos;
    mavlink_msg_global_position_int_decode(&msg, &pos);
    ESP_LOGI(TAG, "POS: lat=%ld lon=%ld alt=%ld", pos.lat, pos.lon, pos.alt);
    break;
  }

  default:
    break;
  }
}

static void udp_client_task(void *pvParameters) {
  char rx_buffer[1024];
  char host_ip[] = HOST_IP_ADDR;
  int addr_family = 0;
  int ip_protocol = 0;

  struct sockaddr_in listen_addr;
  // listen_addr.sin_addr.s_addr = inet_addr(HOST_IP_ADDR);
  listen_addr.sin_family = AF_INET;
  listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  listen_addr.sin_port = htons(PORT);
  addr_family = AF_INET;
  ip_protocol = IPPROTO_IP;

  int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
  if (sock < 0) {
    ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
    vTaskDelete(NULL);
    return;
  }

  // Set timeout
  struct timeval timeout;
  timeout.tv_sec = 10;
  timeout.tv_usec = 0;
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);
  int err = bind(sock, (struct sockaddr *)&listen_addr, sizeof(listen_addr));
  if (err < 0) {
    ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
    close(sock);
    return;
  }
  while (1) {
    struct sockaddr_storage source_addr;
    socklen_t socklen = sizeof(source_addr);
    int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0,
                       (struct sockaddr *)&source_addr, &socklen);

    // Error occurred during receiving
    if (len < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        vTaskDelay(10 / portTICK_PERIOD_MS);
        continue;
      }
      ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
      // break;
    }
    // Data received
    else {
      char addr_str[64];
      inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str,
                  sizeof(addr_str));

      ESP_LOGD(TAG, "Received %d bytes from %s", len, addr_str);
      static mavlink_message_t msg;
      static mavlink_status_t status;
      for (int i = 0; i < len; i++) {
        if (mavlink_parse_char(MAVLINK_COMM_0, rx_buffer[i], &msg, &status)) {
          handle_mavlink_message(msg);
        }
      }
    }

    vTaskDelay(100 / portTICK_PERIOD_MS);
  }

  // if (sock != -1) {
  //   ESP_LOGE(TAG, "Shutting down socket and restarting...");
  //   shutdown(sock, 0);
  //   close(sock);
  // }
}

void init_mavlink_client(void) {
  xTaskCreate(udp_client_task, "udp_mavlink_client", 4096, NULL, 5, NULL);
}