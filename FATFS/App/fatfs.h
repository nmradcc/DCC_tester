/**
  ******************************************************************************
  * @file   fatfs.h
  * @brief  Header for fatfs applications
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
//* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __fatfs_H
#define __fatfs_H
#ifdef __cplusplus
 extern "C" {
#endif

#include "ff.h"
#include "ff_gen_drv.h"
#include "sd_diskio_dma_rtos.h" /* defines SD_Driver as external */

void MX_FATFS_Init(void);

extern FATFS SDFatFS;   /* File system object for SD logical drive */
extern FIL SDFile;      /* File object for SD */
extern char SDPath[4];  /* SD logical drive path */


#ifdef __cplusplus
}
#endif
#endif /*__fatfs_H */
