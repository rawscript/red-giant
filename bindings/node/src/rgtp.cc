#include <napi.h>
#include <thread>
#include <chrono>
#include <fstream>
#include <vector>
#include "../../include/rgtp/rgtp.h"

// Initialize RGTP
Napi::Value Init(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    int result = rgtp_init();
    return Napi::Number::New(env, result);
}

// Cleanup RGTP
Napi::Value Cleanup(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    rgtp_cleanup();
    return env.Undefined();
}

// Get RGTP version
Napi::Value GetVersion(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    const char* version = rgtp_version();
    return Napi::String::New(env, version);
}

// Create RGTP socket
Napi::Value Socket(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    int sockfd = rgtp_socket();
    return Napi::Number::New(env, sockfd);
}

// Bind RGTP socket to port
Napi::Value Bind(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    int sockfd = info[0].As<Napi::Number>().Int32Value();
    uint16_t port = info[1].As<Napi::Number>().Uint32Value();
    
    int result = rgtp_bind(sockfd, port);
    return Napi::Number::New(env, result);
}

// Expose data via RGTP
Napi::Value ExposeData(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    // Get data as buffer
    Napi::Buffer<char> dataBuffer = info[0].As<Napi::Buffer<char>>();
    char* data = dataBuffer.Data();
    size_t size = dataBuffer.Length();
    
    rgtp_surface_t* surface;
    int sockfd = info[1].As<Napi::Number>().Int32Value();
    
    int result = rgtp_expose_data(sockfd, data, size, nullptr, &surface);
    
    if (result == 0) {
        // Return the surface pointer as a number for use in JavaScript
        return Napi::Number::New(env, reinterpret_cast<uint64_t>(surface));
    } else {
        return Napi::Number::New(env, -1);
    }
}

// Poll for RGTP events
Napi::Value Poll(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    uint64_t surfacePtr = info[0].As<Napi::Number>().Int64Value();
    int timeout = info[1].As<Napi::Number>().Int32Value();
    
    rgtp_surface_t* surface = reinterpret_cast<rgtp_surface_t*>(surfacePtr);
    
    int result = rgtp_poll(surface, timeout);
    return Napi::Number::New(env, result);
}

// Destroy RGTP surface
Napi::Value DestroySurface(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    uint64_t surfacePtr = info[0].As<Napi::Number>().Int64Value();
    rgtp_surface_t* surface = reinterpret_cast<rgtp_surface_t*>(surfacePtr);
    
    rgtp_destroy_surface(surface);
    return env.Undefined();
}

// Initialize the addon
Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set(Napi::String::New(env, "init"), Napi::Function::New(env, Init));
    exports.Set(Napi::String::New(env, "cleanup"), Napi::Function::New(env, Cleanup));
    exports.Set(Napi::String::New(env, "getVersion"), Napi::Function::New(env, GetVersion));
    exports.Set(Napi::String::New(env, "socket"), Napi::Function::New(env, Socket));
    exports.Set(Napi::String::New(env, "bind"), Napi::Function::New(env, Bind));
    exports.Set(Napi::String::New(env, "exposeData"), Napi::Function::New(env, ExposeData));
    exports.Set(Napi::String::New(env, "poll"), Napi::Function::New(env, Poll));
    exports.Set(Napi::String::New(env, "destroySurface"), Napi::Function::New(env, DestroySurface));
    
    return exports;
}

NODE_API_MODULE(rgtp, Init)