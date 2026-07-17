#pragma once

#include <stdint.h>
#include "esp_err.h"

/**
 * @brief ESP-NOW 초기화 함수 (브로드캐스트 피어 등록)
 * @param[in] None
 * @retval esp_err_t 오류코드 반환
 */
esp_err_t espnow_init_setup(void);

/**
 * @brief 페어링 요청 브로드캐스트 송신 함수
 * @param[in] None
 * @retval esp_err_t 오류코드 반환
 */
esp_err_t espnow_send_pairing_request(void);

/**
 * @brief 특정 MAC 을 유니캐스트 피어로 등록 (CSI 유도 ping 대상)
 * @param[in] mac 6바이트 대상 MAC
 * @retval esp_err_t 오류코드 반환
 */
esp_err_t espnow_add_unicast_peer(const uint8_t *mac);

/**
 * @brief CSI 유도용 더미 패킷 유니캐스트 송신 (상대 ACK 유발)
 * @param[in] mac 6바이트 대상 MAC
 * @retval esp_err_t 오류코드 반환
 */
esp_err_t espnow_send_ping(const uint8_t *mac);
