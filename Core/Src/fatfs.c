/**
  ******************************************************************************
  * @file   fatfs.c
  * @brief  Code for fatfs applications
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
#include <stdio.h>
#include "fatfs.h"

void Error_Handler(void);

uint8_t retSD;    /* Return value for SD */
char SDPath[4];   /* SD logical drive path */
FATFS SDFatFS;    /* File system object for SD logical drive */
FIL SDFile;       /* File object for SD */

uint32_t file_error = 0, sd_detection_error = 0; 

void FATFS_Init(void)
{
  /*## FatFS: Link the SD driver ###########################*/
  retSD = FATFS_LinkDriver(&SD_Driver, SDPath);

  if (retSD == 0)
  {
    /*## FatFS: Register the file system object to the FatFs module ############*/
    if ((retSD = f_mount(&SDFatFS, (TCHAR const*)SDPath, 0)) != FR_OK)
    {
      /* FatFs mount Error */
      printf("FatFs Driver f_mount failed with error code: %d\r\n", retSD);

    }
    else
    {
      /* FatFs Initialized successfully */
      if((retSD = f_open(&SDFile, "test.txt", FA_READ)) == FR_OK)
      {
        printf("FatFs Driver f_open worked!!\r\n");
        f_close(&SDFile);  
      }
      else /* Can't Open JPG file*/
      {
        printf("FatFs Driver f_open failed with error code: %d\r\n", retSD);
      }       
    }
  }
  else
  {
    /* Driver linking failed */
    printf("FatFs Driver linking failed with error code: %d\r\n", retSD);
  } 
}


/**
  * @brief  Gets Time from RTC
  * @param  None
  * @retval Time in DWORD
  */
DWORD get_fattime(void)
{
  return 0;
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
