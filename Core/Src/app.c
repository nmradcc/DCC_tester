/**
  ******************************************************************************
  * File Name          : app.c
  * Description        : FreeRTOS applicative file
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

/* Includes ------------------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/
#include "app.h"
#include "cli_app.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
  static StaticTask_t defaultTaskTCB;
  static StackType_t defaultTaskStack[ configMINIMAL_STACK_SIZE * 4];
  static StaticTask_t cmdLineTaskTCB;
  static StackType_t cmdLineTaskStack[ configMINIMAL_STACK_SIZE * 4];

  TaskHandle_t cmdLineTaskHandle; // handle for the command line task

#if 0
  osThreadId_t cmdLineTaskHandle; // new command line task
const osThreadAttr_t cmdLineTask_attributes = {
  .name = "cmdLineTask", // defined in cli_app.c
  .priority = (osPriority_t) osPriorityLow,
  .stack_size = 128 * 4
};
#endif
/* Private function prototypes -----------------------------------------------*/
static void defaultTask( void * parameters ) __attribute__( ( noreturn ) );


/* Private application code --------------------------------------------------*/


static void defaultTask( void * parameters )
{
    /* Unused parameters. */
    ( void ) parameters;

    for( ; ; )
    {
      /* Toggle the green LED */
      BSP_LED_Toggle(LED_GREEN);
      vTaskDelay( 500 ); /* delay 500 ticks */
    }
}


/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */

void FREERTOS_Init(void) {

  /* add mutexes, ... */
  /* add semaphores, ... */
  /* start timers, add new ones, ... */
  /* add queues, ... */
  /* add threads, ... */
  /* creation of default task */
  ( void ) xTaskCreateStatic( defaultTask,
                              "default",
                              sizeof( defaultTaskStack ) / sizeof( StackType_t ),
                              NULL,
                              configMAX_PRIORITIES - 1U,
                              &( defaultTaskStack[ 0 ] ),
                              &( defaultTaskTCB ) );

 /* creation of command console task */
  cmdLineTaskHandle = xTaskCreateStatic( vCommandConsoleTask,
                              "cmdLine",
                              sizeof( cmdLineTaskStack ) / sizeof( StackType_t ),
                              NULL,
                              configMAX_PRIORITIES - 1U,
                              &( cmdLineTaskStack[ 0 ] ),
                              &( cmdLineTaskTCB ) );

  /* add events, ... */

}

/* Private application code --------------------------------------------------*/
