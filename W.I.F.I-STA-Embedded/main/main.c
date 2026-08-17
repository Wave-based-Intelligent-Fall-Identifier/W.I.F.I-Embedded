#include <stdio.h>
#include "wifi.h"
#include "espnowAP.h"
#include "baseline_filter.h"

static const char *TAG = "Main";

void app_main(void) {
    baseline_init();

    csi_queue = xQueueCreate(10, sizeof(csi_packet_t));
    if (csi_queue == NULL) {
        ESP_LOGE(TAG, "Queue 생성 실패!");
        return;
    }

    xTaskCreatePinnedToCore(csi_data_calculate, "CSI_TASK", 4096, NULL, 5, NULL, 1);

    ESP_LOGI(TAG, "시스템 시작 [0/6]...");

    ESP_ERROR_CHECK(wifiInit());
    ESP_LOGI(TAG, "WiFi(AP) 초기화 [1/6]...");

    // AT(STA) 가 붙기 전까지 "AT 연결 대기 중..." 을 주기 출력(접속되면 자동 중단).
    xTaskCreate(wait_at_task, "WAIT_AT_TASK", 2048, NULL, 4, NULL);

    // 내장 LED 통신 상태 표시(AT 접속 + CSI 수신 시 깜빡임)
    xTaskCreate(status_led_task, "LED_TASK", 2048, NULL, 3, NULL);

    ESP_LOGI(TAG, "ESP-NOW 초기화 설정 [2/6]...");
    ESP_ERROR_CHECK(espnow_init_setup());

    // [3/6] APSTA + NAPT 게이트웨이 구성에서는 STA 자체 CSI 수신을 끈다.
    //   - 센싱은 AT 가 SoftAP 링크로 수행(AT 가 AI+MQTT 담당).
    //   - promiscuous 모드는 APSTA 데이터 포워딩(NAPT)과 간섭할 수 있어 비활성.
    ESP_LOGI(TAG, "게이트웨이 모드: promiscuous/CSI 비활성 [3/6]...");
    // ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));

    ESP_LOGI(TAG, "페어링 요청 송신 [4/6]...");
    ESP_ERROR_CHECK(espnow_send_pairing_request());

    ESP_LOGI(TAG, "콜백 등록 및 CSI 활성 [5/6]... (게이트웨이 모드: 스킵)");
    // ESP_ERROR_CHECK(esp_wifi_set_csi_rx_cb(&csi_callback, NULL));
    // ESP_ERROR_CHECK(esp_wifi_set_csi(true));

    ESP_LOGI(TAG, "설정 완료 [6/6]...");
}
