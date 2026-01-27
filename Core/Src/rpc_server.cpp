#include "cmsis_os2.h"
#include "stm32h5xx_nucleo.h"
#include "stm32h5xx_hal.h"
#include "main.h"
#include "parameter_manager.h"
#include "command_station.h"
#include "decoder.h"
#include "parameter_manager.h"
#include "analog_manager.h"

#include "rpc_server.hpp"

#include "ux_device_cdc_acm.h"
#include "ux_device_class_cdc_acm.h"
#include <cstring>
#include <cstdio>

extern TX_QUEUE rpc_rxqueue;
extern UX_SLAVE_CLASS_CDC_ACM  *cdc_acm;

static osThreadId_t rpcServerThread_id;
static osSemaphoreId_t rpcServerStart_sem;
static bool rpcServerRunning = false;

/* Definitions for rpcServerTask */
const osThreadAttr_t rpcServerTask_attributes = {
  .name = "rpcServerTask",
  .stack_size = 8192,
  .priority = osPriorityBelowNormal4    //(osPriority_t) osPriorityHigh
};


// ---------------- RpcServer methods ----------------

bool RpcServer::register_method(const char* name, RpcHandlerFn handler) {
    if (!name || !handler) return false;
    if (count >= kMaxMethods) return false;

    for (int i = 0; i < count; ++i) {
        if (table[i].name && std::strcmp(table[i].name, name) == 0) {
            table[i].handler = handler;
            return true;
        }
    }

    table[count++] = {name, handler};
    return true;
}

RpcHandlerFn RpcServer::find(const char* name) const {
    if (!name) return nullptr;
    for (int i = 0; i < count; ++i) {
        if (table[i].name && std::strcmp(table[i].name, name) == 0) {
            return table[i].handler;
        }
    }
    return nullptr;
}

std::string RpcServer::error_response(const char* msg) {
    json resp = {
        {"status", "error"},
        {"message", msg ? msg : "error"}
    };
    return resp.dump();
}

std::string RpcServer::handle(const std::string& request_str) {
    json request;
    if (!json::accept(request_str)) {
        return error_response("Invalid JSON");
    }
    request = json::parse(request_str);

    if (!request.contains("method") || !request.contains("params")) {
        return error_response("Malformed request");
    }

    if (!request["method"].is_string()) {
        return error_response("Method must be string");
    }

    std::string method_str = request["method"].get<std::string>();
    RpcHandlerFn handler = find(method_str.c_str());
    if (!handler) {
        return error_response("Unknown method");
    }

    json response = handler(request["params"]);
    return response.dump();
}

// ---------------- Handlers ----------------

static json echo_handler(const json& params) {
    return {
        {"status", "ok"},
        {"echo", params}
    };
}

static json command_station_start_handler(const json& params) {
    uint8_t loop = 0;  // 0=no loop, 1=loop1, 2=loop2, 3=loop3
    
    // Check if params contains a "loop" field
    if (params.is_object() && params.contains("loop")) {
        if (params["loop"].is_number_unsigned()) {
            loop = params["loop"].get<uint8_t>();
            // Validate loop range
            if (loop > 3) {
                return {
                    {"status", "error"},
                    {"message", "loop must be 0, 1, 2, or 3"}
                };
            }
        }
        else if (params["loop"].is_boolean()) {
            // Backwards compatibility: true=1, false=0
            loop = params["loop"].get<bool>() ? 1 : 0;
        }
        else {
            return {
                {"status", "error"},
                {"message", "loop must be a number (0-3) or boolean"}
            };
        }
    }
    
    if (!CommandStation_Start(loop)) {
        return {
            {"status", "error"},
            {"message", "Command station is already running"}
        };
    }
    
    return {
        {"status", "ok"},
        {"message", "Command station started"},
        {"loop", loop}
    };
}

static json command_station_stop_handler(const json& params) {
    (void)params;  // Unused parameter
    
    if (!CommandStation_Stop()) {
        return {
            {"status", "error"},
            {"message", "Command station is not running"}
        };
    }
    
    return {
        {"status", "ok"},
        {"message", "Command station stopped"}
    };
}

static json command_station_load_packet_handler(const json& params) {
    if (!params.is_object() || !params.contains("bytes")) {
        return {
            {"status", "error"},
            {"message", "params must contain 'bytes' array"}
        };
    }
    
    if (!params["bytes"].is_array()) {
        return {
            {"status", "error"},
            {"message", "'bytes' must be an array"}
        };
    }
    
    auto bytes_array = params["bytes"];
    if (bytes_array.empty() || bytes_array.size() > DCC_MAX_PACKET_SIZE) {
        return {
            {"status", "error"},
            {"message", "bytes array must have 1-18 elements"}
        };
    }
    
    uint8_t bytes[DCC_MAX_PACKET_SIZE];
    uint8_t length = 0;
    
    for (const auto& byte : bytes_array) {
        if (!byte.is_number_unsigned()) {
            return {
                {"status", "error"},
                {"message", "all bytes must be unsigned integers"}
            };
        }
        uint32_t val = byte.get<uint32_t>();
        if (val > 0xFF) {
            return {
                {"status", "error"},
                {"message", "byte values must be 0-255"}
            };
        }
        bytes[length++] = static_cast<uint8_t>(val);
    }
    
    if (!CommandStation_LoadCustomPacket(bytes, length)) {
        return {
            {"status", "error"},
            {"message", "Failed to load packet"}
        };
    }
    
    return {
        {"status", "ok"},
        {"message", "Packet loaded successfully"},
        {"length", length}
    };
}

