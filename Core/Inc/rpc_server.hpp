

//void vRPCTask(void *pvParameters);

#pragma once

#include <string>
#include <unordered_map>
#include <functional>
#include <nlohmann/json.hpp>

// Task entry point for hosting the RPC server
extern "C" void vRPCTask(void* ctx);

// Simple RPC server class for embedded systems
class RpcServer {
public:
    using json = nlohmann::json;
    using RpcHandler = std::function<json(const json&)>;

    RpcServer() = default;
    ~RpcServer() = default;

    // Register a new RPC method by name
    void register_method(const std::string& name, RpcHandler handler);

    // Handle a raw request string and return a serialized response
    std::string handle(const std::string& request_str);

private:
    std::unordered_map<std::string, RpcHandler> methods;

    // Helper to generate error responses
    std::string error_response(const std::string& msg);
};

