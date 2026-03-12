#include "espnowSTA.h"

// temp MAC addr
uint8_t TX_MAC_ADDRESS[6] = {0x24, 0x6F, 0x28, 0xAB, 0xCD, 0xEF}; 

static const char *TAG = "ESPNOW-RX";

void espnow_recv_cb(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len) {
    if (data_len == sizeof(espnow_payload_t)) {
        espnow_payload_t *payload = (espnow_payload_t *)data;
        
        if (payload->command == 1) {
            ESP_LOGI(TAG, "'1' 수신, 슬립 모드 해제");
        }
    }
}

esp_err_t espnow_init_setup(void) {
    esp_err_t err = esp_now_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ESP-NOW 초기화 실패");
        return err;
    }
    
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));
    ESP_LOGI(TAG, "ESP-NOW 단독 세팅 완료!");
    
    return ESP_OK;
}