static json command_station_transmit_packet_handler(const json& params) {
    uint32_t count = 1;  // Default to 1 transmission
    uint32_t delay_ms = 100;  // Default to 100ms delay
    
    // Parse optional count parameter
    if (params.contains("count")) {
        count = params["count"].get<uint32_t>();
        if (count == 0) {
            return {
                {"status", "error"},
                {"message", "Count must be greater than 0"}
            };
        }
    }
    
    // Parse optional delay_ms parameter
    if (params.contains("delay_ms")) {
        delay_ms = params["delay_ms"].get<uint32_t>();
    }
    
    CommandStation_TriggerTransmit(count, delay_ms);
    
    return {
        {"status", "ok"},
        {"message", "Packet transmission triggered"},
        {"count", count},
        {"delay_ms", delay_ms}
    };
}

static json decoder_start_handler(const json& params) {
    (void)params;  // Unused parameter
    
    Decoder_Start();
    
    return {
        {"status", "ok"},
        {"message", "Decoder started"}
    };
}

static json decoder_stop_handler(const json& params) {
    (void)params;  // Unused parameter
    
    Decoder_Stop();
    
    return {
        {"status", "ok"},
        {"message", "Decoder stopped"}
    };
}

static json command_station_params_handler(const json& params) {
    // Check if params is an object
    if (!params.is_object()) {
        return {
            {"status", "error"},
            {"message", "Params must be an object"}
        };
    }
    
    // Set preamble bits if provided
    if (params.contains("preamble_bits")) {
        if (!params["preamble_bits"].is_number_unsigned()) {
            return {
                {"status", "error"},
                {"message", "preamble_bits must be a positive integer"}
            };
        }
        uint8_t preamble = params["preamble_bits"].get<uint8_t>();
        if (set_dcc_preamble_bits(preamble) != 0) {
            return {
                {"status", "error"},
                {"message", "Failed to set preamble_bits"}
            };
        }
    }
    
    // Set bit1 duration if provided
    if (params.contains("bit1_duration")) {
        if (!params["bit1_duration"].is_number_unsigned()) {
            return {
                {"status", "error"},
                {"message", "bit1_duration must be a positive integer"}
            };
        }
        uint8_t bit1 = params["bit1_duration"].get<uint8_t>();
        if (set_dcc_bit1_duration(bit1) != 0) {
            return {
                {"status", "error"},
                {"message", "Failed to set bit1_duration"}
            };
        }
    }
    
    // Set bit0 duration if provided
    if (params.contains("bit0_duration")) {
        if (!params["bit0_duration"].is_number_unsigned()) {
            return {
                {"status", "error"},
                {"message", "bit0_duration must be a positive integer"}
            };
        }
        uint8_t bit0 = params["bit0_duration"].get<uint8_t>();
        if (set_dcc_bit0_duration(bit0) != 0) {
            return {
                {"status", "error"},
                {"message", "Failed to set bit0_duration"}
            };
        }
    }
    
    // Set BiDi enable if provided
    if (params.contains("bidi_enable")) {
        if (!params["bidi_enable"].is_boolean()) {
            return {
                {"status", "error"},
                {"message", "bidi_enable must be a boolean"}
            };
        }
        uint8_t bidi = params["bidi_enable"].get<bool>() ? 1 : 0;
        if (set_dcc_bidi_enable(bidi) != 0) {
            return {
                {"status", "error"},
                {"message", "Failed to set bidi_enable"}
            };
        }
    }
    
    // Set trigger first bit if provided
    if (params.contains("trigger_first_bit")) {
        if (!params["trigger_first_bit"].is_boolean()) {
            return {
                {"status", "error"},
                {"message", "trigger_first_bit must be a boolean"}
            };
        }
        uint8_t trigger = params["trigger_first_bit"].get<bool>() ? 1 : 0;
        if (set_dcc_trigger_first_bit(trigger) != 0) {
            return {
                {"status", "error"},
                {"message", "Failed to set trigger_first_bit"}
            };
        }
    }
    
    return {
        {"status", "ok"},
        {"message", "Command station parameters updated"}
    };
}

