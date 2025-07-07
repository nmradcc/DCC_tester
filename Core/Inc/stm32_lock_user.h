/**
  ******************************************************************************
  * @file      stm32_lock_user.h
  * @author    STMicroelectronics
  * @brief     User defined lock mechanisms
  *
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */

#ifndef __STM32_LOCK_USER_H__
#define __STM32_LOCK_USER_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#if STM32_LOCK_API != 1 /* Version of the implemented API */
#error stm32_lock_user.h not compatible with current version of stm32_lock.h
#endif

#include "tx_api.h"

/* Global mutex for protecting Newlib operations */
extern TX_MUTEX newlib_mutex;

/* Private defines -----------------------------------------------------------*/
/** Initialize members in instance of <code>LockingData_t</code> structure */
#define LOCKING_DATA_INIT { .mutex = &newlib_mutex }

/* Private typedef -----------------------------------------------------------*/
typedef struct
{
  TX_MUTEX *mutex;
} LockingData_t;

/* Private functions ---------------------------------------------------------*/

/**
  * @brief Initialize STM32 lock
  * @param lock The lock to init
  */
static inline void stm32_lock_init(LockingData_t *lock)
{
  static bool initialized = false;
  if (!initialized)
  {
    tx_mutex_create(&newlib_mutex, "Newlib Mutex", TX_NO_INHERIT);
    initialized = true;
  }
  lock->mutex = &newlib_mutex;
}

/**
  * @brief Acquire STM32 lock
  * @param lock The lock to acquire
  */
static inline void stm32_lock_acquire(LockingData_t *lock)
{
  tx_mutex_get(lock->mutex, TX_WAIT_FOREVER);
}

/**
  * @brief Release STM32 lock
  * @param lock The lock to release
  */
static inline void stm32_lock_release(LockingData_t *lock)
{
  tx_mutex_put(lock->mutex);
}

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __STM32_LOCK_USER_H__ */
