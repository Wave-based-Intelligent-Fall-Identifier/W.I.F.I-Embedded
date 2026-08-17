#include "wifi.h"
#include "baseline_filter.h"
#include <math.h>
#include "driver/gpio.h"

// 보드 내장 LED 핀(대부분 ESP32 보드 GPIO2). 보드가 다르면 이 값만 바꾸면 됨.
#define STATUS_LED_PIN 2

static const char* TAG = "WiFi";
QueueHandle_t csi_queue;

// AT(STA) 접속 여부 — wifiHandler 가 갱신, wait_at_task 가 대기 로그 출력에 사용.
volatile bool at_connected = false;
// 유효 CSI 프레임(AT 송신분) 수신 카운터 — status_led_task 가 "통신 중" 깜빡임에 사용.
volatile uint32_t g_csi_rx_count = 0;

//printf된 TX MAC ADDRESS 값 삽입 (스왑후 AT 송신보드 = COM5 의 STA MAC)
static const uint8_t TX_MAC_ADDRESS[6] = {
    0x20, 0x50, 0x0D, 0x07, 0xFC, 0xCC
};

// SoftAP netif 핸들(NAPT 활성화 대상). wifiInit 에서 저장.
static esp_netif_t *s_ap_netif = NULL;

static void wifiHandler(void *args, esp_event_base_t eventBase, int32_t eventId, void* eventData) {
    if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_AP_START) {
        ESP_LOGI(TAG, "WiFi AP모드 시작");
    }
    else if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) eventData;
        at_connected = true;
        ESP_LOGI(TAG, "장치 접속됨 MAC: " MACSTR ", AID: %d", MAC2STR(event->mac), event->aid);
    }
    else if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) eventData;
        at_connected = false;
        ESP_LOGI(TAG, "장치 연결 끊김 MAC: " MACSTR ", AID: %d", MAC2STR(event->mac), event->aid);
    }
    // ---- Station(업링크 dd) 이벤트 ----
    else if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    }
    else if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "업링크(dd) 끊김, 재접속");
        esp_wifi_connect();
    }
    else if (eventBase == IP_EVENT && eventId == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) eventData;
        ESP_LOGI(TAG, "업링크(dd) IP 획득: " IPSTR, IP2STR(&event->ip_info.ip));
        // NAPT 활성화: SoftAP 클라이언트(AT, 192.168.4.x) → dd 업링크로 라우팅.
        // → AT 가 wify_csi_ap(조밀 CSI)에 붙은 채로 브로커(172.20.10.9)에 도달 가능.
        if (s_ap_netif) {
            esp_err_t r = esp_netif_napt_enable(s_ap_netif);
            ESP_LOGI(TAG, "NAPT enable: %s (AT->dd 라우팅 %s)",
                     esp_err_to_name(r), (r == ESP_OK) ? "활성" : "실패");
        }
    }
}

