/**
 * @file parameter_manager.c
 * @brief Parameter Manager system implementation and examples
 * @author Auto-generated
 * @date 2025-12-01
 * 
 * This file provides the Parameter Manager implementation and example usage.
 * All functions use pure C syntax.
 */

#include "parameter_manager.h"
#include "stm32h5xx_hal.h"
#include "tx_api.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* ============================================================================
 * CORE PARAMETER MANAGER IMPLEMENTATION
 * ============================================================================ */

// Default parameter values
static const uint16_t DEFAULT_DCC_TRACK_VOLTAGE = 15000;  // 15V in mV
static const uint16_t DEFAULT_DCC_TRACK_CURRENT_LIMIT = 3000;  // 3A in mA
static const uint8_t DEFAULT_DCC_PREAMBLE_BITS = 14;
static const uint16_t DEFAULT_DCC_SHORT_CIRCUIT_THRESHOLD = 5000;  // 5A in mA

static const uint32_t DEFAULT_NETWORK_IP_ADDRESS = 0xC0A80164;  // 192.168.1.100
static const uint32_t DEFAULT_NETWORK_SUBNET_MASK = 0xFFFFFF00;  // 255.255.255.0
static const uint32_t DEFAULT_NETWORK_GATEWAY = 0xC0A80101;  // 192.168.1.1
static const uint16_t DEFAULT_NETWORK_PORT = 2560;

static const uint32_t DEFAULT_SYSTEM_DEVICE_ID = 1;
static const uint32_t DEFAULT_SYSTEM_BAUD_RATE = 115200;
static const uint8_t DEFAULT_SYSTEM_DEBUG_LEVEL = 2;

// Flash storage configuration
#define FLASH_BASE_ADDRESS      0x08000000
#define FLASH_TOTAL_SIZE        (2048 * 1024)  // 2MB
#define FLASH_SECTOR_SIZE       (8 * 1024)     // 8KB sectors
#define PARAM_FLASH_SECTOR      255            // Last sector
#define PARAM_FLASH_ADDRESS     (FLASH_BASE_ADDRESS + (PARAM_FLASH_SECTOR * FLASH_SECTOR_SIZE))

#define MAGIC_NUMBER            0x50415241  // 'PARA'
#define VERSION                 1
#define PARAM_DATA_SIZE         4096

// Flash storage structure
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t crc32;
    uint32_t dataSize;
    uint8_t data[PARAM_DATA_SIZE];
} FlashStorage_t;

// Runtime parameter storage
static uint8_t g_paramData[PARAM_DATA_SIZE];
static int g_initialized = 0;
static int g_modified = 0;
static TX_MUTEX g_paramMutex;

// CRC32 lookup table
static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
    0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988, 0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
    0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
    0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172, 0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
    0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
    0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924, 0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
    0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
    0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E, 0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
    0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
    0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0, 0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
    0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
    0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A, 0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
    0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
    0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC, 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
    0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
    0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236, 0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
    0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
    0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38, 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
    0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
    0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2, 0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
    0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
    0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94, 0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

/**
 * @brief Calculate CRC32 checksum
 */
static uint32_t calculate_crc32(const void* data, size_t length) {
    const uint8_t* bytes = (const uint8_t*)data;
    uint32_t crc = 0xFFFFFFFF;
    
    for (size_t i = 0; i < length; i++) {
        uint8_t index = (crc ^ bytes[i]) & 0xFF;
        crc = (crc >> 8) ^ crc32_table[index];
    }
    
    return ~crc;
}

/**
 * @brief Load default parameter values
 */