static json command_station_packet_override_handler(const json& params) {
    // Check if params is an object
    if (!params.is_object()) {
        return {
            {"status", "error"},
            {"message", "Params must be an object"}
        };
    }
    
    // Set zero bit override mask if provided
    if (params.contains("zerobit_override_mask")) {
        if (!params["zerobit_override_mask"].is_number_unsigned()) {
            return {
                {"status", "error"},
                {"message", "zerobit_override_mask must be a positive integer"}
            };
        }
        uint64_t mask = params["zerobit_override_mask"].get<uint64_t>();
        CommandStation_SetZerobitOverrideMask(mask);
    }
    
    // Set zero bit deltaP if provided
    if (params.contains("zerobit_deltaP") || params.contains("zerobit_delta")) {
        // Support both old name (zerobit_delta) and new name (zerobit_deltaP) for backwards compatibility
        const char* key = params.contains("zerobit_deltaP") ? "zerobit_deltaP" : "zerobit_delta";
        if (!params[key].is_number_integer()) {
            return {
                {"status", "error"},
                {"message", "zerobit_deltaP must be an integer"}
            };
        }
        int32_t delta = params[key].get<int32_t>();
        CommandStation_SetZerobitDeltaP(delta);
    }
    
    // Set zero bit deltaN if provided
    if (params.contains("zerobit_deltaN")) {
        if (!params["zerobit_deltaN"].is_number_integer()) {
            return {
                {"status", "error"},
                {"message", "zerobit_deltaN must be an integer"}
            };
        }
        int32_t delta = params["zerobit_deltaN"].get<int32_t>();
        CommandStation_SetZerobitDeltaN(delta);
    }
    
    return {
        {"status", "ok"},
        {"message", "Packet override parameters updated"}
    };
}

static json command_station_packet_reset_override_handler(const json& params) {
    (void)params;  // Unused parameter
    
    // Reset all override parameters to 0
    CommandStation_SetZerobitOverrideMask(0);
    CommandStation_SetZerobitDeltaP(0);
    CommandStation_SetZerobitDeltaN(0);
    
    return {
        {"status", "ok"},
        {"message", "Packet override parameters reset to 0"}
    };
}

static json command_station_packet_get_override_handler(const json& params) {
    (void)params;  // Unused parameter
    
    // Get current override parameters
    uint64_t mask = CommandStation_GetZerobitOverrideMask();
    int32_t deltaP = CommandStation_GetZerobitDeltaP();
    int32_t deltaN = CommandStation_GetZerobitDeltaN();
    
    // Format mask as hex string for readability
    char mask_str[20];
    snprintf(mask_str, sizeof(mask_str), "0x%016llX", (unsigned long long)mask);
    
    return {
        {"status", "ok"},
        {"zerobit_override_mask", mask_str},
        {"zerobit_override_mask_decimal", mask},
        {"zerobit_deltaP", deltaP},
        {"zerobit_deltaN", deltaN}
    };
}

static json parameters_save_handler(const json& params) {
    (void)params;  // Unused parameter
    
    if (parameter_manager_save() != 0) {
        return {
            {"status", "error"},
            {"message", "Failed to save parameters to flash"}
        };
    }
    
    return {
        {"status", "ok"},
        {"message", "Parameters saved to flash"}
    };
}

static json parameters_restore_handler(const json& params) {
    (void)params;  // Unused parameter
    
    if (parameter_manager_restore() != 0) {
        return {
            {"status", "error"},
            {"message", "Failed to restore parameters from flash"}
        };
    }
    
    return {
        {"status", "ok"},
        {"message", "Parameters restored from flash"}
    };
}

static json parameters_factory_reset_handler(const json& params) {
    (void)params;  // Unused parameter
    
    parameter_manager_factory_reset();
    
    return {
        {"status", "ok"},
        {"message", "Factory reset completed - all parameters restored to defaults"}
    };
}

static json system_reboot_handler(const json& params) {
    (void)params;  // Unused parameter
    
    // Return success response before rebooting
    json response = {
        {"status", "ok"},
        {"message", "System rebooting..."}
    };
    
    // Small delay to allow response to be sent
    //TODO: ...probably will never send response before rebooting
    osDelay(100);
    
    // Perform system reset
    NVIC_SystemReset();
    
    // This line will never be reached
    return response;
}

static json get_voltage_feedback_mv_handler(const json& params) {
    (void)params;  // Unused parameter
    
    uint16_t voltage_mv = 0;
    
    if (get_voltage_feedback_mv(&voltage_mv) != 0) {
        return {
            {"status", "error"},
            {"message", "Failed to read voltage feedback"}
        };
    }
    
    return {
        {"status", "ok"},
        {"voltage_mv", voltage_mv}
    };
}

static json get_current_feedback_ma_handler(const json& params) {
    (void)params;  // Unused parameter
    
    uint16_t current_ma = 0;
    
    if (get_current_feedback_ma(&current_ma) != 0) {
        return {
            {"status", "error"},
            {"message", "Failed to read current feedback"}
        };
    }
    
    return {
        {"status", "ok"},
        {"current_ma", current_ma}
    };
}

