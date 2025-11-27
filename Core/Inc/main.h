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

#include "stm32h5xx_ll_ucpd.h"
#include "stm32h5xx_ll_bus.h"
#include "stm32h5xx_ll_cortex.h"
#include "stm32h5xx_ll_rcc.h"
#include "stm32h5xx_ll_system.h"
#include "stm32h5xx_ll_utils.h"
#include "stm32h5xx_ll_pwr.h"
#include "stm32h5xx_ll_gpio.h"
#include "stm32h5xx_ll_dma.h"

#include "stm32h5xx_ll_exti.h"

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
void MX_ADC1_Init(void);

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
#define SCOPE_Pin GPIO_PIN_7
#define SCOPE_GPIO_Port GPIOE
#define IN0_Pin GPIO_PIN_10
#define IN0_GPIO_Port GPIOE
#define IN1_Pin GPIO_PIN_12
#define IN1_GPIO_Port GPIOE
#define IN3_Pin GPIO_PIN_15
#define IN3_GPIO_Port GPIOE
#define SUSI_S_CLK_Pin GPIO_PIN_10
#define SUSI_S_CLK_GPIO_Port GPIOB
#define UART4_TX_BIDIR_Pin GPIO_PIN_12
#define UART4_TX_BIDIR_GPIO_Port GPIOD
#define REF_OSC_Pin GPIO_PIN_13
#define REF_OSC_GPIO_Port GPIOD
#define SD_DETECT_Pin GPIO_PIN_2
#define SD_DETECT_GPIO_Port GPIOG
#define BR_ENABLE_Pin GPIO_PIN_6
#define BR_ENABLE_GPIO_Port GPIOG
#define USART6_RX_BIDIR_Pin GPIO_PIN_6
#define USART6_RX_BIDIR_GPIO_Port GPIOC
#define MC_OUT_Pin GPIO_PIN_8
#define MC_OUT_GPIO_Port GPIOA

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

#define RPC_RX_DATA_SIZE   2048


/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
