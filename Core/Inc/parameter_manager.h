/**
 * @file parameter_manager.h
 * @brief C interface header for the Parameter Manager system
 * @author Auto-generated
 * @date 2025-12-01
 * 
 * This header provides a pure C interface to the Parameter Manager.
 * Include this file in your C source files to access the parameter system.
 */

#ifndef PARAMETER_MANAGER_H
#define PARAMETER_MANAGER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Parameter IDs for C interface
 * 
 * These correspond to the ParamId enum in the C++ interface.
 * Use these values when accessing parameters from C code.
 */
typedef enum {
    /* DCC Command Station parameters */
    PARAM_DCC_TRACK_VOLTAGE = 0,
    PARAM_DCC_TRACK_CURRENT_LIMIT,
    PARAM_DCC_PREAMBLE_BITS,
    PARAM_DCC_BIT1_DURATION,
    PARAM_DCC_BIT0_DURATION,
    PARAM_DCC_BIDI_ENABLE,
    PARAM_DCC_TRIGGER_FIRST_BIT,
    PARAM_DCC_SHORT_CIRCUIT_THRESHOLD,
    PARAM_DCC_BIDI_DAC,
    
    /* Network parameters */
    PARAM_NETWORK_IP_ADDRESS,
    PARAM_NETWORK_SUBNET_MASK,
    PARAM_NETWORK_GATEWAY,
    PARAM_NETWORK_PORT,
    
    /* System parameters */
    PARAM_SYSTEM_DEVICE_ID,
    PARAM_SYSTEM_BAUD_RATE,
    PARAM_SYSTEM_DEBUG_LEVEL,
    
    /* User-defined parameters */
    PARAM_USER_PARAM_1,
    PARAM_USER_PARAM_2,
    PARAM_USER_PARAM_3,
    
    /* Keep this last */
    PARAM_COUNT
} ParameterId;

/**
 * @brief Initialize the parameter manager
 * 
 * Call this once during system initialization.
 * 
 * @param force_defaults If non-zero, reset all parameters to defaults.
 *                       If zero, attempt to restore from flash first.
 * @return 0 on success, -1 on failure
 * 
 * @example
 * @code
 * // Initialize and restore from flash
 * if (parameter_manager_init(0) != 0) {
 *     printf("Failed to initialize\n");
 * }
 * @endcode
 */
int parameter_manager_init(int force_defaults);

/**
 * @brief Save all parameters to flash memory
 * 
 * Persists the current parameter values to non-volatile flash storage.
 * Parameters will be retained across power cycles.
 * 
 * @return 0 on success, -1 on failure
 * 
 * @example
 * @code
 * // After changing parameters, save them
 * if (parameter_manager_save() == 0) {
 *     printf("Parameters saved\n");
 * }
 * @endcode
 */
int parameter_manager_save(void);

/**
 * @brief Restore all parameters from flash memory
 * 
 * Loads previously saved parameters from flash into RAM.
 * If flash is empty or corrupted, this will fail.
 * 
 * @return 0 on success, -1 on failure (no valid data in flash)
 * 
 * @example
 * @code
 * // Restore saved parameters
 * if (parameter_manager_restore() != 0) {
 *     printf("No saved data, using defaults\n");
 *     parameter_manager_init(1);  // Force defaults
 * }
 * @endcode
 */
int parameter_manager_restore(void);

/**
 * @brief Factory reset - restore defaults
 */
void parameter_manager_factory_reset(void);

/* Individual parameter accessors */
int set_dcc_track_voltage(uint16_t voltage_mv);
int get_dcc_track_voltage(uint16_t *voltage_mv);

int set_dcc_bit1_duration(uint8_t duration_us);
int get_dcc_bit1_duration(uint8_t *duration_us);

int set_dcc_bit0_duration(uint8_t duration_us);
int get_dcc_bit0_duration(uint8_t *duration_us);

int set_dcc_bidi_enable(uint8_t enable);
int get_dcc_bidi_enable(uint8_t *enable);

int set_dcc_preamble_bits(uint8_t preamble_bits);
int get_dcc_preamble_bits(uint8_t *preamble_bits);

int set_dcc_bidi_dac(uint16_t dac_value);
int get_dcc_bidi_dac(uint16_t *dac_value);

int set_dcc_trigger_first_bit(uint8_t enable);
int get_dcc_trigger_first_bit(uint8_t *enable);

/**
 * @brief Usage Notes:
 * 
 * INITIALIZATION:
 * ---------------
 * Always call parameter_manager_init() before using any other functions.
 * Typically done once in your App_ThreadX().
 * 
 * FLASH PERSISTENCE:
 * ------------------
 * - Parameters are stored in the first flash sector
 * - Automatic wear leveling for flash longevity
 * - CRC32 validation ensures data integrity
 * - Call save() to persist changes to flash
 * - Call restore() to load from flash
 * 
 * MEMORY USAGE:
 * -------------
 * - ~2KB RAM for parameter cache
 * - 6KB flash for persistent storage
 * 
 * TYPICAL USAGE PATTERN:
 * ----------------------
 * 1. Initialize: parameter_manager_init(0)
 * 2. Read params: getParameter()
 * 3. Modify params: setParameter()
 * 4. Save to flash: parameter_manager_save()
 * 5. On next boot: parameter_manager_init(0) restores automatically
 */

#ifdef __cplusplus
}
#endif

#endif /* PARAMETER_MANAGER_H */
