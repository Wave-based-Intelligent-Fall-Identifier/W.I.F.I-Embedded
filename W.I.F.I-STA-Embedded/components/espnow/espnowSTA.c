#include "espnowSTA.h"

// temp MAC addr
extern uint8_t TX_MAC_ADDRESS[6];
static const char *TAG = "ESPNOW-RX";

void espnow_recv_cb(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len) {
    if (data_len == sizeof(espnow_payload_t)) {
        espnow_payload_t *payload = (espnow_payload_t *)data;
        
        if (payload->command == 1) {
            ESP_LOGI(TAG, "'1' 수신, 슬립 모드 해제");
        }
    }
}

void csi_rx_cb(void *ctx, wifi_csi_info_t *info) {
    if (!info || !info->buf) {
        return;
    }

    int8_t *csi_data = info->buf;
    uint16_t len = info->len;

    if (len >= 2) {
        int8_t real = csi_data[0]; 
        int8_t imag = csi_data[1]; 
        
        float amplitude = sqrt((real * real) + (imag * imag));
        printf("%.2f\n", amplitude); 
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
    
    wifi_csi_config_t csi_config = {
        .lltf_en           = true,
        .htltf_en          = true,
        .stbc_htltf2_en    = true,
        .ltf_merge_en      = true,
        .channel_filter_en = true,
        .manu_scale        = false,
        .shift             = false,
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_csi_config(&csi_config));
    ESP_ERROR_CHECK(esp_wifi_set_csi_rx_cb(csi_rx_cb, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_csi(true)); 
    ESP_LOGI(TAG, "CSI 파동 수집 세팅 완료!");
    
    return ESP_OK;
}