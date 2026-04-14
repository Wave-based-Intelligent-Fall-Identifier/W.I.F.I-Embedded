#pragma once 

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_log.h"
#include "nvs_flash.h"

typedef struct {
    uint8_t command; 
} espnow_payload_t;

/**
 * @brief espnow값 recv함수
 * @param const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len
 * @retval None
 */
void espnow_recv_cb(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len);

/**
 * @brief ESP-NOW 초기화 함수
 * @param[in] None
 * @retval[out] esp_err_t: 상태 여부
 *  1. ESP_OK: 성공
 *  2. ESP_FAIL: 실패
 */
esp_err_t espnow_init_setup(void);

/**
 * @brief CSI recv분석 함수
 * @param[in] None
 * @retval[out] None
 */
void csi_rx_cb(void *ctx, wifi_csi_info_t *info);