static json get_gpio_input_handler(const json& params) {
    // Check if pin parameter exists
    if (!params.contains("pin") || !params["pin"].is_number_integer()) {
        return {
            {"status", "error"},
            {"message", "Missing or invalid 'pin' parameter (must be 1-16)"}
        };
    }
    
    int pin_num = params["pin"].get<int>();
    
    // Validate pin number
    if (pin_num < 1 || pin_num > 16) {
        return {
            {"status", "error"},
            {"message", "Pin number must be between 1 and 16"}
        };
    }
    
    GPIO_PinState state = GPIO_PIN_RESET;
    
    // Read the appropriate GPIO pin based on pin number
    switch (pin_num) {
        case 1:  state = HAL_GPIO_ReadPin(IO1_GPIO_Port, IO1_Pin); break;
        case 2:  state = HAL_GPIO_ReadPin(IO2_GPIO_Port, IO2_Pin); break;
        case 3:  state = HAL_GPIO_ReadPin(IO3_GPIO_Port, IO3_Pin); break;
        case 4:  state = HAL_GPIO_ReadPin(IO4_GPIO_Port, IO4_Pin); break;
        case 5:  state = HAL_GPIO_ReadPin(IO5_GPIO_Port, IO5_Pin); break;
        case 6:  state = HAL_GPIO_ReadPin(IO6_GPIO_Port, IO6_Pin); break;
        case 7:  state = HAL_GPIO_ReadPin(IO7_GPIO_Port, IO7_Pin); break;
        case 8:  state = HAL_GPIO_ReadPin(IO8_GPIO_Port, IO8_Pin); break;
        case 9:  state = HAL_GPIO_ReadPin(IO9_GPIO_Port, IO9_Pin); break;
        case 10: state = HAL_GPIO_ReadPin(IO10_GPIO_Port, IO10_Pin); break;
        case 11: state = HAL_GPIO_ReadPin(IO11_GPIO_Port, IO11_Pin); break;
        case 12: state = HAL_GPIO_ReadPin(IO12_GPIO_Port, IO12_Pin); break;
        case 13: state = HAL_GPIO_ReadPin(IO13_GPIO_Port, IO13_Pin); break;
        case 14: state = HAL_GPIO_ReadPin(IO14_GPIO_Port, IO14_Pin); break;
        case 15: state = HAL_GPIO_ReadPin(IO15_GPIO_Port, IO15_Pin); break;
        case 16: state = HAL_GPIO_ReadPin(IO16_GPIO_Port, IO16_Pin); break;
        default:
            return {
                {"status", "error"},
                {"message", "Invalid pin number"}
            };
    }
    
    return {
        {"status", "ok"},
        {"pin", pin_num},
        {"value", state == GPIO_PIN_SET ? 1 : 0}
    };
}

static json get_gpio_inputs_handler(const json& params) {
    (void)params;  // Unused parameter
    
    uint16_t gpio_word = 0;
    
    // Read all GPIO inputs and pack into a 16-bit word
    // Bit 0 = IO1, Bit 1 = IO2, ..., Bit 15 = IO16
    if (HAL_GPIO_ReadPin(IO1_GPIO_Port, IO1_Pin) == GPIO_PIN_SET)   gpio_word |= (1 << 0);
    if (HAL_GPIO_ReadPin(IO2_GPIO_Port, IO2_Pin) == GPIO_PIN_SET)   gpio_word |= (1 << 1);
    if (HAL_GPIO_ReadPin(IO3_GPIO_Port, IO3_Pin) == GPIO_PIN_SET)   gpio_word |= (1 << 2);
    if (HAL_GPIO_ReadPin(IO4_GPIO_Port, IO4_Pin) == GPIO_PIN_SET)   gpio_word |= (1 << 3);
    if (HAL_GPIO_ReadPin(IO5_GPIO_Port, IO5_Pin) == GPIO_PIN_SET)   gpio_word |= (1 << 4);
    if (HAL_GPIO_ReadPin(IO6_GPIO_Port, IO6_Pin) == GPIO_PIN_SET)   gpio_word |= (1 << 5);
    if (HAL_GPIO_ReadPin(IO7_GPIO_Port, IO7_Pin) == GPIO_PIN_SET)   gpio_word |= (1 << 6);
    if (HAL_GPIO_ReadPin(IO8_GPIO_Port, IO8_Pin) == GPIO_PIN_SET)   gpio_word |= (1 << 7);
    if (HAL_GPIO_ReadPin(IO9_GPIO_Port, IO9_Pin) == GPIO_PIN_SET)   gpio_word |= (1 << 8);
    if (HAL_GPIO_ReadPin(IO10_GPIO_Port, IO10_Pin) == GPIO_PIN_SET) gpio_word |= (1 << 9);
    if (HAL_GPIO_ReadPin(IO11_GPIO_Port, IO11_Pin) == GPIO_PIN_SET) gpio_word |= (1 << 10);
    if (HAL_GPIO_ReadPin(IO12_GPIO_Port, IO12_Pin) == GPIO_PIN_SET) gpio_word |= (1 << 11);
    if (HAL_GPIO_ReadPin(IO13_GPIO_Port, IO13_Pin) == GPIO_PIN_SET) gpio_word |= (1 << 12);
    if (HAL_GPIO_ReadPin(IO14_GPIO_Port, IO14_Pin) == GPIO_PIN_SET) gpio_word |= (1 << 13);
    if (HAL_GPIO_ReadPin(IO15_GPIO_Port, IO15_Pin) == GPIO_PIN_SET) gpio_word |= (1 << 14);
    if (HAL_GPIO_ReadPin(IO16_GPIO_Port, IO16_Pin) == GPIO_PIN_SET) gpio_word |= (1 << 15);
    
    // Format as hex string for easy reading
    char hex_str[7];  // "0x" + 4 hex digits + null terminator
    snprintf(hex_str, sizeof(hex_str), "0x%04X", gpio_word);
    
    return {
        {"status", "ok"},
        {"value", gpio_word},
        {"hex", hex_str}
    };
}

