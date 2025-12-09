/**
 * @file analog_manager.c
 * @brief Analog Manager - ADC scanning and averaging implementation
 * @author Auto-generated
 * @date 2025-12-08
 * 
 * This module manages periodic ADC scanning with averaging for all allocated channels.
 * Scans ADC1 (channels 2,3,5,6) and ADC2 (channels 2,6) approximately every 100ms.
 * Results are averaged and stored in global variables for later scaling.
 */

#include "analog_manager.h"
#include "cmsis_os2.h"
#include "main.h"
#include "stm32h5xx_hal.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* External ADC handles from main.c */
extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;

/* Global averaged ADC values (12-bit: 0-4095) */
volatile uint16_t g_adc1_ch2_avg = 0;
volatile uint16_t g_adc1_ch3_avg = 0;
volatile uint16_t g_adc1_ch5_avg = 0;
volatile uint16_t g_adc1_ch6_avg = 0;
volatile uint16_t g_adc2_ch2_avg = 0;
volatile uint16_t g_adc2_ch6_avg = 0;

/* Private variables */
static osThreadId_t analogManagerThread_id;
static bool analogManagerRunning = false;

/* Thread attributes */
const osThreadAttr_t analogManagerTask_attributes = {
    .name = "analogManagerTask",
    .stack_size = 512 * 4,
    .priority = (osPriority_t) osPriorityNormal
};

/* Private function prototypes */
static void AnalogManagerThread(void *argument);
static uint16_t read_adc_channel(ADC_HandleTypeDef *hadc, uint32_t channel);
static uint16_t average_adc_readings(ADC_HandleTypeDef *hadc, uint32_t channel, uint8_t samples);

/**
 * @brief ADC scanning thread function
 */
static void AnalogManagerThread(void *argument)
{
    (void)argument;
    
    printf("Analog Manager Task started\n");
    
    // Calibrate ADCs before use
    
    if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED) != HAL_OK)
    {
        printf("ADC1 calibration failed\n");
    }
    
    if (HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED) != HAL_OK)
    {
        printf("ADC2 calibration failed\n");
    }
    
    while (analogManagerRunning)
    {
        // Scan ADC1 channels: 2, 3, 5, 6
        g_adc1_ch2_avg = average_adc_readings(&hadc1, ADC_CHANNEL_2, ADC_AVG_SAMPLES);
        g_adc1_ch3_avg = average_adc_readings(&hadc1, ADC_CHANNEL_3, ADC_AVG_SAMPLES);
        g_adc1_ch5_avg = average_adc_readings(&hadc1, ADC_CHANNEL_5, ADC_AVG_SAMPLES);
        g_adc1_ch6_avg = average_adc_readings(&hadc1, ADC_CHANNEL_6, ADC_AVG_SAMPLES);
        
        // Scan ADC2 channels: 2, 6
        g_adc2_ch2_avg = average_adc_readings(&hadc2, ADC_CHANNEL_2, ADC_AVG_SAMPLES);
        g_adc2_ch6_avg = average_adc_readings(&hadc2, ADC_CHANNEL_6, ADC_AVG_SAMPLES);
        
        // Wait approximately 100ms before next scan
        osDelay(100);
    }
    
    printf("Analog Manager Task stopped\n");
}

/**
 * @brief Read a single ADC channel value
 * @param hadc Pointer to ADC handle
 * @param channel ADC channel to read
 * @return 12-bit ADC value (0-4095) or 0 on error
 */
static uint16_t read_adc_channel(ADC_HandleTypeDef *hadc, uint32_t channel)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    uint16_t adc_value = 0;
    
    // Configure the ADC channel
    sConfig.Channel = channel;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
    sConfig.SingleDiff = ADC_SINGLE_ENDED;
    sConfig.OffsetNumber = ADC_OFFSET_NONE;
    sConfig.Offset = 0;
    
    if (HAL_ADC_ConfigChannel(hadc, &sConfig) != HAL_OK)
    {
        return 0;
    }
    
    // Start ADC conversion
    if (HAL_ADC_Start(hadc) != HAL_OK)
    {
        return 0;
    }
    
    // Wait for conversion to complete (timeout 10ms)
    if (HAL_ADC_PollForConversion(hadc, 10) == HAL_OK)
    {
        adc_value = HAL_ADC_GetValue(hadc);
    }
    
    // Stop ADC
    HAL_ADC_Stop(hadc);
    
    return adc_value;
}

