#include "rpc_server.hpp"
#include <iostream>

static osThreadId_t rpcServerThread_id;
static osSemaphoreId_t rpcServerStart_sem;
static bool rpcServerRunning = false;

/* Definitions for rpcServerTask */
const osThreadAttr_t rpcServerTask_attributes = {
  .name = "rpcServerTask",
  .stack_size = 8192,
  .priority = (osPriority_t) osPriorityHigh
};


// Transport stubs (replace with UART/TCP/etc.)
static bool transport_receive(std::string& out) {
    // Example: blocking read from stdin for demo
    std::string line;
    if (std::getline(std::cin, line)) {
        out = line;
        return true;
    }
    return false;
}

static void transport_send(const std::string& msg) {
    // Example: write to stdout for demo
    std::cout << msg << std::endl;
}

// Implementation of RpcServer methods
using json = nlohmann::json;

bool RpcServer::register_method(const char* name, RpcHandlerFn handler) {
    if (!name || !handler) return false;
    if (count >= kMaxMethods) return false;

    // Prevent duplicates
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

std::string RpcServer::handle(const std::string& request_str) {
    // Non-throwing parse mode
    json request = json::parse(request_str, nullptr, false);
    if (request.is_discarded()) {
        return error_response("Invalid JSON");
    }

    if (!request.contains("method") || !request.contains("params")) {
        return error_response("Malformed request");
    }

    // Use safe accessors
    const char* method_cstr = nullptr;
    if (request["method"].is_string()) {
        // Pull into a std::string temporarily; JSON string storage lives separately
        static std::string method_buf;
        method_buf = request["method"].get<std::string>();
        method_cstr = method_buf.c_str();
    } else {
        return error_response("Method must be string");
    }

    RpcHandlerFn handler = find(method_cstr);
    if (!handler) {
        return error_response("Unknown method");
    }

    const json& params = request["params"];
    json result = handler(params);
    json response = { {"status","ok"}, {"result",result} };
    return response.dump();
}

std::string RpcServer::error_response(const char* msg) {
    json response = { {"status","error"}, {"message",msg ? msg : "error"} };
    return response.dump();
}


// Example handlers
static json add_handler(const json& params) {
    if (!params.is_array() || params.size() < 2) return json{{"error","missing params"}};
    if (!params[0].is_number_integer() || !params[1].is_number_integer())
        return json{{"error","params must be integers"}};
    int a = params[0].get<int>();
    int b = params[1].get<int>();
    return json(a + b);
}

static json echo_handler(const json& params) {
    return params;
}
    
RpcServer server;


// The task entry point
void RpcServerThread(void*  argument) {
    (void)argument;  // Unused parameter


    // Block until externally started
    osSemaphoreAcquire(rpcServerStart_sem, osWaitForever);
    rpcServerRunning = true;

    // Register methods once
    server.register_method("add",  add_handler);
    server.register_method("echo", echo_handler);
    
    // Main loop
    while (rpcServerRunning) {
        std::string request;
        if (transport_receive(request)) {
            std::string response = server.handle(request);
            transport_send(response);
        }

        osDelay(10);
    }
    osSemaphoreRelease(rpcServerStart_sem);
    osDelay(5u); // Give some time for the semaphore to be released
}


// Called at system init
extern "C" void RpcServer_Init(void)
{
    rpcServerStart_sem = osSemaphoreNew(1, 0, NULL);  // Start locked
    rpcServerThread_id = osThreadNew(RpcServerThread, NULL, &rpcServerTask_attributes);
}

// Can be called from anywhere
extern "C" void RpcServer_Start(bool test_mode)
{
  if (!rpcServerRunning) {
    osSemaphoreRelease(rpcServerStart_sem);
    printf("RPC Server started\n");
  }
  else {
    printf("RPC Server already running\n");
  } 
}

extern "C" void RpcServer_Stop(void)
{
  if (rpcServerRunning) {
    printf("RPC Server stopping\n");
    rpcServerRunning = false;
    osSemaphoreAcquire(rpcServerStart_sem, osWaitForever);
    printf("RPC Server stopped\n");
  }
  else {
    printf("RPC Server not running\n");
  }
} 
