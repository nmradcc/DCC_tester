#include "cmsis_os2.h"
#include "stm32h5xx_nucleo.h"
#include "stm32h5xx_hal.h"
#include "main.h"
#include "command_station.h"

#include "rpc_server.hpp"
extern "C" {
#include "ux_api.h"
#include "ux_device_class_cdc_acm.h"
}
#include <cstring>

// USBX CDC ACM handle (CubeMX usually generates this)
extern UX_SLAVE_CLASS_CDC_ACM *cdc_acm;

static osThreadId_t rpcServerThread_id;
static osSemaphoreId_t rpcServerStart_sem;
static bool rpcServerRunning = false;

/* Definitions for rpcServerTask */
const osThreadAttr_t rpcServerTask_attributes = {
  .name = "rpcServerTask",
  .stack_size = 8192,
  .priority = (osPriority_t) osPriorityHigh
};

// ---------------- Transport using USBX CDC ACM ----------------

// Blocking receive: waits until a packet arrives, returns as string
static bool transport_receive(std::string& out) {
    UCHAR buf[128];
    ULONG actual_length = 0;

    // Blocking read from USB CDC ACM
    UINT status = ux_device_class_cdc_acm_read(cdc_acm, buf, sizeof(buf)-1, &actual_length);
    if (status != UX_SUCCESS || actual_length == 0) {
        return false;
    }

    buf[actual_length] = '\0';  // ensure null-terminated
    out.assign(reinterpret_cast<char*>(buf));
    return true;
}

// Send data over USB CDC ACM
static void transport_send(const std::string& msg) {
    ULONG actual_length = 0;
    ux_device_class_cdc_acm_write(cdc_acm,
                                  (UCHAR*)msg.c_str(),
                                  msg.size(),
                                  &actual_length);

    // Append CRLF for framing
    const char newline[] = "\r\n";
    ux_device_class_cdc_acm_write(cdc_acm,
                                  (UCHAR*)newline,
                                  2,
                                  &actual_length);
}

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
    rapidjson::Document resp;
    resp.SetObject();
    auto& alloc = resp.GetAllocator();

    resp.AddMember("status", "error", alloc);
    resp.AddMember("message", rapidjson::Value(msg ? msg : "error", alloc), alloc);

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    resp.Accept(writer);

    return buffer.GetString();
}

std::string RpcServer::handle(const std::string& request_str) {
    rapidjson::Document request;
    if (request.Parse(request_str.c_str()).HasParseError()) {
        return error_response("Invalid JSON");
    }

    if (!request.HasMember("method") || !request.HasMember("params")) {
        return error_response("Malformed request");
    }

    if (!request["method"].IsString()) {
        return error_response("Method must be string");
    }

    const char* method_cstr = request["method"].GetString();
    RpcHandlerFn handler = find(method_cstr);
    if (!handler) {
        return error_response("Unknown method");
    }

    rapidjson::Document response = handler(request["params"]);

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    response.Accept(writer);

    return buffer.GetString();
}

// ---------------- Example handlers ----------------

static rapidjson::Document add_handler(const rapidjson::Value& params) {
    rapidjson::Document resp;
    resp.SetObject();
    auto& alloc = resp.GetAllocator();

    if (!params.IsArray() || params.Size() < 2) {
        resp.AddMember("status", "error", alloc);
        resp.AddMember("message", "missing params", alloc);
        return resp;
    }
    if (!params[0].IsInt() || !params[1].IsInt()) {
        resp.AddMember("status", "error", alloc);
        resp.AddMember("message", "params must be integers", alloc);
        return resp;
    }

    int a = params[0].GetInt();
    int b = params[1].GetInt();
    resp.AddMember("status", "ok", alloc);
    resp.AddMember("result", a + b, alloc);
    return resp;
}

static rapidjson::Document echo_handler(const rapidjson::Value& params) {
    rapidjson::Document resp;
    resp.SetObject();
    auto& alloc = resp.GetAllocator();

    resp.AddMember("status", "ok", alloc);
    resp.AddMember("echo", rapidjson::Value(params, alloc), alloc);
    return resp;
}

// ---------------- RTOS Task ----------------

RpcServer server;

void RpcServerThread(void* argument) {
    (void)argument;

    osSemaphoreAcquire(rpcServerStart_sem, osWaitForever);
    rpcServerRunning = true;

    server.register_method("add",  add_handler);
    server.register_method("echo", echo_handler);

    while (rpcServerRunning) {
        std::string request;
        if (transport_receive(request)) {
            std::string response = server.handle(request);
            transport_send(response);
        }
        osDelay(10);
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