static json configure_gpio_output_handler(const json& params) {
    // Check if pin parameter exists
    if (!params.contains("pin") || !params["pin"].is_number_integer()) {
        return {
            {"status", "error"},
            {"message", "Missing or invalid 'pin' parameter (must be 1-16)"}
        };
    }
    
    // Check if state parameter exists
    if (!params.contains("state") || !params["state"].is_number_integer()) {
        return {
            {"status", "error"},
            {"message", "Missing or invalid 'state' parameter (must be 0 or 1)"}
        };
    }
    
    int pin_num = params["pin"].get<int>();
    int state = params["state"].get<int>();
    
    // Validate pin number
    if (pin_num < 1 || pin_num > 16) {
        return {
            {"status", "error"},
            {"message", "Pin number must be between 1 and 16"}
        };
    }
    
    // Validate state
    if (state != 0 && state != 1) {
        return {
            {"status", "error"},
            {"message", "State must be 0 (low) or 1 (high)"}
        };
    }
    
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    
    GPIO_PinState initial_state = (state == 1) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    
    // Configure the appropriate GPIO pin as output and set initial state
    switch (pin_num) {
        case 1:
            HAL_GPIO_WritePin(IO1_GPIO_Port, IO1_Pin, initial_state);
            GPIO_InitStruct.Pin = IO1_Pin;
            HAL_GPIO_Init(IO1_GPIO_Port, &GPIO_InitStruct);
            break;
        case 2:
            HAL_GPIO_WritePin(IO2_GPIO_Port, IO2_Pin, initial_state);
            GPIO_InitStruct.Pin = IO2_Pin;
            HAL_GPIO_Init(IO2_GPIO_Port, &GPIO_InitStruct);
            break;
        case 3:
            HAL_GPIO_WritePin(IO3_GPIO_Port, IO3_Pin, initial_state);
            GPIO_InitStruct.Pin = IO3_Pin;
            HAL_GPIO_Init(IO3_GPIO_Port, &GPIO_InitStruct);
            break;
        case 4:
            HAL_GPIO_WritePin(IO4_GPIO_Port, IO4_Pin, initial_state);
            GPIO_InitStruct.Pin = IO4_Pin;
            HAL_GPIO_Init(IO4_GPIO_Port, &GPIO_InitStruct);
            break;
        case 5:
            HAL_GPIO_WritePin(IO5_GPIO_Port, IO5_Pin, initial_state);
            GPIO_InitStruct.Pin = IO5_Pin;
            HAL_GPIO_Init(IO5_GPIO_Port, &GPIO_InitStruct);
            break;
        case 6:
            HAL_GPIO_WritePin(IO6_GPIO_Port, IO6_Pin, initial_state);
            GPIO_InitStruct.Pin = IO6_Pin;
            HAL_GPIO_Init(IO6_GPIO_Port, &GPIO_InitStruct);
            break;
        case 7:
            HAL_GPIO_WritePin(IO7_GPIO_Port, IO7_Pin, initial_state);
            GPIO_InitStruct.Pin = IO7_Pin;
            HAL_GPIO_Init(IO7_GPIO_Port, &GPIO_InitStruct);
            break;
        case 8:
            HAL_GPIO_WritePin(IO8_GPIO_Port, IO8_Pin, initial_state);
            GPIO_InitStruct.Pin = IO8_Pin;
            HAL_GPIO_Init(IO8_GPIO_Port, &GPIO_InitStruct);
            break;
        case 9:
            HAL_GPIO_WritePin(IO9_GPIO_Port, IO9_Pin, initial_state);
            GPIO_InitStruct.Pin = IO9_Pin;
            HAL_GPIO_Init(IO9_GPIO_Port, &GPIO_InitStruct);
            break;
        case 10:
            HAL_GPIO_WritePin(IO10_GPIO_Port, IO10_Pin, initial_state);
            GPIO_InitStruct.Pin = IO10_Pin;
            HAL_GPIO_Init(IO10_GPIO_Port, &GPIO_InitStruct);
            break;
        case 11:
            HAL_GPIO_WritePin(IO11_GPIO_Port, IO11_Pin, initial_state);
            GPIO_InitStruct.Pin = IO11_Pin;
            HAL_GPIO_Init(IO11_GPIO_Port, &GPIO_InitStruct);
            break;
        case 12:
            HAL_GPIO_WritePin(IO12_GPIO_Port, IO12_Pin, initial_state);
            GPIO_InitStruct.Pin = IO12_Pin;
            HAL_GPIO_Init(IO12_GPIO_Port, &GPIO_InitStruct);
            break;
        case 13:
            HAL_GPIO_WritePin(IO13_GPIO_Port, IO13_Pin, initial_state);
            GPIO_InitStruct.Pin = IO13_Pin;
            HAL_GPIO_Init(IO13_GPIO_Port, &GPIO_InitStruct);
            break;
        case 14:
            HAL_GPIO_WritePin(IO14_GPIO_Port, IO14_Pin, initial_state);
            GPIO_InitStruct.Pin = IO14_Pin;
            HAL_GPIO_Init(IO14_GPIO_Port, &GPIO_InitStruct);
            break;
        case 15:
            HAL_GPIO_WritePin(IO15_GPIO_Port, IO15_Pin, initial_state);
            GPIO_InitStruct.Pin = IO15_Pin;
            HAL_GPIO_Init(IO15_GPIO_Port, &GPIO_InitStruct);
            break;
        case 16:
            HAL_GPIO_WritePin(IO16_GPIO_Port, IO16_Pin, initial_state);
            GPIO_InitStruct.Pin = IO16_Pin;
            HAL_GPIO_Init(IO16_GPIO_Port, &GPIO_InitStruct);
            break;
        default:
            return {
                {"status", "error"},
                {"message", "Invalid pin number"}
            };
    }
    
    return {
        {"status", "ok"},
        {"message", "GPIO configured as output"},
        {"pin", pin_num},
        {"state", state}
    };
}

