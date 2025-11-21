#pragma once

#include <string>
#include <functional>
#include <cstring>

// RapidJSON headers
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

// Task entry point for hosting the RPC server
extern "C" void vRPCTask(void* ctx);

// Plain C-style handler signature to reduce overhead
// Handlers take a JSON Value (the "params" node) and return a response Document
typedef rapidjson::Document (*RpcHandlerFn)(const rapidjson::Value&);

// Fixed-size registry entry
struct RpcEntry {
    const char* name;
    RpcHandlerFn handler;
};

// Simple RPC server class for embedded systems
class RpcServer {
public:
    using json = rapidjson::Document;

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
