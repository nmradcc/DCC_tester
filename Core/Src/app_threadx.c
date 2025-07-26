/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_threadx.c
  * @author  MCD Application Team
  * @brief   ThreadX applicative file
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

/* Includes ------------------------------------------------------------------*/
#include "app_threadx.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "cmsis_os2.h"
#include "cli_app.h"
#include "stm32h5xx_nucleo.h"
#include "tx_api.h"
#include "command_station.h"
#include "decoder.h"
#include "SUSI.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
osThreadAttr_t LED_thread_attr = {
        .name = "LED_Task",
        .priority = osPriorityNormal,
        .stack_size = 256 * 4 // 1 KB stack size        
    };

/* Definitions for cmdLineTask */
const osThreadAttr_t cmdLineTask_attributes = {
  .name = "cmdLineTask",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 512 * 4
};


/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
osThreadId_t ledThreadHandle;
osThreadId_t cmdLineTaskHandle;
osThreadId_t cmdStationTaskHandle;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
void LedThreadTask(void *argument);


/* USER CODE END PFP */

/**
  * @brief  Application ThreadX Initialization.
  * @param memory_ptr: memory pointer
  * @retval int
  */
UINT App_ThreadX_Init(VOID *memory_ptr)
{
  UINT ret = TX_SUCCESS;

  /* USER CODE BEGIN App_ThreadX_MEM_POOL */
  (void)memory_ptr;
  /* USER CODE END App_ThreadX_MEM_POOL */

  /* USER CODE BEGIN App_ThreadX_Init */

  /* Create the led line task */  
  ledThreadHandle = osThreadNew(LedThreadTask, NULL, &LED_thread_attr);  // Create thread with attributes
  /* Create the command line task */
  cmdLineTaskHandle = osThreadNew(vCommandConsoleTask, NULL, &cmdLineTask_attributes);
  /* Create the command station task ... but don't start it */
  CommandStation_Init();
  /* Create the decoder task ... but don't start it */
  Decoder_Init();
  /* Create the SUSI Master task ... but don't start it */
  SUSI_Master_Init();
  /* Create the SUSI Slave task ... but don't start it */
  SUSI_Slave_Init();

  /* USER CODE END App_ThreadX_Init */

  return ret;
}

  /**
  * @brief  Function that implements the kernel's initialization.
  * @param  None
  * @retval None
  */
void MX_ThreadX_Init(void)
{
  /* USER CODE BEGIN Before_Kernel_Start */
  /* needed for CMSIS-RTOS2 support */
  osKernelInitialize();  // Initialize the ThreadX kernel

  /* USER CODE END Before_Kernel_Start */

  tx_kernel_enter();

  /* USER CODE BEGIN Kernel_Start_Error */

  /* USER CODE END Kernel_Start_Error */
}

/* USER CODE BEGIN 1 */
void LedThreadTask(void *argument)
{
    (void)argument;
    while (1)
    {
        BSP_LED_Toggle(LED_YELLOW);
//        tx_thread_sleep(500);
        osDelay(500);  // Delay for 500ms
    }
}



/* USER CODE END 1 */
