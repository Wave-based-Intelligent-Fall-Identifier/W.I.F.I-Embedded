#include <stdio.h>
#include "wifi.h"
#include "espnowSTA.h"

static const char *TAG = "Main";

void app_main(void) {
    csi_queue = xQueueCreate(10, sizeof(csi_packet_t));
    if (csi_queue == NULL) {
        ESP_LOGE(TAG, "Queue 생성 실패!");
        return;
    }
    xTaskCreatePinnedToCore(csi_data_calculate, "CSI_TASK", 4096, NULL, 5, NULL, 1);

    ESP_LOGI(TAG, "시스템 시작 [0/6]...");
    ESP_ERROR_CHECK(wifiInit());
    ESP_LOGI(TAG, "WiFi 초기화 [1/6]...");

    ESP_LOGI(TAG, "ESP-NOW 초기화 설정 [2/6]...");
    ESP_ERROR_CHECK(espnow_init_setup());

    ESP_LOGI(TAG, "CSI 수신 모드 시작 [3/6]...");
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
    
    ESP_LOGI(TAG, "WiFi 채널 설정 [4/6]...");
    esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE);
    
    ESP_LOGI(TAG, "콜백함수 등록 및 CSI 활성 [5/6]...");
    ESP_ERROR_CHECK(esp_wifi_set_csi_rx_cb(&csi_callback, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_csi(true));

    ESP_LOGI(TAG, "설정 완료 [6/6]...");
}