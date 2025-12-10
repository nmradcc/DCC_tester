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
    
    CommandStation_Start(loop);
    
    return {
        {"status", "ok"},
        {"message", "Command station started"},
        {"loop", loop}
    };
}

static json command_station_stop_handler(const json& params) {
    (void)params;  // Unused parameter
    
    CommandStation_Stop();
    
    return {
        {"status", "ok"},
        {"message", "Command station stopped"}
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
    
    // Set zero bit override mask if provided
    if (params.contains("zerobit_override_mask")) {
        if (!params["zerobit_override_mask"].is_number_unsigned()) {
            return {
                {"status", "error"},
                {"message", "zerobit_override_mask must be a positive integer"}
            };
        }
        uint64_t mask = params["zerobit_override_mask"].get<uint64_t>();
        if (set_dcc_zerobit_override_mask(mask) != 0) {
            return {
                {"status", "error"},
                {"message", "Failed to set zerobit_override_mask"}
            };
        }
    }
    
    // Set zero bit delta if provided
    if (params.contains("zerobit_delta")) {
        if (!params["zerobit_delta"].is_number_integer()) {
            return {
                {"status", "error"},
                {"message", "zerobit_delta must be an integer"}
            };
        }
        int32_t delta = params["zerobit_delta"].get<int32_t>();
        if (set_dcc_zerobit_delta(delta) != 0) {
            return {
                {"status", "error"},
                {"message", "Failed to set zerobit_delta"}
            };
        }
    }
    
    return {
        {"status", "ok"},
        {"message", "Command station parameters updated"}
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
    server.register_method("command_station_params", command_station_params_handler);
    server.register_method("decoder_start", decoder_start_handler);
    server.register_method("decoder_stop", decoder_stop_handler);
    server.register_method("parameters_save", parameters_save_handler);
    server.register_method("parameters_restore", parameters_restore_handler);
    server.register_method("parameters_factory_reset", parameters_factory_reset_handler);
    server.register_method("system_reboot", system_reboot_handler);
    server.register_method("get_voltage_feedback_mv", get_voltage_feedback_mv_handler);
    server.register_method("get_current_feedback_ma", get_current_feedback_ma_handler);

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
