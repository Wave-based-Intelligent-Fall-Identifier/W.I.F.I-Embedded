#include "espnowAP.h"

#include <string.h>

#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_log.h"

static const char* TAG = "ESP-NOW-AP";

static const uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

/** 마지막으로 AT 로부터 수신한 명령(1=사람감지 ACT, 2=종료 DEACT). 0=아직 없음. */
volatile uint8_t g_last_at_command = 0;

/**
 * @brief ESP-NOW 수신 콜백 — AT(송신보드)가 보낸 command 페이로드 처리.
 *   command=1: 사람 감지(활성), command=2: 10분 경과 종료. 페이로드 첫 바이트가 명령값.
 */
static void espnow_recv_cb(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (!info || !data || len < 1) {
        return;
    }
    // CSI 트래픽 더미 프레임(50Hz, payload[0]=0xC5)은 무시 — 실제 PIR 명령(1/2)만 처리.
    if (data[0] != 1 && data[0] != 2) {
        return;
    }
    const uint8_t *m = info->src_addr;
    g_last_at_command = data[0];
    ESP_LOGI(TAG, "ESP-NOW 수신 from %02X:%02X:%02X:%02X:%02X:%02X, command=%u (len=%d)",
             m[0], m[1], m[2], m[3], m[4], m[5], (unsigned)data[0], len);
}

esp_err_t espnow_init_setup(void) {
    esp_err_t err = esp_now_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ESP-NOW 초기화 실패");
        return err;
    }

    err = esp_now_register_recv_cb(espnow_recv_cb);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ESP-NOW recv 콜백 등록 실패");
        return err;
    }

    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, BROADCAST_MAC, sizeof(BROADCAST_MAC));
    peer.channel = 0;          // 0 = 현재 WiFi 채널 사용
    peer.ifidx   = WIFI_IF_AP; // AP 인터페이스로 송신
    peer.encrypt = false;

    err = esp_now_add_peer(&peer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "브로드캐스트 피어 등록 실패");
        return err;
    }

    ESP_LOGI(TAG, "ESP-NOW 초기화 완료");
    return ESP_OK;
}

esp_err_t espnow_send_pairing_request(void) {
    uint8_t payload = 0; // 페어링 요청 신호
    esp_err_t err = esp_now_send(BROADCAST_MAC, &payload, sizeof(payload));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "페어링 요청 송신 실패");
    }
    return err;
}

esp_err_t espnow_add_unicast_peer(const uint8_t *mac) {
    if (mac == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (esp_now_is_peer_exist(mac)) {
        return ESP_OK;
    }

    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, mac, 6);
    peer.channel = 0;
    peer.ifidx   = WIFI_IF_AP;
    peer.encrypt = false;

    esp_err_t err = esp_now_add_peer(&peer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "유니캐스트 피어 등록 실패 (0x%x)", err);
    }
    return err;
}

esp_err_t espnow_send_ping(const uint8_t *mac) {
    static const uint8_t payload = 0xAA;
    return esp_now_send(mac, &payload, sizeof(payload));
}
