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
#define RMII_MDC_Pin GPIO_PIN_1
#define RMII_MDC_GPIO_Port GPIOC
#define TRACK_P_Pin GPIO_PIN_0
#define TRACK_P_GPIO_Port GPIOA
#define RMII_REF_CLK_Pin GPIO_PIN_1
#define RMII_REF_CLK_GPIO_Port GPIOA
#define RMII_MDIO_Pin GPIO_PIN_2
#define RMII_MDIO_GPIO_Port GPIOA
#define BIDIR_EN_Pin GPIO_PIN_3
#define BIDIR_EN_GPIO_Port GPIOA
#define VBUS_SENSE_Pin GPIO_PIN_4
#define VBUS_SENSE_GPIO_Port GPIOA
#define RMII_CRS_DV_Pin GPIO_PIN_7
#define RMII_CRS_DV_GPIO_Port GPIOA
#define RMII_RXD0_Pin GPIO_PIN_4
#define RMII_RXD0_GPIO_Port GPIOC
#define RMII_RXD1_Pin GPIO_PIN_5
#define RMII_RXD1_GPIO_Port GPIOC
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
#define UCPD_CC1_Pin GPIO_PIN_13
#define UCPD_CC1_GPIO_Port GPIOB
#define UCPD_CC2_Pin GPIO_PIN_14
#define UCPD_CC2_GPIO_Port GPIOB
#define RMII_TXD1_Pin GPIO_PIN_15
#define RMII_TXD1_GPIO_Port GPIOB
#define REF_OSC_Pin GPIO_PIN_13
#define REF_OSC_GPIO_Port GPIOD
#define SD_DETECT_Pin GPIO_PIN_2
#define SD_DETECT_GPIO_Port GPIOG
#define LD3_Pin GPIO_PIN_4
#define LD3_GPIO_Port GPIOG
#define BR_ENABLE_Pin GPIO_PIN_6
#define BR_ENABLE_GPIO_Port GPIOG
#define UART6_RX_BIDIR_Pin GPIO_PIN_7
#define UART6_RX_BIDIR_GPIO_Port GPIOC
#define UCDP_DBn_Pin GPIO_PIN_9
#define UCDP_DBn_GPIO_Port GPIOA
#define USB_FS_N_Pin GPIO_PIN_11
#define USB_FS_N_GPIO_Port GPIOA
#define USB_FS_P_Pin GPIO_PIN_12
#define USB_FS_P_GPIO_Port GPIOA
#define RMII_TXT_EN_Pin GPIO_PIN_11
#define RMII_TXT_EN_GPIO_Port GPIOG
#define RMI_TXD0_Pin GPIO_PIN_13
#define RMI_TXD0_GPIO_Port GPIOG

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
