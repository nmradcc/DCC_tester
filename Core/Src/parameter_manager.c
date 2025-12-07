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
#include "main.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* ============================================================================
 * CORE PARAMETER MANAGER IMPLEMENTATION
 * ============================================================================ */

// Default parameter values
static const uint16_t DEFAULT_DCC_TRACK_VOLTAGE = 15000;  // 15V in mV
static const uint16_t DEFAULT_DCC_TRACK_CURRENT_LIMIT = 3000;  // 3A in mA
static const uint8_t DEFAULT_DCC_PREAMBLE_BITS = 17;  // Minimum preamble bits
static const uint16_t DEFAULT_DCC_SHORT_CIRCUIT_THRESHOLD = 5000;  // 5A in mA
static const uint8_t DEFAULT_DCC_BIT1_DURATION = 58;  // 58 microseconds (NMRA spec: 55-61us)
static const uint8_t DEFAULT_DCC_BIT0_DURATION = 100; // 100 microseconds (NMRA spec: 95-9900us)
static const uint8_t DEFAULT_DCC_BIDI_ENABLE = 0;     // BiDi disabled by default
static const uint16_t DEFAULT_DCC_BIDI_DAC = DEFAULT_BIDIR_THRESHOLD;    // BiDi DAC threshold (12-bit: 0-4095)
static const uint8_t DEFAULT_DCC_TRIGGER_FIRST_BIT = 0; // Trigger on first bit disabled by default

static const uint32_t DEFAULT_NETWORK_IP_ADDRESS = 0xC0A80164;  // 192.168.1.100
static const uint32_t DEFAULT_NETWORK_SUBNET_MASK = 0xFFFFFF00;  // 255.255.255.0
static const uint32_t DEFAULT_NETWORK_GATEWAY = 0xC0A80101;  // 192.168.1.1
static const uint16_t DEFAULT_NETWORK_PORT = 2560;

static const uint32_t DEFAULT_SYSTEM_DEVICE_ID = 1;
static const uint32_t DEFAULT_SYSTEM_BAUD_RATE = 115200;
static const uint8_t DEFAULT_SYSTEM_DEBUG_LEVEL = 2;

// Flash storage configuration
/* Start @ of user Flash eData area */
#define EDATA_USER_START_ADDR   ADDR_EDATA1_STRT_7
/* End @ of user Flash eData area */
/* (FLASH_EDATA_SIZE/16) is the sector size of high-cycle area (6KB) */
#define EDATA_USER_END_ADDR     (ADDR_EDATA1_STRT_7 + (8*(FLASH_EDATA_SIZE/16)) - 1)

#define FLASH_BASE_ADDRESS      EDATA_USER_START_ADDR
#define PARAM_SECTOR_SIZE       (FLASH_EDATA_SIZE/16)     // 6KB sectors
#define PARAM_FLASH_SECTOR      0            // first sector
#define PARAM_FLASH_ADDRESS     (FLASH_BASE_ADDRESS + (PARAM_FLASH_SECTOR * PARAM_SECTOR_SIZE))

#define MAGIC_NUMBER            0x50415241  // 'PARA'
#define VERSION                 1
#define PARAM_DATA_SIZE         512

// Parameter data structure matching the layout in g_paramData
typedef struct {
    // DCC Command Station parameters
    uint16_t dcc_track_voltage;
    uint16_t dcc_track_current_limit;
    uint8_t dcc_preamble_bits;
    uint8_t dcc_bit1_duration;  // "1" bit duration in microseconds
    uint8_t dcc_bit0_duration;  // "0" bit duration in microseconds
    uint8_t dcc_bidi_enable;    // BiDi (bidirectional) enable flag
    uint8_t dcc_trigger_first_bit; // Trigger on first bit enable flag
    uint16_t dcc_short_circuit_threshold;
    uint16_t dcc_bidi_dac;      // BiDi DAC threshold value (12-bit: 0-4095)
    
    // Network parameters
    uint32_t network_ip_address;
    uint32_t network_subnet_mask;
    uint32_t network_gateway;
    uint16_t network_port;
    uint8_t _padding2[2];  // Alignment padding
    
    // System parameters
    uint32_t system_device_id;
    uint32_t system_baud_rate;
    uint8_t system_debug_level;
    uint8_t _padding3[3];  // Alignment padding
    
    // User-defined parameters
    uint32_t user_param_1;
    uint32_t user_param_2;
    uint32_t user_param_3;
} ParameterData_t;

