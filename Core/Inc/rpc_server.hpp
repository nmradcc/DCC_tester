#pragma once

#include <string>
#include <unordered_map>
#include <functional>
#include <nlohmann/json.hpp>

// Task entry point for hosting the RPC server
extern "C" void vRPCTask(void* ctx);

// Plain C-style handler signature to reduce overhead
typedef nlohmann::json (*RpcHandlerFn)(const nlohmann::json&);

// Fixed-size registry entry
struct RpcEntry {
    const char* name;
    RpcHandlerFn handler;
};

// Simple RPC server class for embedded systems
class RpcServer {
public:
    using json = nlohmann::json;

    RpcServer() : count(0) {
        for (int i = 0; i < kMaxMethods; ++i) {
            table[i] = {nullptr, nullptr};
        }
    }

    // Register a handler into the fixed table
    bool register_method(const char* name, RpcHandlerFn handler);

    // Handle a raw request string and return a serialized response
    std::string handle(const std::string& request_str);

private:
    static constexpr int kMaxMethods = 16;
    RpcEntry table[kMaxMethods];
    int count;

    std::string error_response(const char* msg);
    RpcHandlerFn find(const char* name) const;
};
