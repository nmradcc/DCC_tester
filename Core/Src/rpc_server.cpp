#include "cmsis_os2.h"
#include "stm32h5xx_nucleo.h"
#include "stm32h5xx_hal.h"
#include "main.h"
#include "command_station.h"

#include "rpc_server.hpp"
#include <iostream>

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

void RpcServer::register_method(const std::string& name, RpcHandler handler) {
    methods[name] = handler;
}

std::string RpcServer::handle(const std::string& request_str) {
    // Use parse with exceptions disabled
    json request = json::parse(request_str, nullptr, false);
    if (request.is_discarded()) {
        return error_response("Invalid JSON");
    }

    if (!request.contains("method") || !request.contains("params")) {
        return error_response("Malformed request");
    }

    std::string method = request["method"];
    json params = request["params"];

    auto it = methods.find(method);
    if (it == methods.end()) {
        return error_response("Unknown method: " + method);
    }

    json result = it->second(params);
    json response = { {"status","ok"}, {"result",result} };
    return response.dump();
}

std::string RpcServer::error_response(const std::string& msg) {
    json response = { {"status","error"}, {"message",msg} };
    return response.dump();
}

// The task entry point
extern "C" void vRPCTask(void* ctx) {
    RpcServer* server = static_cast<RpcServer*>(ctx);

    // Register example methods
    server->register_method("add", [](const RpcServer::json& params) {
        return params[0].get<int>() + params[1].get<int>();
    });

    server->register_method("echo", [](const RpcServer::json& params) {
        return params;
    });

    // Main loop
    while (true) {
        std::string request;
        if (transport_receive(request)) {
            std::string response = server->handle(request);
            transport_send(response);
        }

        osDelay(10);
    }
}