// Flash storage structure
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t crc32;
    uint32_t dataSize;
    uint8_t data[PARAM_DATA_SIZE];
} FlashStorage_t;

// Runtime parameter storage - use union for type-safe access
static union {
    uint8_t bytes[PARAM_DATA_SIZE];
    ParameterData_t params;
} g_paramData;

static int g_initialized = 0;
static int g_modified = 0;

// Static storage buffer to avoid stack overflow
static FlashStorage_t g_flashStorage;



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
    // Safety checks
    if (data == NULL || length == 0 || length > PARAM_DATA_SIZE) {
        return 0;
    }
    
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
    // Note: This function assumes mutex is already held by caller
    
    // Clear all data
    memset(&g_paramData, 0, sizeof(g_paramData));
    
    // Set default values using direct structure access
    g_paramData.params.dcc_track_voltage = DEFAULT_DCC_TRACK_VOLTAGE;
    g_paramData.params.dcc_track_current_limit = DEFAULT_DCC_TRACK_CURRENT_LIMIT;
    g_paramData.params.dcc_preamble_bits = DEFAULT_DCC_PREAMBLE_BITS;
    g_paramData.params.dcc_bit1_duration = DEFAULT_DCC_BIT1_DURATION;
    g_paramData.params.dcc_bit0_duration = DEFAULT_DCC_BIT0_DURATION;
    g_paramData.params.dcc_bidi_enable = DEFAULT_DCC_BIDI_ENABLE;
    g_paramData.params.dcc_trigger_first_bit = DEFAULT_DCC_TRIGGER_FIRST_BIT;
    g_paramData.params.dcc_short_circuit_threshold = DEFAULT_DCC_SHORT_CIRCUIT_THRESHOLD;
    g_paramData.params.dcc_bidi_dac = DEFAULT_DCC_BIDI_DAC;
    
    g_paramData.params.network_ip_address = DEFAULT_NETWORK_IP_ADDRESS;
    g_paramData.params.network_subnet_mask = DEFAULT_NETWORK_SUBNET_MASK;
    g_paramData.params.network_gateway = DEFAULT_NETWORK_GATEWAY;
    g_paramData.params.network_port = DEFAULT_NETWORK_PORT;
    
    g_paramData.params.system_device_id = DEFAULT_SYSTEM_DEVICE_ID;
    g_paramData.params.system_baud_rate = DEFAULT_SYSTEM_BAUD_RATE;
    g_paramData.params.system_debug_level = DEFAULT_SYSTEM_DEBUG_LEVEL;
    
    g_modified = 1;
}

/**
 * @brief Initialize the parameter manager
 */
int parameter_manager_init(int force_defaults) {
    if (g_initialized && !force_defaults) {
        return 0;  // Already initialized
    }
    
    // Load defaults (will clear data inside)
    load_defaults();
    
    g_initialized = 1;
    g_modified = 0;
    
    if (!force_defaults) {
        // Try to restore from flash
        if (parameter_manager_restore() != 0) {
            // Restore failed, keep defaults
            load_defaults();
        }
    }
    
    return 0;
}

/**
 * @brief Save parameters to flash
 */