esp_err_t wifiInit(void) {
    esp_err_t err;

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_ap_netif = esp_netif_create_default_wifi_ap();   // SoftAP(AT 링크)
    esp_netif_create_default_wifi_sta();               // Station(dd 업링크)

    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    if(err != ESP_OK) {
        ESP_LOGE(TAG, "WiFi 초기화 실패");
        return err;
    }

    wifi_init_config_t wifiInitConfig = WIFI_INIT_CONFIG_DEFAULT();

    err = esp_wifi_init(&wifiInitConfig);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "WiFi 기본 초기화 실패");
        return err;
    }

    
    // static bool is_paired = false;
    uint8_t mac[6];

    esp_wifi_get_mac(WIFI_IF_AP, mac);

    printf("%02X:%02X:%02X:%02X:%02X:%02X\n",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    
    esp_event_handler_instance_t instance_any_id;

    err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifiHandler, NULL, &instance_any_id);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "핸들러 등록 실패 (handler.1)");
        return err;
    }
    // Station 업링크 IP 획득 이벤트 → NAPT 활성화용
    esp_event_handler_instance_t instance_got_ip;
    err = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifiHandler, NULL, &instance_got_ip);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "핸들러 등록 실패 (IP_EVENT)");
        return err;
    }

    wifi_config_t ap_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .channel = 6,
            .password = WIFI_PASS,
            .max_connection = 2,
            .authmode = WIFI_AUTH_WPA2_PSK
        },
    };
    if (strlen(WIFI_PASS) == 0) {
        ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    wifi_config_t sta_config = {
        .sta = {
            .ssid = HOME_SSID,
            .password = HOME_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    // APSTA: SoftAP(wify_csi_ap, AT 링크) + Station(dd, 업링크) 동시 운용.
    // 채널은 Station 이 붙는 dd(ch6) 로 강제되며 SoftAP 도 같은 ch6 를 따른다.
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    ESP_ERROR_CHECK(esp_wifi_start());   // STA_START → wifiHandler 에서 esp_wifi_connect()

    ESP_LOGI(TAG, "WiFi(APSTA) 초기화 성공 — SoftAP(%s) + Station(%s)", WIFI_SSID, HOME_SSID);
    return ESP_OK;
}

void csi_callback(void *ctx, wifi_csi_info_t *data) {
    if (!data || !data->buf || csi_queue == NULL) {
        return;
    }

    // 빈/비정상 길이 패킷(amplitude 한 쌍도 못 만드는 것) 드롭 — 빈 값 방지
    if (data->len < 2) {
        return;
    }

    const uint8_t*sender_mac = data->mac;

    if (memcmp(TX_MAC_ADDRESS, sender_mac, 6) != 0) {
        return;
    }

    g_csi_rx_count++;   // AT 로부터 유효 CSI 프레임 수신 → LED "통신 중" 표시용

    csi_packet_t packet = {0};
    packet.len = data->len;
    
    if (packet.len > 128) {
        packet.len = 128;
    }

    memcpy(packet.raw_data, data->buf, packet.len);
    xQueueSend(csi_queue, &packet, 0);
}

void csi_data_calculate(void* pvParameters) {
    csi_packet_t packet;

    // CSI 값의 의미 안내(시작 시 1회 + 50줄마다 재출력)
    static const char *CSI_LEGEND =
        "[CSI 안내] 한 줄=한 시점의 채널 스냅샷 | scN:값 = N번 서브캐리어(주파수)의 진폭 sqrt(I^2+Q^2) | "
        "RAW=기준선 학습중(raw 진폭), FILT=기준선 감산(움직임 변화량) | 양끝 scN(약 -20.x)=guard/null(무신호 구간)";
    int line_cnt = 0;
    printf("%s\n", CSI_LEGEND);

    while(1) {
        if(xQueueReceive(csi_queue, &packet, portMAX_DELAY)) {

            // 유효 데이터가 없으면 빈 줄을 찍지 않고 버린다
            if (packet.len < 2) {
                continue;
            }

            // 50줄마다 안내 줄 재출력(스크롤되어도 의미를 다시 볼 수 있게)
            if (line_cnt % 50 == 0) {
                printf("%s\n", CSI_LEGEND);
            }
            line_cnt++;

            // 줄 앞 모드 표시: RAW=기준선 학습 중(raw 진폭), FILT=기준선 감산값
            // 각 값은 sc{번호}:{진폭} 형태로 어떤 서브캐리어인지 보이게 출력
            printf("%s| ", baseline_is_ready() ? "FILT" : "RAW ");

            for(int i = 0; i+1 < packet.len; i+=2) {
                int sc = i / 2;                    // 서브캐리어 인덱스 (0,1,2,...)
                int8_t real = packet.raw_data[i];
                int8_t imaginary = packet.raw_data[i + 1];

                float amplitude = sqrt((real * real) + (imaginary * imaginary));

                if (!baseline_is_ready()) {
                    baseline_update(amplitude);
                    printf("sc%d:%.2f ", sc, amplitude);
                } else {
                    float filtered_amplitude = baseline_apply(amplitude);
                    printf("sc%d:%.2f ", sc, filtered_amplitude);
                }
            }

            printf("\n");
        }
    }
}

void wait_at_task(void* pvParameters) {
    while (1) {
        if (!at_connected) {
            ESP_LOGI(TAG, "AT 연결 대기 중...");
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

/**
 * @brief 내장 LED 로 통신 상태 표시
 *   - AT 미접속        : 꺼짐
 *   - 접속 + CSI 수신중 : 깜빡임(통신 중)
 *   - 접속됐으나 데이터X : 꺼짐
 */
void status_led_task(void* pvParameters) {
    gpio_config_t led = {
        .pin_bit_mask = (1ULL << STATUS_LED_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&led);

    uint32_t last = 0;
    int level = 0;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100));
        if (!at_connected) {
            level = 0;                      // 링크 없음 → 꺼짐
        } else if (g_csi_rx_count != last) {
            last = g_csi_rx_count;
            level ^= 1;                     // CSI 수신 중 → 깜빡임
        } else {
            level = 0;                      // 접속됐으나 데이터 없음 → 꺼짐
        }
        gpio_set_level(STATUS_LED_PIN, level);
    }
}