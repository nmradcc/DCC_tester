#ifndef OPENMRN_CLIENT_H
#define OPENMRN_CLIENT_H

#include <stdint.h>
#include <stdbool.h>
#include "main.h"
#include "tx_api.h"
#include "stm32h5xx_hal_fdcan.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize OpenMRN Client
 * 
 * @param hfdcan Pointer to FDCAN_HandleTypeDef (hfdcan1)
 */
void OpenMRN_Client_Init(FDCAN_HandleTypeDef *hfdcan);

/**
 * @brief Start OpenMRN Client thread
 */
void OpenMRN_Client_Start(void);

/**
 * @brief Stop OpenMRN Client thread
 */
void OpenMRN_Client_Stop(void);

/**
 * @brief Send OpenMRN CAN message
 * 
 * @param arbitration_id CAN identifier (11-bit standard ID)
 * @param data Pointer to data buffer (up to 8 bytes)
 * @param dlc Data length code (0-8)
 * @return HAL_OK if successful, HAL_ERROR if parameters invalid
 */
HAL_StatusTypeDef OpenMRN_Client_SendMessage(uint32_t arbitration_id, const uint8_t *data, uint8_t dlc);

#ifdef __cplusplus
}
#endif

#endif /* OPENMRN_CLIENT_H */
