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
extern SD_HandleTypeDef hsd1;
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
#define DCC_TRG_Pin GPIO_PIN_5
#define DCC_TRG_GPIO_Port GPIOE
#define TRACK_P_Pin GPIO_PIN_0
#define TRACK_P_GPIO_Port GPIOA
#define TRACK_N_Pin GPIO_PIN_3
#define TRACK_N_GPIO_Port GPIOA
#define SCOPE_Pin GPIO_PIN_7
#define SCOPE_GPIO_Port GPIOE
#define SD_DETECT_Pin GPIO_PIN_2
#define SD_DETECT_GPIO_Port GPIOG
#define BR_ENABLE_Pin GPIO_PIN_6
#define BR_ENABLE_GPIO_Port GPIOG

/* USER CODE BEGIN Private defines */
/* set and reset TR bit positions */
#define TR_P_BS_Pos GPIO_BSRR_BS2_Pos
#define TR_P_BR_Pos GPIO_BSRR_BR2_Pos
#define TR_N_BS_Pos GPIO_BSRR_BS3_Pos
#define TR_N_BR_Pos GPIO_BSRR_BR3_Pos
/* set and reset TRACK bit positions */
#define TRACK_P_BS_Pos GPIO_BSRR_BS0_Pos
#define TRACK_P_BR_Pos GPIO_BSRR_BR0_Pos
#define TRACK_N_BS_Pos GPIO_BSRR_BS3_Pos
#define TRACK_N_BR_Pos GPIO_BSRR_BR3_Pos

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