int parameter_manager_save(void) {
    if (!g_initialized) {
        return -1;
    }
    
    // Use static storage buffer to avoid stack overflow
    g_flashStorage.magic = MAGIC_NUMBER;
    g_flashStorage.version = VERSION;
    g_flashStorage.dataSize = PARAM_DATA_SIZE;
    memcpy(g_flashStorage.data, g_paramData.bytes, PARAM_DATA_SIZE);
    g_flashStorage.crc32 = calculate_crc32(g_flashStorage.data, g_flashStorage.dataSize);
    
    // Unlock flash
    HAL_FLASH_Unlock();

    // Calculate how many sectors we need for the storage structure
    size_t storageSize = sizeof(FlashStorage_t);
    size_t sectorsNeeded = (storageSize + PARAM_SECTOR_SIZE - 1) / PARAM_SECTOR_SIZE;
    
    // Erase the required sectors
    FLASH_EraseInitTypeDef EraseInitStruct;
    EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.Banks = GetBank_EDATA(PARAM_FLASH_ADDRESS);
    EraseInitStruct.Sector = GetSector_EDATA(PARAM_FLASH_ADDRESS);
    EraseInitStruct.NbSectors = sectorsNeeded;

    uint32_t SectorError = 0;
    HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError);
    
    if (status != HAL_OK) {
        HAL_FLASH_Lock();
        return -1;
    }

    // Write the entire storage structure as halfwords (16-bit)
    uint32_t Address = PARAM_FLASH_ADDRESS;
    uint16_t* pData = (uint16_t*)&g_flashStorage;
    size_t halfwordCount = (storageSize + 1) / 2;  // Round up to halfwords
    
    for (size_t i = 0; i < halfwordCount; i++) {
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD_EDATA, Address, (uint32_t)&pData[i]);
        
        if (status != HAL_OK) {
            HAL_FLASH_Lock();
            return -1;
        }
        
        Address += 2;  // Move to next halfword
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
    
    // Validate flash address is within valid range
    if (PARAM_FLASH_ADDRESS < EDATA_USER_START_ADDR || 
        PARAM_FLASH_ADDRESS > EDATA_USER_END_ADDR) {
        return -1;
    }
    
    // Read flash data word by word to avoid alignment issues
    const uint32_t* flashAddr = (const uint32_t*)PARAM_FLASH_ADDRESS;
    uint32_t* destAddr = (uint32_t*)&g_flashStorage;
    size_t wordCount = sizeof(FlashStorage_t) / 4;
    
    // Copy data from flash word by word
    for (size_t i = 0; i < wordCount; i++) {
        destAddr[i] = flashAddr[i];
    }
    
    // Handle remaining bytes if structure size is not multiple of 4
    size_t remainingBytes = sizeof(FlashStorage_t) % 4;
    if (remainingBytes > 0) {
        uint8_t* destByte = (uint8_t*)&destAddr[wordCount];
        const uint8_t* flashByte = (const uint8_t*)&flashAddr[wordCount];
        for (size_t i = 0; i < remainingBytes; i++) {
            destByte[i] = flashByte[i];
        }
    }
    
    // Validate magic number
    if (g_flashStorage.magic != MAGIC_NUMBER) {
        return -1;
    }
    
    // Validate version
    if (g_flashStorage.version != VERSION) {
        return -1;
    }
    
    // Validate data size
    if (g_flashStorage.dataSize != PARAM_DATA_SIZE) {
        return -1;
    }
    
    // Validate CRC
    uint32_t calculated_crc = calculate_crc32(g_flashStorage.data, g_flashStorage.dataSize);
    if (calculated_crc != g_flashStorage.crc32) {
        return -1;
    }
    
    // Copy validated data to runtime parameter storage
    memcpy(g_paramData.bytes, g_flashStorage.data, PARAM_DATA_SIZE);
    g_modified = 0;
    
    return 0;
}

/**
 * @brief Factory reset - restore defaults
 */
