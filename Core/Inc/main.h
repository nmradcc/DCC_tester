/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h5xx_hal.h"

#include "stm32h5xx_nucleo.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim15;
extern SD_HandleTypeDef hsd1;
extern RTC_HandleTypeDef hrtc;
extern DAC_HandleTypeDef hdac1;
extern UART_HandleTypeDef huart6;
extern ETH_HandleTypeDef heth;
extern ETH_TxPacketConfigTypeDef TxConfig;
extern ETH_DMADescTypeDef  DMARxDscrTab[ETH_RX_DESC_CNT]; /* Ethernet Rx DMA Descriptors */
extern ETH_DMADescTypeDef  DMATxDscrTab[ETH_TX_DESC_CNT]; /* Ethernet Tx DMA Descriptors */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

#define DEST_IP_ADDR0   ((uint8_t)192U)
#define DEST_IP_ADDR1   ((uint8_t)168U)
#define DEST_IP_ADDR2   ((uint8_t)0U)
#define DEST_IP_ADDR3   ((uint8_t)2U)

#define DEST_PORT       ((uint16_t)7U)

/*Static IP ADDRESS: IP_ADDR0.IP_ADDR1.IP_ADDR2.IP_ADDR3 */
#define IP_ADDR0   ((uint8_t) 192U)
#define IP_ADDR1   ((uint8_t) 168U)
#define IP_ADDR2   ((uint8_t) 0U)
#define IP_ADDR3   ((uint8_t) 10U)

/*NETMASK*/
#define NETMASK_ADDR0   ((uint8_t) 255U)
#define NETMASK_ADDR1   ((uint8_t) 255U)
#define NETMASK_ADDR2   ((uint8_t) 255U)
#define NETMASK_ADDR3   ((uint8_t) 0U)

/*Gateway Address*/
#define GW_ADDR0   ((uint8_t) 192U)
#define GW_ADDR1   ((uint8_t) 168U)
#define GW_ADDR2   ((uint8_t) 0U)
#define GW_ADDR3   ((uint8_t) 1U)

#define ETH_MAC_ADDR0    ((uint8_t)0x02)
#define ETH_MAC_ADDR1    ((uint8_t)0x00)
#define ETH_MAC_ADDR2    ((uint8_t)0x00)
#define ETH_MAC_ADDR3    ((uint8_t)0x00)
#define ETH_MAC_ADDR4    ((uint8_t)0x00)
#define ETH_MAC_ADDR5    ((uint8_t)0x00)

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);
void MX_SDMMC1_SD_Init(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define TR_P_Pin GPIO_PIN_2
#define TR_P_GPIO_Port GPIOE
#define TR_N_Pin GPIO_PIN_3
#define TR_N_GPIO_Port GPIOE
#define DEC_IN_Pin GPIO_PIN_5
#define DEC_IN_GPIO_Port GPIOE
#define IN2_Pin GPIO_PIN_6
#define IN2_GPIO_Port GPIOE
#define LD2_Pin GPIO_PIN_4
#define LD2_GPIO_Port GPIOF
#define TRACK_P_Pin GPIO_PIN_0
#define TRACK_P_GPIO_Port GPIOA
#define BIDIR_EN_Pin GPIO_PIN_3
#define BIDIR_EN_GPIO_Port GPIOA
#define LD1_Pin GPIO_PIN_0
#define LD1_GPIO_Port GPIOB
#define SCOPE_Pin GPIO_PIN_7
#define SCOPE_GPIO_Port GPIOE
#define IN0_Pin GPIO_PIN_10
#define IN0_GPIO_Port GPIOE
#define IN1_Pin GPIO_PIN_12
#define IN1_GPIO_Port GPIOE
#define IN3_Pin GPIO_PIN_15
#define IN3_GPIO_Port GPIOE
#define REF_OSC_Pin GPIO_PIN_13
#define REF_OSC_GPIO_Port GPIOD
#define SD_DETECT_Pin GPIO_PIN_2
#define SD_DETECT_GPIO_Port GPIOG
#define LD3_Pin GPIO_PIN_4
#define LD3_GPIO_Port GPIOG
#define BR_ENABLE_Pin GPIO_PIN_6
#define BR_ENABLE_GPIO_Port GPIOG
#define USART6_RX_BIDIR_Pin GPIO_PIN_7
#define USART6_RX_BIDIR_GPIO_Port GPIOC
#define USB_FS_N_Pin GPIO_PIN_11
#define USB_FS_N_GPIO_Port GPIOA
#define USB_FS_P_Pin GPIO_PIN_12
#define USB_FS_P_GPIO_Port GPIOA

/* USER CODE BEGIN Private defines */
/* set and reset TR bit positions */
#define TR_P_BS_Pos GPIO_BSRR_BS2_Pos
#define TR_P_BR_Pos GPIO_BSRR_BR2_Pos
#define TR_N_BS_Pos GPIO_BSRR_BS3_Pos
#define TR_N_BR_Pos GPIO_BSRR_BR3_Pos
/* set and reset TRACK bit positions */
#define TRACK_P_BS_Pos GPIO_BSRR_BS0_Pos
#define TRACK_P_BR_Pos GPIO_BSRR_BR0_Pos

/* Default BiDi threshold */
#define DEFAULT_BIDIR_THRESHOLD 466

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
