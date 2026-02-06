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
extern SPI_HandleTypeDef hspi2;
extern SPI_HandleTypeDef hspi5;
extern FDCAN_HandleTypeDef hfdcan1;

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);
void MX_SDMMC1_SD_Init(void);
void MX_USB_PCD_Init(void);

/* USER CODE BEGIN EFP */

uint32_t GetSector_EDATA(uint32_t Address);
uint32_t GetBank_EDATA(uint32_t Address);

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
#define IO13_Pin GPIO_PIN_0
#define IO13_GPIO_Port GPIOF
#define IO14_Pin GPIO_PIN_1
#define IO14_GPIO_Port GPIOF
#define IO15_Pin GPIO_PIN_2
#define IO15_GPIO_Port GPIOF
#define SUSI_M_CLK_Pin GPIO_PIN_7
#define SUSI_M_CLK_GPIO_Port GPIOF
#define SUSI_M_DAT_Pin GPIO_PIN_9
#define SUSI_M_DAT_GPIO_Port GPIOF
#define SUSI_S_DAT_Pin GPIO_PIN_2
#define SUSI_S_DAT_GPIO_Port GPIOC
#define TRACK_P_Pin GPIO_PIN_0
#define TRACK_P_GPIO_Port GPIOA
#define BIDIR_EN_Pin GPIO_PIN_3
#define BIDIR_EN_GPIO_Port GPIOA
#define DEADTRP_ADC1_INP3_Pin GPIO_PIN_6
#define DEADTRP_ADC1_INP3_GPIO_Port GPIOA
#define DEADTRM_ADC1_INP5_Pin GPIO_PIN_1
#define DEADTRM_ADC1_INP5_GPIO_Port GPIOB
#define IO10_Pin GPIO_PIN_2
#define IO10_GPIO_Port GPIOB
#define SCOPE_Pin GPIO_PIN_7
#define SCOPE_GPIO_Port GPIOE
#define IO7_Pin GPIO_PIN_9
#define IO7_GPIO_Port GPIOE
#define IN0_Pin GPIO_PIN_10
#define IN0_GPIO_Port GPIOE
#define IO1_Pin GPIO_PIN_11
#define IO1_GPIO_Port GPIOE
#define IN1_Pin GPIO_PIN_12
#define IN1_GPIO_Port GPIOE
#define IO2_Pin GPIO_PIN_13
#define IO2_GPIO_Port GPIOE
#define IO12_Pin GPIO_PIN_14
#define IO12_GPIO_Port GPIOE
#define IN3_Pin GPIO_PIN_15
#define IN3_GPIO_Port GPIOE
#define SUSI_S_CLK_Pin GPIO_PIN_10
#define SUSI_S_CLK_GPIO_Port GPIOB
#define IO3_Pin GPIO_PIN_12
#define IO3_GPIO_Port GPIOB
#define UART4_TX_BIDIR_Pin GPIO_PIN_12
#define UART4_TX_BIDIR_GPIO_Port GPIOD
#define REF_OSC_Pin GPIO_PIN_13
#define REF_OSC_GPIO_Port GPIOD
#define IO4_Pin GPIO_PIN_14
#define IO4_GPIO_Port GPIOD
#define IO5_Pin GPIO_PIN_15
#define IO5_GPIO_Port GPIOD
#define SD_DETECT_Pin GPIO_PIN_2
#define SD_DETECT_GPIO_Port GPIOG
#define BR_ENABLE_Pin GPIO_PIN_6
#define BR_ENABLE_GPIO_Port GPIOG
#define USART6_RX_BIDIR_Pin GPIO_PIN_6
#define USART6_RX_BIDIR_GPIO_Port GPIOC
#define MC_OUT_Pin GPIO_PIN_8
#define MC_OUT_GPIO_Port GPIOA
#define IO9_Pin GPIO_PIN_7
#define IO9_GPIO_Port GPIOD
#define IO8_Pin GPIO_PIN_9
#define IO8_GPIO_Port GPIOG
#define IO10G10_Pin GPIO_PIN_10
#define IO10G10_GPIO_Port GPIOG
#define IO11_Pin GPIO_PIN_12
#define IO11_GPIO_Port GPIOG
#define IO6_Pin GPIO_PIN_14
#define IO6_GPIO_Port GPIOG
#define HL_Pin GPIO_PIN_4
#define HL_GPIO_Port GPIOB
#define HL_EXT_Pin GPIO_PIN_5
#define HL_EXT_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* Base address of the Flash sectors */
#define ADDR_EDATA1_STRT_0     (0x0900A800U) /* Base @ of last sector of Bank1 reserved to EDATA (EDATA1_STRT = 0), 6 Kbytes    */
#define ADDR_EDATA1_STRT_1     (0x09009000U) /* Base @ of last 2 sectors of Bank1 reserved to EDATA (EDATA1_STRT = 1), 6 Kbytes */
#define ADDR_EDATA1_STRT_2     (0x09007800U) /* Base @ of last 3 sectors of Bank1 reserved to EDATA (EDATA1_STRT = 2), 6 Kbytes */
#define ADDR_EDATA1_STRT_3     (0x09006000U) /* Base @ of last 4 sectors of Bank1 reserved to EDATA (EDATA1_STRT = 3), 6 Kbytes */
#define ADDR_EDATA1_STRT_4     (0x09004800U) /* Base @ of last 5 sectors of Bank1 reserved to EDATA (EDATA1_STRT = 4), 6 Kbytes */
#define ADDR_EDATA1_STRT_5     (0x09003000U) /* Base @ of last 6 sectors of Bank1 reserved to EDATA (EDATA1_STRT = 5), 6 Kbytes */
#define ADDR_EDATA1_STRT_6     (0x09001800U) /* Base @ of last 7 sectors of Bank1 reserved to EDATA (EDATA1_STRT = 6), 6 Kbytes */
#define ADDR_EDATA1_STRT_7     (0x09000000U) /* Base @ of last 8 sectors of Bank1 reserved to EDATA (EDATA1_STRT = 7), 6 Kbytes */

/* Start @ of user Flash eData area */
#define EDATA_USER_START_ADDR   ADDR_EDATA1_STRT_7
/* End @ of user Flash eData area */
/* (FLASH_EDATA_SIZE/16) is the sector size of high-cycle area (6KB) */
#define EDATA_USER_END_ADDR     (ADDR_EDATA1_STRT_7 + (8*(FLASH_EDATA_SIZE/16)) - 1)


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

#define RPC_RX_DATA_SIZE   2048


/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
