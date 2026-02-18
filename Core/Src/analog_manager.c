/**
 * @file analog_manager.c
 * @brief Analog Manager - On-demand ADC reading and averaging implementation
 * @author Auto-generated
 * @date 2025-12-08
 * 
 * This module manages on-demand ADC readings with averaging for all allocated channels.
 * Supports ADC1 (channels 2,3,5,6) and ADC2 (channels 2,6).
 * All readings are performed on-demand with mutex protection for thread safety.
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

/* Private variables */
static osMutexId_t adc_mutex = NULL;

/* Private function prototypes */
static uint16_t read_adc_channel(ADC_HandleTypeDef *hadc, uint32_t channel);
static uint16_t average_adc_readings(ADC_HandleTypeDef *hadc, uint32_t channel, uint8_t samples);

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
    // Create mutex for ADC protection
    if (adc_mutex == NULL)
    {
        adc_mutex = osMutexNew(NULL);
        if (adc_mutex == NULL)
        {
            printf("Failed to create ADC mutex\n");
            return -1;
        }
    }
    
    // Calibrate ADCs
    if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED) != HAL_OK)
    {
        printf("ADC1 calibration failed\n");
    }
    
    if (HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED) != HAL_OK)
    {
        printf("ADC2 calibration failed\n");
    }
    
    printf("Analog Manager initialized (on-demand mode)\n");
    return 0;
}

/**
 * @brief Get the current averaged value for a specific ADC channel (on-demand)
 * @param adc_num ADC number (1 or 2)
 * @param channel Channel number (2, 3, 5, or 6)
 * @param value Pointer to store the averaged value
 * @return 0 on success, -1 on invalid parameters or mutex timeout
 */
int analog_manager_get_value(uint8_t adc_num, uint8_t channel, uint16_t *value)
{
    if (value == NULL)
    {
        return -1;
    }
    
    ADC_HandleTypeDef *hadc = NULL;
    uint32_t adc_channel = 0;
    
    // Validate and map ADC/channel combination
    if (adc_num == 1)
    {
        hadc = &hadc1;
        switch (channel)
        {
            case 2:
                adc_channel = ADC_CHANNEL_2;
                break;
            case 3:
                adc_channel = ADC_CHANNEL_3;
                break;
            case 5:
                adc_channel = ADC_CHANNEL_5;
                break;
            case 6:
                adc_channel = ADC_CHANNEL_6;
                break;
            default:
                return -1;
        }
    }
    else if (adc_num == 2)
    {
        hadc = &hadc2;
        switch (channel)
        {
            case 2:
                adc_channel = ADC_CHANNEL_2;
                break;
            case 6:
                adc_channel = ADC_CHANNEL_6;
                break;
            default:
                return -1;
        }
    }
    else
    {
        return -1;
    }
    
    // Perform on-demand ADC reading with mutex protection
    if (adc_mutex != NULL && osMutexAcquire(adc_mutex, 100) == osOK)
    {
        *value = average_adc_readings(hadc, adc_channel, ADC_AVG_SAMPLES);
        osMutexRelease(adc_mutex);
        return 0;
    }
    
    return -1;
}


int get_voltage_feedback_mv(uint16_t *voltage_mv)
{
    if (voltage_mv == NULL) {
        return -1;
    }

    uint16_t adc_value = 0;
    if (analog_manager_get_value(1, 6, &adc_value) != 0) {
        return -1;
    }

    // Convert ADC value to millivolts 
    *voltage_mv = (uint16_t)((float)adc_value * VOLTAGE_FEEDBACK_SCALE_FACTOR_MV);

    return 0;
}


int get_voltage_feedback_mv_averaged(uint16_t *voltage_mv, uint8_t num_samples, uint32_t sample_delay_ms)
{
    if (voltage_mv == NULL) {
        return -1;
    }

    if (num_samples == 0) {
        num_samples = 1;
    }

    uint32_t sum = 0;
    uint16_t adc_value = 0;

    // Collect samples with delay
    for (uint8_t i = 0; i < num_samples; i++) {
        if (analog_manager_get_value(1, 6, &adc_value) != 0) {
            return -1;
        }
        sum += adc_value;

        // Delay between samples (except after the last sample)
        if (i < num_samples - 1 && sample_delay_ms > 0) {
            osDelay(sample_delay_ms);
        }
    }

    // Calculate average
    uint16_t avg_adc_value = (uint16_t)(sum / num_samples);

    // Convert averaged ADC value to millivolts
    *voltage_mv = (uint16_t)((float)avg_adc_value * VOLTAGE_FEEDBACK_SCALE_FACTOR_MV);

    return 0;
}


int get_current_feedback_ma(uint16_t *current_ma)
{
    if (current_ma == NULL) {
        return -1;
    }

    uint16_t adc_value = 0;
    if (analog_manager_get_value(2, 2, &adc_value) != 0) {
        return -1;
    }

    // Convert ADC value to milliamps  (0.5ma per ADC count) 
    *current_ma = (uint16_t)(adc_value / CURRENT_FEEDBACK_SCALE_FACTOR_MA);

    return 0;
}


int get_current_feedback_ma_averaged(uint16_t *current_ma, uint8_t num_samples, uint32_t sample_delay_ms)
{
    if (current_ma == NULL) {
        return -1;
    }

    if (num_samples == 0) {
        num_samples = 1;
    }

    uint32_t sum = 0;
    uint16_t adc_value = 0;

    // Collect samples with delay
    for (uint8_t i = 0; i < num_samples; i++) {
        if (analog_manager_get_value(2, 2, &adc_value) != 0) {
            return -1;
        }
        sum += adc_value;

        // Delay between samples (except after the last sample)
        if (i < num_samples - 1 && sample_delay_ms > 0) {
            osDelay(sample_delay_ms);
        }
    }

    // Calculate average
    uint16_t avg_adc_value = (uint16_t)(sum / num_samples);

    // Convert averaged ADC value to milliamps (0.5ma per ADC count)
    *current_ma = (uint16_t)(avg_adc_value / CURRENT_FEEDBACK_SCALE_FACTOR_MA);

    return 0;
}