static void load_defaults(void) {
    size_t offset = 0;
    
    // DCC parameters
    memcpy(&g_paramData[offset], &DEFAULT_DCC_TRACK_VOLTAGE, sizeof(uint16_t));
    offset += sizeof(uint16_t);
    
    memcpy(&g_paramData[offset], &DEFAULT_DCC_TRACK_CURRENT_LIMIT, sizeof(uint16_t));
    offset += sizeof(uint16_t);
    
    memcpy(&g_paramData[offset], &DEFAULT_DCC_PREAMBLE_BITS, sizeof(uint8_t));
    offset += sizeof(uint8_t);
    
    memcpy(&g_paramData[offset], &DEFAULT_DCC_SHORT_CIRCUIT_THRESHOLD, sizeof(uint16_t));
    offset += sizeof(uint16_t);
    
    // Network parameters
    memcpy(&g_paramData[offset], &DEFAULT_NETWORK_IP_ADDRESS, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    
    memcpy(&g_paramData[offset], &DEFAULT_NETWORK_SUBNET_MASK, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    
    memcpy(&g_paramData[offset], &DEFAULT_NETWORK_GATEWAY, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    
    memcpy(&g_paramData[offset], &DEFAULT_NETWORK_PORT, sizeof(uint16_t));
    offset += sizeof(uint16_t);
    
    // System parameters
    memcpy(&g_paramData[offset], &DEFAULT_SYSTEM_DEVICE_ID, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    
    memcpy(&g_paramData[offset], &DEFAULT_SYSTEM_BAUD_RATE, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    
    memcpy(&g_paramData[offset], &DEFAULT_SYSTEM_DEBUG_LEVEL, sizeof(uint8_t));
    offset += sizeof(uint8_t);
}

/**
 * @brief Initialize the parameter manager
 */
int parameter_manager_init(int force_defaults) {
    if (g_initialized) {
        return 0;  // Already initialized
    }
    
    // Create mutex
    UINT status = tx_mutex_create(&g_paramMutex, "ParamMutex", TX_NO_INHERIT);
    if (status != TX_SUCCESS) {
        return -1;
    }
    
    // Load defaults
    memset(g_paramData, 0, sizeof(g_paramData));
    load_defaults();
    
    if (!force_defaults) {
        // Try to restore from flash
        if (parameter_manager_restore() != 0) {
            // Restore failed, keep defaults
            load_defaults();
        }
    }
    
    g_initialized = 1;
    g_modified = 0;
    
    return 0;
}

/**
 * @brief Save parameters to flash
 */
int parameter_manager_save(void) {
    if (!g_initialized) {
        return -1;
    }
    
    FlashStorage_t storage;
    storage.magic = MAGIC_NUMBER;
    storage.version = VERSION;
    storage.dataSize = PARAM_DATA_SIZE;
    memcpy(storage.data, g_paramData, PARAM_DATA_SIZE);
    storage.crc32 = calculate_crc32(storage.data, storage.dataSize);
    
    // Unlock flash
    HAL_FLASH_Unlock();
    
    // Erase sector
    FLASH_EraseInitTypeDef eraseInit;
    eraseInit.TypeErase = FLASH_TYPEERASE_SECTORS;
    eraseInit.Banks = FLASH_BANK_1;
    eraseInit.Sector = PARAM_FLASH_SECTOR;
    eraseInit.NbSectors = 1;
    
    uint32_t sectorError = 0;
    HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&eraseInit, &sectorError);
    
    if (status != HAL_OK) {
        HAL_FLASH_Lock();
        return -1;
    }
    
    // Write data (STM32H5 uses 128-bit QUADWORD programming)
    uint32_t address = PARAM_FLASH_ADDRESS;
    uint32_t* data = (uint32_t*)&storage;
    size_t wordCount = (sizeof(storage) + 15) / 16;  // Round up to quadword
    
    for (size_t i = 0; i < wordCount && status == HAL_OK; i++) {
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD, address, (uint32_t)&data[i * 4]);
        address += 16;
    }
    
    HAL_FLASH_Lock();
    
    if (status == HAL_OK) {
        g_modified = 0;
        return 0;
    }
    
    return -1;
}

/**
 * @brief Restore parameters from flash
 */
int parameter_manager_restore(void) {
    if (!g_initialized) {
        return -1;
    }
    
    // Read from flash
    const FlashStorage_t* storage = (const FlashStorage_t*)PARAM_FLASH_ADDRESS;
    
    // Validate magic number
    if (storage->magic != MAGIC_NUMBER) {
        return -1;
    }
    
    // Validate version
    if (storage->version != VERSION) {
        return -1;
    }
    
    // Validate CRC
    uint32_t calculated_crc = calculate_crc32(storage->data, storage->dataSize);
    if (calculated_crc != storage->crc32) {
        return -1;
    }
    
    // Copy data
    memcpy(g_paramData, storage->data, PARAM_DATA_SIZE);
    g_modified = 0;
    
    return 0;
}

/* ============================================================================
 * EXAMPLE FUNCTIONS
 * ============================================================================ */

/**
 * @brief Initialize the parameter manager
 * @return 0 on success, -1 on failure
 */
int initialize_parameters(void) {
    // Initialize parameter manager
    // Pass 0 to restore from flash, 1 to force defaults
    int result = parameter_manager_init(0);
    
    if (result == 0) {
        printf("Parameter manager initialized successfully\n");
        return 0;
    } else {
        printf("Parameter manager initialization failed\n");
        // Try with defaults
        result = parameter_manager_init(1);
        if (result == 0) {
            printf("Initialized with default values\n");
            return 0;
        }
        return -1;
    }
}

/**
 * @brief Set DCC track voltage parameter
 * @param voltage_mv Voltage in millivolts (e.g., 15000 for 15V)
 * @return 0 on success, -1 on failure
 */
int set_dcc_track_voltage(uint16_t voltage_mv) {
    // Use the C++ interface through the C wrapper
    // You can still use C++ in a .c file by calling extern "C" functions
    
    // For pure C access, you would typically create C wrapper functions
    // like the ones below. For now, printf shows the value
    printf("Setting DCC track voltage to %u mV\n", voltage_mv);
    
    // In production, you'd call a C wrapper that internally uses C++:
    // return parameter_set_uint16(PARAM_DCC_TRACK_VOLTAGE, voltage_mv);
    
    return 0;
}

/**
 * @brief Get DCC track voltage parameter
 * @param voltage_mv Pointer to store voltage in millivolts
 * @return 0 on success, -1 on failure
 */
int get_dcc_track_voltage(uint16_t *voltage_mv) {
    if (voltage_mv == NULL) {
        return -1;
    }
    
    // In production, call C wrapper:
    // return parameter_get_uint16(PARAM_DCC_TRACK_VOLTAGE, voltage_mv);
    
    // For demonstration
    *voltage_mv = 15000;
    printf("Read DCC track voltage: %u mV\n", *voltage_mv);
    
    return 0;
}

/**
 * @brief Save all parameters to flash memory
 * @return 0 on success, -1 on failure
 */
int save_parameters_to_flash(void) {
    printf("Saving parameters to flash...\n");
    
    int result = parameter_manager_save();
    
    if (result == 0) {
        printf("Parameters saved successfully!\n");
        return 0;
    } else {
        printf("Failed to save parameters\n");
        return -1;
    }
}

/**
 * @brief Restore all parameters from flash memory
 * @return 0 on success, -1 on failure
 */
int restore_parameters_from_flash(void) {
    printf("Restoring parameters from flash...\n");
    
    int result = parameter_manager_restore();
    
    if (result == 0) {
        printf("Parameters restored successfully!\n");
        return 0;
    } else {
        printf("Failed to restore parameters (may not exist or corrupted)\n");
        return -1;
    }
}

/**
 * @brief Example: Configure DCC system parameters
 */
void configure_dcc_system(void) {
    printf("\n=== Configuring DCC System ===\n");
    
    // Set track voltage (15V)
    set_dcc_track_voltage(15000);
    
    // Set current limit (3A)
    printf("Setting current limit to 3000 mA\n");
    
    // Set preamble bits
    printf("Setting preamble bits to 14\n");
    
    // Save to flash
    save_parameters_to_flash();
    
    printf("=== DCC Configuration Complete ===");
}

/**
 * @brief Example: Read and display current configuration
 */
void display_current_configuration(void) {
    printf("\n=== Current Configuration ===\n");
    
    uint16_t voltage = 0;
    if (get_dcc_track_voltage(&voltage) == 0) {
        printf("DCC Track Voltage: %u mV (%.1f V)\n", 
               voltage, voltage / 1000.0f);
    }
    
    // Read other parameters similarly
    printf("Current Limit: 3000 mA (3.0 A)\n");
    printf("Preamble Bits: 14\n");
    
    printf("=============================\n\n");
}

/**
 * @brief Example: Factory reset - restore defaults
 */
void factory_reset(void) {
    printf("\n=== Performing Factory Reset ===\n");
    
    // Reinitialize with forced defaults
    int result = parameter_manager_init(1);
    
    if (result == 0) {
        printf("Reset to factory defaults\n");
        
        // Save defaults to flash
        parameter_manager_save();
        printf("Factory defaults saved to flash\n");
    } else {
        printf("Factory reset failed\n");
    }
    
    printf("=================================\n\n");
}

/**
 * @brief Example: Startup sequence for applications
 */
void parameter_startup_sequence(void) {
    printf("\n=== Parameter System Startup ===\n");
    
    // Step 1: Initialize
    printf("1. Initializing parameter manager...\n");
    int result = initialize_parameters();
    
    if (result != 0) {
        printf("   ERROR: Cannot initialize parameters!\n");
        return;
    }
    
    // Step 2: Display current configuration
    printf("2. Loading current configuration...\n");
    display_current_configuration();
    
    printf("=== Startup Complete ===\n\n");
}

/**
 * @brief Example: Periodic parameter check task
 * 
 * This can be called from a ThreadX task or timer callback
 */
void periodic_parameter_check(void) {
    static uint32_t check_count = 0;
    
    check_count++;
    
    // Every 10th check, verify critical parameters
    if ((check_count % 10) == 0) {
        printf("Periodic check #%lu\n", check_count);
        
        uint16_t voltage = 0;
        if (get_dcc_track_voltage(&voltage) == 0) {
            // Validate voltage is in acceptable range
            if (voltage < 10000 || voltage > 20000) {
                printf("WARNING: Voltage out of range: %u mV\n", voltage);
                // Could trigger alarm or reset to safe value
            }
        }
    }
}

/**
 * @brief Example: Update configuration from user command
 * 
 * Simulates receiving a command to change configuration
 */
void update_configuration_from_command(uint16_t new_voltage, uint16_t new_current_limit) {
    printf("\n=== Updating Configuration ===\n");
    printf("New voltage: %u mV\n", new_voltage);
    printf("New current limit: %u mA\n", new_current_limit);
    
    // Validate inputs
    if (new_voltage < 10000 || new_voltage > 20000) {
        printf("ERROR: Voltage must be between 10V and 20V\n");
        return;
    }
    
    if (new_current_limit > 5000) {
        printf("ERROR: Current limit must be <= 5A\n");
        return;
    }
    
    // Set new values
    set_dcc_track_voltage(new_voltage);
    printf("Setting current limit to %u mA\n", new_current_limit);
    
    // Save to flash so it persists across reboots
    if (save_parameters_to_flash() == 0) {
        printf("Configuration updated and saved\n");
    }
    
    printf("==============================\n\n");
}

/**
 * @brief Complete example showing typical usage flow
 */
void complete_example(void) {
    printf("\n");
    printf("================================================\n");
    printf("  Parameter Manager - Pure C Interface Example  \n");
    printf("================================================\n");
    
    // 1. Startup
    parameter_startup_sequence();
    
    // 2. Configure system
    configure_dcc_system();
    
    // 3. Display configuration
    display_current_configuration();
    
    // 4. Simulate user command to change settings
    update_configuration_from_command(16000, 3500);
    
    // 5. Display updated configuration
    display_current_configuration();
    
    printf("================================================\n");
    printf("  Example Complete                              \n");
    printf("================================================\n\n");
}

/**
 * @brief Minimal example for quick reference
 */
void minimal_example(void) {
    // Initialize
    parameter_manager_init(0);
    
    // Use parameters
    uint16_t voltage = 15000;
    printf("Setting voltage to %u mV\n", voltage);
    
    // Save
    parameter_manager_save();
    
    // Later, restore
    parameter_manager_restore();
    
    printf("Voltage: %u mV\n", voltage);
}

/**
 * @brief ThreadX task example using parameters
 * 
 * Copy this pattern for your own tasks
 */
void parameter_monitor_task(unsigned long thread_input) {
    (void)thread_input;
    
    printf("Parameter monitor task started\n");
    
    // Initialize on first run
    static int initialized = 0;
    if (!initialized) {
        parameter_manager_init(0);
        initialized = 1;
    }
    
    // Main task loop
    while (1) {
        // Check parameters
        periodic_parameter_check();
        
        // Sleep for 1 second (adjust as needed)
        // tx_thread_sleep(100);  // 100 ticks = 1 second typically
        
        // For this example, break after one iteration
        break;
    }
}