static json set_gpio_output_handler(const json& params) {
    // Check if pin parameter exists
    if (!params.contains("pin") || !params["pin"].is_number_integer()) {
        return {
            {"status", "error"},
            {"message", "Missing or invalid 'pin' parameter (must be 1-16)"}
        };
    }
    
    // Check if state parameter exists
    if (!params.contains("state") || !params["state"].is_number_integer()) {
        return {
            {"status", "error"},
            {"message", "Missing or invalid 'state' parameter (must be 0 or 1)"}
        };
    }
    
    int pin_num = params["pin"].get<int>();
    int state = params["state"].get<int>();
    
    // Validate pin number
    if (pin_num < 1 || pin_num > 16) {
        return {
            {"status", "error"},
            {"message", "Pin number must be between 1 and 16"}
        };
    }
    
    // Validate state
    if (state != 0 && state != 1) {
        return {
            {"status", "error"},
            {"message", "State must be 0 (low) or 1 (high)"}
        };
    }
    
    GPIO_PinState new_state = (state == 1) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    
    // Set the appropriate GPIO pin output state
    switch (pin_num) {
        case 1:  HAL_GPIO_WritePin(IO1_GPIO_Port, IO1_Pin, new_state); break;
        case 2:  HAL_GPIO_WritePin(IO2_GPIO_Port, IO2_Pin, new_state); break;
        case 3:  HAL_GPIO_WritePin(IO3_GPIO_Port, IO3_Pin, new_state); break;
        case 4:  HAL_GPIO_WritePin(IO4_GPIO_Port, IO4_Pin, new_state); break;
        case 5:  HAL_GPIO_WritePin(IO5_GPIO_Port, IO5_Pin, new_state); break;
        case 6:  HAL_GPIO_WritePin(IO6_GPIO_Port, IO6_Pin, new_state); break;
        case 7:  HAL_GPIO_WritePin(IO7_GPIO_Port, IO7_Pin, new_state); break;
        case 8:  HAL_GPIO_WritePin(IO8_GPIO_Port, IO8_Pin, new_state); break;
        case 9:  HAL_GPIO_WritePin(IO9_GPIO_Port, IO9_Pin, new_state); break;
        case 10: HAL_GPIO_WritePin(IO10_GPIO_Port, IO10_Pin, new_state); break;
        case 11: HAL_GPIO_WritePin(IO11_GPIO_Port, IO11_Pin, new_state); break;
        case 12: HAL_GPIO_WritePin(IO12_GPIO_Port, IO12_Pin, new_state); break;
        case 13: HAL_GPIO_WritePin(IO13_GPIO_Port, IO13_Pin, new_state); break;
        case 14: HAL_GPIO_WritePin(IO14_GPIO_Port, IO14_Pin, new_state); break;
        case 15: HAL_GPIO_WritePin(IO15_GPIO_Port, IO15_Pin, new_state); break;
        case 16: HAL_GPIO_WritePin(IO16_GPIO_Port, IO16_Pin, new_state); break;
        default:
            return {
                {"status", "error"},
                {"message", "Invalid pin number"}
            };
    }
    
    return {
        {"status", "ok"},
        {"message", "GPIO output state set"},
        {"pin", pin_num},
        {"state", state}
    };
}

