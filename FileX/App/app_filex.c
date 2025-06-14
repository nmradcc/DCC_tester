/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_filex.c
  * @author  MCD Application Team
  * @brief   FileX applicative file
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
#include "app_filex.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* Main thread stack size */
#define FX_APP_THREAD_STACK_SIZE         2048
/* Main thread priority */
#define FX_APP_THREAD_PRIO               10
/* USER CODE BEGIN PD */
#define DEFAULT_QUEUE_LENGTH             16

/* Message content*/
typedef enum {
CARD_STATUS_CHANGED             = 99,
CARD_STATUS_DISCONNECTED        = 88,
CARD_STATUS_CONNECTED           = 77
} SD_ConnectionStateTypeDef;

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#define MEDIA_CLOSED                     1UL
#define MEDIA_OPENED                     0UL
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* Main thread global data structures.  */
TX_THREAD       fx_app_thread;

/* Buffer for FileX FX_MEDIA sector cache. */
ALIGN_32BYTES (uint32_t fx_sd_media_memory[FX_STM32_SD_DEFAULT_SECTOR_SIZE / sizeof(uint32_t)]);
/* Define FileX global data structures.  */
FX_MEDIA        sdio_disk;

/* USER CODE BEGIN PV */
static UINT media_status;
/* Define FileX global data structures.  */
FX_FILE         fx_file;
/* Define ThreadX global data structures.  */
TX_QUEUE        tx_msg_queue;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/

/* Main thread entry function.  */
void fx_app_thread_entry(ULONG thread_input);

/* USER CODE BEGIN PFP */
//static UINT SD_IsDetected(uint32_t Instance);
//static VOID media_close_callback (FX_MEDIA *media_ptr);
/* Add Error_Handler prototype to avoid implicit declaration error */
void Error_Handler(void);
/* USER CODE END PFP */

/**
  * @brief  Application FileX Initialization.
  * @param memory_ptr: memory pointer
  * @retval int
*/
UINT MX_FileX_Init(VOID *memory_ptr)
{
  UINT ret = FX_SUCCESS;
  TX_BYTE_POOL *byte_pool = (TX_BYTE_POOL*)memory_ptr;
  VOID *pointer;

/* USER CODE BEGIN MX_FileX_MEM_POOL */

/* USER CODE END MX_FileX_MEM_POOL */

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/*Allocate memory for the main thread's stack*/
  ret = tx_byte_allocate(byte_pool, &pointer, FX_APP_THREAD_STACK_SIZE, TX_NO_WAIT);

/* Check FX_APP_THREAD_STACK_SIZE allocation*/
  if (ret != FX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }

/* Create the main thread.  */
  ret = tx_thread_create(&fx_app_thread, FX_APP_THREAD_NAME, fx_app_thread_entry, 0, pointer, FX_APP_THREAD_STACK_SIZE,
                         FX_APP_THREAD_PRIO, FX_APP_PREEMPTION_THRESHOLD, FX_APP_THREAD_TIME_SLICE, FX_APP_THREAD_AUTO_START);

/* Check main thread creation */
  if (ret != FX_SUCCESS)
  {
    return TX_THREAD_ERROR;
  }

/* USER CODE BEGIN MX_FileX_Init */

/* USER CODE END MX_FileX_Init */

/* Initialize FileX.  */
  fx_system_initialize();

/* USER CODE BEGIN MX_FileX_Init 1*/

/* USER CODE END MX_FileX_Init 1*/

  return ret;
}