/**
 * @brief Average multiple readings from an ADC channel
 * @param hadc Pointer to ADC handle
 * @param channel ADC channel to read
 * @param samples Number of samples to average
 * @return Averaged 12-bit ADC value (0-4095)
 */
static uint16_t average_adc_readings(ADC_HandleTypeDef *hadc, uint32_t channel, uint8_t samples)
{
    uint32_t sum = 0;
    uint16_t avg = 0;
    
    if (samples == 0)
    {
        samples = 1;
    }
    
    // Collect samples
    for (uint8_t i = 0; i < samples; i++)
    {
        sum += read_adc_channel(hadc, channel);
        // Small delay between samples
        osDelay(1);
    }
    
    // Calculate average
    avg = (uint16_t)(sum / samples);
    
    return avg;
}

/**
 * @brief Initialize the Analog Manager
 * @return 0 on success, -1 on failure
 */
int analog_manager_init(void)
{
    printf("Analog Manager initialized\n");
    return 0;
}

/**
 * @brief Start the ADC scanning task
 * @return 0 on success, -1 on failure
 */
int analog_manager_start(void)
{
    if (analogManagerRunning)
    {
        printf("Analog Manager already running\n");
        return -1;
    }
    
    analogManagerRunning = true;
    
    // Create the thread
    analogManagerThread_id = osThreadNew(AnalogManagerThread, NULL, &analogManagerTask_attributes);
    
    if (analogManagerThread_id == NULL)
    {
        printf("Failed to create Analog Manager thread\n");
        analogManagerRunning = false;
        return -1;
    }
    
    printf("Analog Manager started\n");
    return 0;
}

/**
 * @brief Stop the ADC scanning task
 * @return 0 on success, -1 on failure
 */
int analog_manager_stop(void)
{
    if (!analogManagerRunning)
    {
        printf("Analog Manager not running\n");
        return -1;
    }
    
    analogManagerRunning = false;
    
    // Wait a bit for thread to finish
    osDelay(200);
    
    // Terminate the thread
    if (analogManagerThread_id != NULL)
    {
        osThreadTerminate(analogManagerThread_id);
        analogManagerThread_id = NULL;
    }
    
    printf("Analog Manager stopped\n");
    return 0;
}

/**
 * @brief Get the current averaged value for a specific ADC channel
 * @param adc_num ADC number (1 or 2)
 * @param channel Channel number (2, 3, 5, or 6)
 * @param value Pointer to store the averaged value
 * @return 0 on success, -1 on invalid parameters
 */
int analog_manager_get_value(uint8_t adc_num, uint8_t channel, uint16_t *value)
{
    if (value == NULL)
    {
        return -1;
    }
    
    if (adc_num == 1)
    {
        switch (channel)
        {
            case 2:
                *value = g_adc1_ch2_avg;
                return 0;
            case 3:
                *value = g_adc1_ch3_avg;
                return 0;
            case 5:
                *value = g_adc1_ch5_avg;
                return 0;
            case 6:
                *value = g_adc1_ch6_avg;
                return 0;
            default:
                return -1;
        }
    }
    else if (adc_num == 2)
    {
        switch (channel)
        {
            case 2:
                *value = g_adc2_ch2_avg;
                return 0;
            case 6:
                *value = g_adc2_ch6_avg;
                return 0;
            default:
                return -1;
        }
    }
    
    return -1;
}


int get_voltage_feedback_mv(uint16_t *voltage_mv)
{
    if (voltage_mv == NULL) {
        return -1;
    }

    uint16_t adc_value = 0;
    if (analog_manager_get_value(1, 6, &adc_value) != 0) {  // Assuming ADC1 Channel 6 is voltage feedback
        return -1;
    }

    // Convert ADC value to millivolts 
    *voltage_mv = (uint16_t)(adc_value * VOLTAGE_FEEDBACK_SCALE_FACTOR_MV);

    return 0;
}


int get_current_feedback_ma(uint16_t *current_ma)
{
    if (current_ma == NULL) {
        return -1;
    }

    uint16_t adc_value = 0;
    if (analog_manager_get_value(2, 2, &adc_value) != 0) {  // Assuming ADC2 Channel 2 is current feedback
        return -1;
    }

    // Convert ADC value to milliamps  (0.5ma per ADC count) 
    *current_ma = (uint16_t)(adc_value / CURRENT_FEEDBACK_SCALE_FACTOR_MA);

    return 0;

}
