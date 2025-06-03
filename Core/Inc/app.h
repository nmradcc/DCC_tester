/**
  ******************************************************************************
  * File Name          : app.h
  * Description        : applicative header file
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __APP_H
#define __APP_H

#ifdef __cplusplus
extern "C" {
#endif
/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include <queue.h>
#include <timers.h>
#include <semphr.h>
#include "main.h"


/* Private includes ----------------------------------------------------------*/

/* Exported types ------------------------------------------------------------*/
extern TaskHandle_t cmdLineTaskHandle;
/* Exported constants --------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Exported macro -------------------------------------------------------------*/

/* Exported function prototypes -----------------------------------------------*/

//void StartDefaultTask(void *argument);

void FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Private application code --------------------------------------------------*/

#ifdef __cplusplus
}
#endif
#endif /* __APP_H */
