/**
 * @file analog_manager.h
 * @brief Analog Manager - ADC scanning and averaging
 * @author Auto-generated
 * @date 2025-12-08
 * 
 * This module manages periodic ADC scanning with averaging for all allocated channels.
 * Scans ADC1 (channels 2,3,5,6) and ADC2 (channels 2,6) approximately every 100ms.
 * Results are averaged and stored in global variables for later scaling.
 */

#ifndef ANALOG_MANAGER_H
#define ANALOG_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Number of samples to average for each channel */
#define ADC_AVG_SAMPLES 4

// 656mV per count (multiplier)
#define VOLTAGE_FEEDBACK_SCALE_FACTOR_MV  (6.8f)
// 0.5ma per count (divider)
#define CURRENT_FEEDBACK_SCALE_FACTOR_MA  (2)

/**
 * @brief Initialize the Analog Manager
 * @return 0 on success, -1 on failure
 */
int analog_manager_init(void);

/**
 * @brief Start the ADC scanning task
 * @return 0 on success, -1 on failure
 */
int analog_manager_start(void);

/**
 * @brief Stop the ADC scanning task
 * @return 0 on success, -1 on failure
 */
int analog_manager_stop(void);

/**
 * @brief Get the current averaged value for a specific ADC channel
 * @param adc_num ADC number (1 or 2)
 * @param channel Channel number (2, 3, 5, or 6)
 * @param value Pointer to store the averaged value
 * @return 0 on success, -1 on invalid parameters
 */
int analog_manager_get_value(uint8_t adc_num, uint8_t channel, uint16_t *value);

int get_voltage_feedback_mv(uint16_t *voltage_mv);
int get_current_feedback_ma(uint16_t *current_ma);

#ifdef __cplusplus
}
#endif

#endif /* ANALOG_MANAGER_H */
