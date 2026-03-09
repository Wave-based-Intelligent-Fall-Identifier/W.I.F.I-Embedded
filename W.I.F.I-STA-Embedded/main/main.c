#include <stdio.h>
#include "wifi.h"

static const char *TAG = "Main";

void app_main(void) {
    ESP_LOGI(TAG, "시스템 시작 [0/5]...");
    ESP_ERROR_CHECK(wifiInit());
    ESP_LOGI(TAG, "WiFi 초기화 [1/5]...");

    
}