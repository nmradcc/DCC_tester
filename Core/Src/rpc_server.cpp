#include "cmsis_os2.h"
#include "stm32h5xx_nucleo.h"
#include "stm32h5xx_hal.h"
#include "main.h"
#include "command_station.h"

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

// ---------------- Example handlers ----------------

static json add_handler(const json& params) {
    if (!params.is_array() || params.size() < 2) {
        return {
            {"status", "error"},
            {"message", "missing params"}
        };
    }
    if (!params[0].is_number_integer() || !params[1].is_number_integer()) {
        return {
            {"status", "error"},
            {"message", "params must be integers"}
        };
    }

    int a = params[0].get<int>();
    int b = params[1].get<int>();
    return {
        {"status", "ok"},
        {"result", a + b}
    };
}

static json echo_handler(const json& params) {
    return {
        {"status", "ok"},
        {"echo", params}
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

    server.register_method("add",  add_handler);
    server.register_method("echo", echo_handler);

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
    rpcServerStart_sem = osSemaphoreNew(1, 0, NULL);
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