static json get_rtc_datetime_handler(const json& params) {
    (void)params;  // Unused parameter
    
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};
    
    // Read time and date from RTC
    if (HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK) {
        return {
            {"status", "error"},
            {"message", "Failed to read RTC time"}
        };
    }
    
    if (HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK) {
        return {
            {"status", "error"},
            {"message", "Failed to read RTC date"}
        };
    }
    
    // Format date and time strings
    char date_str[16];
    char time_str[16];
    snprintf(date_str, sizeof(date_str), "20%02d-%02d-%02d", 
             sDate.Year, sDate.Month, sDate.Date);
    snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", 
             sTime.Hours, sTime.Minutes, sTime.Seconds);
    
    return {
        {"status", "ok"},
        {"date", date_str},
        {"time", time_str},
        {"year", sDate.Year + 2000},
        {"month", sDate.Month},
        {"day", sDate.Date},
        {"weekday", sDate.WeekDay},
        {"hours", sTime.Hours},
        {"minutes", sTime.Minutes},
        {"seconds", sTime.Seconds}
    };
}

static json set_rtc_datetime_handler(const json& params) {
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};
    bool set_date = false;
    bool set_time = false;
    
    // Check for date parameters
    if (params.contains("year") && params.contains("month") && params.contains("day")) {
        if (!params["year"].is_number_integer() || 
            !params["month"].is_number_integer() || 
            !params["day"].is_number_integer()) {
            return {
                {"status", "error"},
                {"message", "Date parameters must be integers"}
            };
        }
        
        int year = params["year"].get<int>();
        int month = params["month"].get<int>();
        int day = params["day"].get<int>();
        
        // Validate date ranges
        if (year < 2000 || year > 2099) {
            return {
                {"status", "error"},
                {"message", "Year must be between 2000 and 2099"}
            };
        }
        if (month < 1 || month > 12) {
            return {
                {"status", "error"},
                {"message", "Month must be between 1 and 12"}
            };
        }
        if (day < 1 || day > 31) {
            return {
                {"status", "error"},
                {"message", "Day must be between 1 and 31"}
            };
        }
        
        sDate.Year = year - 2000;
        sDate.Month = month;
        sDate.Date = day;
        
        // Optional weekday parameter (1-7, Monday=1)
        if (params.contains("weekday") && params["weekday"].is_number_integer()) {
            int weekday = params["weekday"].get<int>();
            if (weekday < 1 || weekday > 7) {
                return {
                    {"status", "error"},
                    {"message", "Weekday must be between 1 (Monday) and 7 (Sunday)"}
                };
            }
            sDate.WeekDay = weekday;
        } else {
            sDate.WeekDay = RTC_WEEKDAY_MONDAY;  // Default
        }
        
        set_date = true;
    }
    
    // Check for time parameters
    if (params.contains("hours") && params.contains("minutes") && params.contains("seconds")) {
        if (!params["hours"].is_number_integer() || 
            !params["minutes"].is_number_integer() || 
            !params["seconds"].is_number_integer()) {
            return {
                {"status", "error"},
                {"message", "Time parameters must be integers"}
            };
        }
        
        int hours = params["hours"].get<int>();
        int minutes = params["minutes"].get<int>();
        int seconds = params["seconds"].get<int>();
        
        // Validate time ranges
        if (hours < 0 || hours > 23) {
            return {
                {"status", "error"},
                {"message", "Hours must be between 0 and 23"}
            };
        }
        if (minutes < 0 || minutes > 59) {
            return {
                {"status", "error"},
                {"message", "Minutes must be between 0 and 59"}
            };
        }
        if (seconds < 0 || seconds > 59) {
            return {
                {"status", "error"},
                {"message", "Seconds must be between 0 and 59"}
            };
        }
        
        sTime.Hours = hours;
        sTime.Minutes = minutes;
        sTime.Seconds = seconds;
        sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
        sTime.StoreOperation = RTC_STOREOPERATION_RESET;
        
        set_time = true;
    }
    
    // Check if at least date or time was provided
    if (!set_date && !set_time) {
        return {
            {"status", "error"},
            {"message", "Must provide date (year, month, day) and/or time (hours, minutes, seconds)"}
        };
    }
    
    // Set date if provided
    if (set_date) {
        if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK) {
            return {
                {"status", "error"},
                {"message", "Failed to set RTC date"}
            };
        }
    }
    
    // Set time if provided
    if (set_time) {
        if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK) {
            return {
                {"status", "error"},
                {"message", "Failed to set RTC time"}
            };
        }
    }
    
    json response = {
        {"status", "ok"},
        {"message", "RTC updated successfully"}
    };
    
    if (set_date) {
        response["date_set"] = true;
    }
    if (set_time) {
        response["time_set"] = true;
    }
    
    return response;
}

