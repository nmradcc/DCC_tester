#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "stdio.h"   // For logging (optional)
#include "assert.h"  // Optional: use your own assert handler if needed

#if (configSUPPORT_STATIC_ALLOCATION == 1)

// Idle task memory allocation
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer,
                                    StackType_t **ppxIdleTaskStackBuffer,
                                    uint32_t *pulIdleTaskStackSize )
{
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE];

    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

#if (configUSE_TIMERS == 1)

// Timer task memory allocation
void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer,
                                     StackType_t **ppxTimerTaskStackBuffer,
                                     uint32_t *pulTimerTaskStackSize )
{
    static StaticTask_t xTimerTaskTCB;
    static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH];

    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
#endif
#endif

#if (configUSE_MALLOC_FAILED_HOOK == 1)

// Called when pvPortMalloc fails
void vApplicationMallocFailedHook(void)
{
    // Log or halt system
    printf("Malloc failed! Heap exhausted.\n");
    assert(0); // Replace with system halt or recovery
}

#endif

#if (configCHECK_FOR_STACK_OVERFLOW > 0)

// Called when a task stack overflows
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    // Log or halt system
    printf("Stack overflow in task: %s\n", pcTaskName);
    assert(0); // Replace with system halt or recovery
}

#endif

#if (configUSE_TICK_HOOK == 1)

// Optional: called every tick
void vApplicationTickHook(void)
{
    // Example: toggle debug pin, trace, or monitor
    // GPIO_Toggle(DEBUG_PIN);
}

#endif