void parameter_manager_factory_reset(void) {

    /* Variable used for OB Program procedure */
    FLASH_OBProgramInitTypeDef FLASH_OBInitStruct;

    printf("\n=== Performing Factory Reset ===\n");
    
    /* Unlock the Flash to enable the flash control register access *************/
    HAL_FLASH_Unlock();

    /* Unlock the Flash option bytes to enable the flash option control register access */
    HAL_FLASH_OB_Unlock();

    /* Erase the EDATA Flash area
    (area defined by EDATA_USER_START_ADDR and EDATA_USER_END_ADDR) ***********/

    /* Configure 8 sectors for FLASH high-cycle data */
    FLASH_OBInitStruct.OptionType = OPTIONBYTE_EDATA;
    FLASH_OBInitStruct.Banks = GetBank_EDATA(EDATA_USER_START_ADDR);
    FLASH_OBInitStruct.EDATASize = GetSector_EDATA(EDATA_USER_END_ADDR) - GetSector_EDATA(EDATA_USER_START_ADDR) + 1;
    if(HAL_FLASHEx_OBProgram(&FLASH_OBInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /* Start option byte load operation after successful programming operation */
    HAL_FLASH_OB_Launch();

    /* Lock the Flash control option to restrict register access */
    HAL_FLASH_OB_Lock();

    HAL_FLASH_Lock();


    // Reinitialize with forced defaults
    int result = parameter_manager_init(1);
    
    if (result == 0) {
        printf("Reset to factory defaults\n");
        
        // Save defaults to flash
        parameter_manager_save();
        printf("Factory defaults saved to flash\n");
    } else {
        printf("Factory reset failed\n");
        printf(" please perform reboot and try again!\n");
    }
    
    printf("=================================\n\n");
}

/* ============================================================================
 * ACCESSOR FUNCTIONS
 * ============================================================================ */

/**
 * @brief Set DCC track voltage parameter
 * @param voltage_mv Voltage in millivolts (e.g., 15000 for 15V)
 * @return 0 on success, -1 on failure
 */
int set_dcc_track_voltage(uint16_t voltage_mv) {
    if (!g_initialized) {
        return -1;
    }
    
    // Write directly to the parameter structure
    g_paramData.params.dcc_track_voltage = voltage_mv;
    
    // Mark as modified
    g_modified = 1;
    
    return 0;
}

/**
 * @brief Get DCC track voltage parameter
 * @param voltage_mv Pointer to store voltage in millivolts
 * @return 0 on success, -1 on failure
 */
int get_dcc_track_voltage(uint16_t *voltage_mv) {
    if (voltage_mv == NULL || !g_initialized) {
        return -1;
    }
    
    // Read directly from the parameter structure
    *voltage_mv = g_paramData.params.dcc_track_voltage;
    
    return 0;
}

/**
 * @brief Set DCC bit1 duration ("1" bit timing)
 * @param duration_us Duration in microseconds (NMRA spec: 55-61us, typical: 58us)
 * @return 0 on success, -1 on failure
 */
int set_dcc_bit1_duration(uint8_t duration_us) {
    if (!g_initialized) {
        return -1;
    }
    
    // Write directly to the parameter structure
    g_paramData.params.dcc_bit1_duration = duration_us;
    
    // Mark as modified
    g_modified = 1;
    
    return 0;
}

/**
 * @brief Get DCC bit1 duration ("1" bit timing)
 * @param duration_us Pointer to store duration in microseconds
 * @return 0 on success, -1 on failure
 */
int get_dcc_bit1_duration(uint8_t *duration_us) {
    if (duration_us == NULL || !g_initialized) {
        return -1;
    }
    
    // Read directly from the parameter structure
    *duration_us = g_paramData.params.dcc_bit1_duration;
    
    return 0;
}

/**
 * @brief Set DCC bit0 duration ("0" bit timing)
 * @param duration_us Duration in microseconds (NMRA spec: 95-9900us, typical: 100us)
 * @return 0 on success, -1 on failure
 */
int set_dcc_bit0_duration(uint8_t duration_us) {
    if (!g_initialized) {
        return -1;
    }
    
    // Write directly to the parameter structure
    g_paramData.params.dcc_bit0_duration = duration_us;
    
    // Mark as modified
    g_modified = 1;
    
    return 0;
}

/**
 * @brief Get DCC bit0 duration ("0" bit timing)
 * @param duration_us Pointer to store duration in microseconds
 * @return 0 on success, -1 on failure
 */
int get_dcc_bit0_duration(uint8_t *duration_us) {
    if (duration_us == NULL || !g_initialized) {
        return -1;
    }
    
    // Read directly from the parameter structure
    *duration_us = g_paramData.params.dcc_bit0_duration;
    
    return 0;
}

/**
 * @brief Set DCC BiDi (bidirectional communication) enable
 * @param enable 0 to disable, non-zero to enable BiDi
 * @return 0 on success, -1 on failure
 */
int set_dcc_bidi_enable(uint8_t enable) {
    if (!g_initialized) {
        return -1;
    }
    
    // Write directly to the parameter structure
    g_paramData.params.dcc_bidi_enable = enable ? 1 : 0;
    
    // Mark as modified
    g_modified = 1;
    
    return 0;
}

/**
 * @brief Get DCC BiDi (bidirectional communication) enable status
 * @param enable Pointer to store enable status (0=disabled, 1=enabled)
 * @return 0 on success, -1 on failure
 */
int get_dcc_bidi_enable(uint8_t *enable) {
    if (enable == NULL || !g_initialized) {
        return -1;
    }
    
    // Read directly from the parameter structure
    *enable = g_paramData.params.dcc_bidi_enable;
    
    return 0;
}

/**
 * @brief Set DCC preamble bits
 * @param preamble_bits Number of preamble bits (NMRA spec minimum: 14 for operations, 20 for service mode)
 * @return 0 on success, -1 on failure
 */
int set_dcc_preamble_bits(uint8_t preamble_bits) {
    if (!g_initialized) {
        return -1;
    }
    
    // Write directly to the parameter structure
    g_paramData.params.dcc_preamble_bits = preamble_bits;
    
    // Mark as modified
    g_modified = 1;
    
    return 0;
}

/**
 * @brief Get DCC preamble bits
 * @param preamble_bits Pointer to store number of preamble bits
 * @return 0 on success, -1 on failure
 */
int get_dcc_preamble_bits(uint8_t *preamble_bits) {
    if (preamble_bits == NULL || !g_initialized) {
        return -1;
    }
    
    // Read directly from the parameter structure
    *preamble_bits = g_paramData.params.dcc_preamble_bits;
    
    return 0;
}

/**
 * @brief Set DCC BiDi DAC threshold value
 * @param dac_value DAC threshold value (12-bit: 0-4095)
 * @return 0 on success, -1 on failure
 */
int set_dcc_bidi_dac(uint16_t dac_value) {
    if (!g_initialized) {
        return -1;
    }
    
    // Write directly to the parameter structure
    g_paramData.params.dcc_bidi_dac = dac_value;
    
    // Mark as modified
    g_modified = 1;
    
    return 0;
}

/**
 * @brief Get DCC BiDi DAC threshold value
 * @param dac_value Pointer to store DAC threshold value (12-bit: 0-4095)
 * @return 0 on success, -1 on failure
 */
int get_dcc_bidi_dac(uint16_t *dac_value) {
    if (dac_value == NULL || !g_initialized) {
        return -1;
    }
    
    // Read directly from the parameter structure
    *dac_value = g_paramData.params.dcc_bidi_dac;
    
    return 0;
}

/**
 * @brief Set DCC trigger on first bit enable
 * @param enable 0 to disable, non-zero to enable trigger on first bit
 * @return 0 on success, -1 on failure
 */
int set_dcc_trigger_first_bit(uint8_t enable) {
    if (!g_initialized) {
        return -1;
    }
    
    // Write directly to the parameter structure
    g_paramData.params.dcc_trigger_first_bit = enable ? 1 : 0;
    
    // Mark as modified
    g_modified = 1;
    
    return 0;
}

/**
 * @brief Get DCC trigger on first bit enable status
 * @param enable Pointer to store enable status (0=disabled, 1=enabled)
 * @return 0 on success, -1 on failure
 */
int get_dcc_trigger_first_bit(uint8_t *enable) {
    if (enable == NULL || !g_initialized) {
        return -1;
    }
    
    // Read directly from the parameter structure
    *enable = g_paramData.params.dcc_trigger_first_bit;
    
    return 0;
}