/**
 * @brief  Main thread entry.
 * @param thread_input: ULONG user argument used by the thread entry
 * @retval none
*/
 void fx_app_thread_entry(ULONG thread_input)
 {

  UINT sd_status = FX_SUCCESS;

/* USER CODE BEGIN fx_app_thread_entry 0*/
  ULONG r_msg;
  ULONG s_msg = CARD_STATUS_CHANGED;
  ULONG last_status = CARD_STATUS_DISCONNECTED;
  ULONG bytes_read;
  CHAR read_buffer[32];
  CHAR data[] = "This is FileX working on STM32";
/* USER CODE END fx_app_thread_entry 0*/

/* Open the SD disk driver */
  sd_status =  fx_media_open(&sdio_disk, FX_SD_VOLUME_NAME, fx_stm32_sd_driver, (VOID *)FX_NULL, (VOID *) fx_sd_media_memory, sizeof(fx_sd_media_memory));

/* Check the media open sd_status */
  if (sd_status != FX_SUCCESS)
  {
     /* USER CODE BEGIN SD DRIVER get info error */
    while(1);
    /* USER CODE END SD DRIVER get info error */
  }

/* USER CODE BEGIN fx_app_thread_entry 1*/
    sd_status = fx_media_flush(&sdio_disk);

    /* Check the media flush  status.  */
    if (sd_status != FX_SUCCESS)
    {
      /* Error closing the file, call error handler.  */
      Error_Handler();
    }

    /* Open the test file.  */
    sd_status =  fx_file_open(&sdio_disk, &fx_file, "STM32.TXT", FX_OPEN_FOR_READ);

    /* Check the file open status.  */
    if (sd_status != FX_SUCCESS)
    {
      /* Error opening file, call error handler.  */
      Error_Handler();
    }

    /* Seek to the beginning of the test file.  */
    sd_status =  fx_file_seek(&fx_file, 0);

    /* Check the file seek status.  */
    if (sd_status != FX_SUCCESS)
    {
      /* Error performing file seek, call error handler.  */
      Error_Handler();
    }

    /* Read the first 28 bytes of the test file.  */
    sd_status =  fx_file_read(&fx_file, read_buffer, sizeof(data), &bytes_read);

    /* Check the file read status.  */
    if ((sd_status != FX_SUCCESS) || (bytes_read != sizeof(data)))
    {
      /* Error reading file, call error handler.  */
      Error_Handler();
    }

    /* Close the test file.  */
    sd_status =  fx_file_close(&fx_file);

    /* Check the file close status.  */
    if (sd_status != FX_SUCCESS)
    {
      /* Error closing the file, call error handler.  */
      Error_Handler();
    }

    /* Close the media.  */
    sd_status =  fx_media_close(&sdio_disk);

    /* Check the media close status.  */
    if (sd_status != FX_SUCCESS)
    {
      /* Error closing the media, call error handler.  */
      Error_Handler();
    }


/* USER CODE END fx_app_thread_entry 1*/
  }

/* USER CODE BEGIN 1 */
#if 0
/**
 * @brief  Detects if SD card is correctly plugged in the memory slot or not.
 * @param Instance  SD Instance
 * @retval Returns if SD is detected or not
 */
static UINT SD_IsDetected(uint32_t Instance)
{
  UINT ret;

  if(Instance >= 1)
  {
    ret = HAL_ERROR;
  }
  else
  {
    /* Check SD card detect pin */
    if (HAL_GPIO_ReadPin(SD_DETECT_GPIO_Port, SD_DETECT_Pin) == GPIO_PIN_SET)
    {
      ret = HAL_ERROR;
    }
    else
    {
      ret = HAL_OK;
    }
  }

  return ret;
}

/**
  * @brief  EXTI line detection callback.
  * @param  GPIO_Pin: Specifies the port pin connected to corresponding EXTI line.
  * @retval None
  */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  ULONG s_msg = CARD_STATUS_CHANGED;

  if(GPIO_Pin == SD_DETECT_Pin)
  {
    tx_queue_send(&tx_msg_queue, &s_msg, TX_NO_WAIT);
  }
}

/**
  * @brief  Media close notify callback function.
  * @param  media_ptr: Media control block pointer
  * @retval None
  */
static VOID media_close_callback(FX_MEDIA *media_ptr)
{
  media_status = MEDIA_CLOSED;
}
#endif
/* USER CODE END 1 */