static json command_station_get_params_handler(const json& params) {
    (void)params;  // Unused parameter
    
    // Retrieve all command station parameters
    uint16_t track_voltage = 0;
    uint8_t preamble_bits = 0;
    uint8_t bit1_duration = 0;
    uint8_t bit0_duration = 0;
    uint8_t bidi_enable = 0;
    uint16_t bidi_dac = 0;
    uint8_t trigger_first_bit = 0;
    
    // Get persistent parameters from parameter manager
    if (get_dcc_track_voltage(&track_voltage) != 0 ||
        get_dcc_preamble_bits(&preamble_bits) != 0 ||
        get_dcc_bit1_duration(&bit1_duration) != 0 ||
        get_dcc_bit0_duration(&bit0_duration) != 0 ||
        get_dcc_bidi_enable(&bidi_enable) != 0 ||
        get_dcc_bidi_dac(&bidi_dac) != 0 ||
        get_dcc_trigger_first_bit(&trigger_first_bit) != 0) {
        return {
            {"status", "error"},
            {"message", "Failed to retrieve one or more parameters"}
        };
    }
    
    // Get RAM-only override parameters from command station
    uint64_t zerobit_override_mask = CommandStation_GetZerobitOverrideMask();
    int32_t zerobit_deltaP = CommandStation_GetZerobitDeltaP();
    int32_t zerobit_deltaN = CommandStation_GetZerobitDeltaN();
    
    // Convert uint64_t to hex string for JSON compatibility
    char mask_str[19];  // "0x" + 16 hex digits + null terminator
    snprintf(mask_str, sizeof(mask_str), "0x%016llX", (unsigned long long)zerobit_override_mask);
    
    return {
        {"status", "ok"},
        {"parameters", {
            {"track_voltage", track_voltage},
            {"preamble_bits", preamble_bits},
            {"bit1_duration", bit1_duration},
            {"bit0_duration", bit0_duration},
            {"bidi_enable", bidi_enable != 0},
            {"bidi_dac", bidi_dac},
            {"trigger_first_bit", trigger_first_bit != 0},
            {"zerobit_override_mask", mask_str},
            {"zerobit_deltaP", zerobit_deltaP},
            {"zerobit_deltaN", zerobit_deltaN}
        }}
    };
}

// ---------------- RTOS Task ----------------

RpcServer server;

void RpcServerThread(void* argument) {
    (void)argument;
    rpc_rxbuffer_t* msg;
    ULONG actual_length;

    osSemaphoreAcquire(rpcServerStart_sem, osWaitForever);
    rpcServerRunning = true;

    server.register_method("echo", echo_handler);
    server.register_method("command_station_start", command_station_start_handler);
    server.register_method("command_station_stop", command_station_stop_handler);
    server.register_method("command_station_load_packet", command_station_load_packet_handler);
    server.register_method("command_station_transmit_packet", command_station_transmit_packet_handler);
    server.register_method("command_station_params", command_station_params_handler);
    server.register_method("command_station_packet_override", command_station_packet_override_handler);
    server.register_method("command_station_packet_reset_override", command_station_packet_reset_override_handler);
    server.register_method("command_station_packet_get_override", command_station_packet_get_override_handler);
    server.register_method("command_station_get_params", command_station_get_params_handler);
    server.register_method("decoder_start", decoder_start_handler);
    server.register_method("decoder_stop", decoder_stop_handler);
    server.register_method("parameters_save", parameters_save_handler);
    server.register_method("parameters_restore", parameters_restore_handler);
    server.register_method("parameters_factory_reset", parameters_factory_reset_handler);
    server.register_method("system_reboot", system_reboot_handler);
    server.register_method("get_voltage_feedback_mv", get_voltage_feedback_mv_handler);
    server.register_method("get_current_feedback_ma", get_current_feedback_ma_handler);
    server.register_method("get_gpio_input", get_gpio_input_handler);
    server.register_method("get_gpio_inputs", get_gpio_inputs_handler);
    server.register_method("configure_gpio_output", configure_gpio_output_handler);
    server.register_method("set_gpio_output", set_gpio_output_handler);
    server.register_method("get_rtc_datetime", get_rtc_datetime_handler);
    server.register_method("set_rtc_datetime", set_rtc_datetime_handler);

    while (rpcServerRunning) {
        std::string request;
        // Block until a message pointer is available from RX thread
        if (tx_queue_receive(&rpc_rxqueue, &msg, MS_TO_TICK(10)) == TX_SUCCESS)
        {
            std::string request(msg->data, msg->length);
            std::string response = server.handle(request);
            /* transport_send(response);*/
            /* Send response data over the class cdc_acm_write */
            ux_device_class_cdc_acm_write(cdc_acm, (UCHAR *)response.c_str(),
                                                response.size(), &actual_length);
        }
    }
    osSemaphoreRelease(rpcServerStart_sem);
    osDelay(5u);
}

// ---------------- Init / Start / Stop ----------------

extern "C" void RpcServer_Init(void) {
    rpcServerStart_sem = osSemaphoreNew(1, 1, NULL);
    rpcServerThread_id = osThreadNew(RpcServerThread, NULL, &rpcServerTask_attributes);
}

extern "C" void RpcServer_Start(bool test_mode) {
    if (!rpcServerRunning) {
        osSemaphoreRelease(rpcServerStart_sem);
        printf("RPC Server started\n");
    } else {
        printf("RPC Server already running\n");
    }
}

extern "C" void RpcServer_Stop(void) {
    if (rpcServerRunning) {
        printf("RPC Server stopping\n");
        rpcServerRunning = false;
        osSemaphoreAcquire(rpcServerStart_sem, osWaitForever);
        printf("RPC Server stopped\n");
    } else {
        printf("RPC Server not running\n");
    }
}
