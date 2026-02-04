#include <napi.h>
#include <thread>
#include <chrono>
#include <fstream>
#include <vector>
#include "../../include/rgtp/rgtp.h"

// Forward declarations for new functions
Napi::Value SessionCreate(const Napi::CallbackInfo& info);
Napi::Value SessionExposeFile(const Napi::CallbackInfo& info);
Napi::Value SessionWaitComplete(const Napi::CallbackInfo& info);
Napi::Value SessionGetStats(const Napi::CallbackInfo& info);
Napi::Value SessionDestroy(const Napi::CallbackInfo& info);

Napi::Value ClientCreate(const Napi::CallbackInfo& info);
Napi::Value ClientPullToFile(const Napi::CallbackInfo& info);
Napi::Value ClientGetStats(const Napi::CallbackInfo& info);
Napi::Value ClientDestroy(const Napi::CallbackInfo& info);

Napi::Value SetExposureRate(const Napi::CallbackInfo& info);
Napi::Value AdaptiveExposure(const Napi::CallbackInfo& info);
Napi::Value GetExposureStatus(const Napi::CallbackInfo& info);

// Helper function to convert JavaScript object to rgtp_config_t
rgtp_config_t* ParseConfig(const Napi::Object& jsConfig) {
    rgtp_config_t* config = new rgtp_config_t();
    
    config->chunk_size = jsConfig.Has("chunkSize") ? 
        jsConfig.Get("chunkSize").As<Napi::Number>().Uint32Value() : 1024 * 1024;
    config->exposure_rate = jsConfig.Has("exposureRate") ? 
        jsConfig.Get("exposureRate").As<Napi::Number>().Uint32Value() : 1000;
    config->adaptive_mode = jsConfig.Has("adaptiveMode") ? 
        jsConfig.Get("adaptiveMode").As<Napi::Boolean>().Value() : true;
    config->enable_compression = jsConfig.Has("enableCompression") ? 
        jsConfig.Get("enableCompression").As<Napi::Boolean>().Value() : false;
    config->enable_encryption = jsConfig.Has("enableEncryption") ? 
        jsConfig.Get("enableEncryption").As<Napi::Boolean>().Value() : false;
    config->port = jsConfig.Has("port") ? 
        jsConfig.Get("port").As<Napi::Number>().Uint32Value() : 0;
    config->timeout_ms = jsConfig.Has("timeout") ? 
        jsConfig.Get("timeout").As<Napi::Number>().Int32Value() : 30000;
    config->user_data = nullptr;
    
    return config;
}

// Helper function to convert rgtp_stats_t to JavaScript object
Napi::Object StatsToJSObject(Napi::Env env, const rgtp_stats_t& stats) {
    Napi::Object jsStats = Napi::Object::New(env);
    
    jsStats.Set("bytesSent", Napi::Number::New(env, stats.bytes_sent));
    jsStats.Set("bytesReceived", Napi::Number::New(env, stats.bytes_received));
    jsStats.Set("chunksSent", Napi::Number::New(env, stats.chunks_sent));
    jsStats.Set("chunksReceived", Napi::Number::New(env, stats.chunks_received));
    jsStats.Set("packetLossRate", Napi::Number::New(env, stats.packet_loss_rate));
    jsStats.Set("rttMs", Napi::Number::New(env, stats.rtt_ms));
    jsStats.Set("packetsLost", Napi::Number::New(env, stats.packets_lost));
    jsStats.Set("retransmissions", Napi::Number::New(env, stats.retransmissions));
    jsStats.Set("avgThroughputMbps", Napi::Number::New(env, stats.avg_throughput_mbps));
    jsStats.Set("completionPercent", Napi::Number::New(env, stats.completion_percent));
    jsStats.Set("activeConnections", Napi::Number::New(env, stats.active_connections));
    
    return jsStats;
}

// Session management functions
Napi::Value SessionCreate(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    rgtp_config_t* config = nullptr;
    if (info.Length() > 0 && info[0].IsObject()) {
        config = ParseConfig(info[0].As<Napi::Object>());
    }
    
    rgtp_session_t* session = rgtp_session_create(config);
    delete config; // Clean up the config
    
    if (!session) {
        Napi::Error::New(env, "Failed to create RGTP session").ThrowAsJavaScriptException();
        return env.Null();
    }
    
    return Napi::BigInt::New(env, reinterpret_cast<uint64_t>(session));
}

Napi::Value SessionExposeFile(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    uint64_t sessionPtr = info[0].As<Napi::BigInt>().Uint64Value();
    std::string filename = info[1].As<Napi::String>().Utf8Value();
    
    rgtp_session_t* session = reinterpret_cast<rgtp_session_t*>(sessionPtr);
    int result = rgtp_session_expose_file(session, filename.c_str());
    
    return Napi::Number::New(env, result);
}

Napi::Value SessionWaitComplete(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    uint64_t sessionPtr = info[0].As<Napi::BigInt>().Uint64Value();
    rgtp_session_t* session = reinterpret_cast<rgtp_session_t*>(sessionPtr);
    
    int result = rgtp_session_wait_complete(session);
    return Napi::Number::New(env, result);
}

Napi::Value SessionGetStats(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    uint64_t sessionPtr = info[0].As<Napi::BigInt>().Uint64Value();
    rgtp_session_t* session = reinterpret_cast<rgtp_session_t*>(sessionPtr);
    
    rgtp_stats_t stats;
    int result = rgtp_session_get_stats(session, &stats);
    
    if (result != 0) {
        Napi::Error::New(env, "Failed to get session stats").ThrowAsJavaScriptException();
        return env.Null();
    }
    
    return StatsToJSObject(env, stats);
}

Napi::Value SessionDestroy(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    uint64_t sessionPtr = info[0].As<Napi::BigInt>().Uint64Value();
    rgtp_session_t* session = reinterpret_cast<rgtp_session_t*>(sessionPtr);
    
    rgtp_session_destroy(session);
    return env.Undefined();
}

// Client management functions
Napi::Value ClientCreate(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    rgtp_config_t* config = nullptr;
    if (info.Length() > 0 && info[0].IsObject()) {
        config = ParseConfig(info[0].As<Napi::Object>());
    }
    
    rgtp_client_t* client = rgtp_client_create(config);
    delete config; // Clean up the config
    
    if (!client) {
        Napi::Error::New(env, "Failed to create RGTP client").ThrowAsJavaScriptException();
        return env.Null();
    }
    
    return Napi::BigInt::New(env, reinterpret_cast<uint64_t>(client));
}

Napi::Value ClientPullToFile(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    uint64_t clientPtr = info[0].As<Napi::BigInt>().Uint64Value();
    std::string host = info[1].As<Napi::String>().Utf8Value();
    uint16_t port = info[2].As<Napi::Number>().Uint32Value();
    std::string filename = info[3].As<Napi::String>().Utf8Value();
    
    rgtp_client_t* client = reinterpret_cast<rgtp_client_t*>(clientPtr);
    int result = rgtp_client_pull_to_file(client, host.c_str(), port, filename.c_str());
    
    return Napi::Number::New(env, result);
}

Napi::Value ClientGetStats(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    uint64_t clientPtr = info[0].As<Napi::BigInt>().Uint64Value();
    rgtp_client_t* client = reinterpret_cast<rgtp_client_t*>(clientPtr);
    
    rgtp_stats_t stats;
    int result = rgtp_client_get_stats(client, &stats);
    
    if (result != 0) {
        Napi::Error::New(env, "Failed to get client stats").ThrowAsJavaScriptException();
        return env.Null();
    }
    
    return StatsToJSObject(env, stats);
}

Napi::Value ClientDestroy(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    uint64_t clientPtr = info[0].As<Napi::BigInt>().Uint64Value();
    rgtp_client_t* client = reinterpret_cast<rgtp_client_t*>(clientPtr);
    
    rgtp_client_destroy(client);
    return env.Undefined();
}

// Additional utility functions
Napi::Value SetExposureRate(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    uint64_t surfacePtr = info[0].As<Napi::BigInt>().Uint64Value();
    uint32_t rate = info[1].As<Napi::Number>().Uint32Value();
    
    rgtp_surface_t* surface = reinterpret_cast<rgtp_surface_t*>(surfacePtr);
    int result = rgtp_set_exposure_rate(surface, rate);
    
    return Napi::Number::New(env, result);
}

Napi::Value AdaptiveExposure(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    uint64_t surfacePtr = info[0].As<Napi::BigInt>().Uint64Value();
    rgtp_surface_t* surface = reinterpret_cast<rgtp_surface_t*>(surfacePtr);
    
    int result = rgtp_adaptive_exposure(surface);
    return Napi::Number::New(env, result);
}

Napi::Value GetExposureStatus(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    uint64_t surfacePtr = info[0].As<Napi::BigInt>().Uint64Value();
    rgtp_surface_t* surface = reinterpret_cast<rgtp_surface_t*>(surfacePtr);
    
    float completion_pct;
    int result = rgtp_get_exposure_status(surface, &completion_pct);
    
    if (result != 0) {
        return Napi::Number::New(env, -1.0);
    }
    
    return Napi::Number::New(env, completion_pct);
}

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

// Generate exposure ID
Napi::Value GenerateExposureID(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    uint64_t id[2];
    rgtp_generate_exposure_id(id);
    
    // Return as hex string
    char hex_str[33]; // 32 hex chars + null terminator
    sprintf(hex_str, "%016llx%016llx", (unsigned long long)id[0], (unsigned long long)id[1]);
    
    return Napi::String::New(env, hex_str);
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
    exports.Set(Napi::String::New(env, "generateExposureID"), Napi::Function::New(env, GenerateExposureID));
    
    // New session management functions
    exports.Set(Napi::String::New(env, "sessionCreate"), Napi::Function::New(env, SessionCreate));
    exports.Set(Napi::String::New(env, "sessionExposeFile"), Napi::Function::New(env, SessionExposeFile));
    exports.Set(Napi::String::New(env, "sessionWaitComplete"), Napi::Function::New(env, SessionWaitComplete));
    exports.Set(Napi::String::New(env, "sessionGetStats"), Napi::Function::New(env, SessionGetStats));
    exports.Set(Napi::String::New(env, "sessionDestroy"), Napi::Function::New(env, SessionDestroy));
    
    // New client management functions
    exports.Set(Napi::String::New(env, "clientCreate"), Napi::Function::New(env, ClientCreate));
    exports.Set(Napi::String::New(env, "clientPullToFile"), Napi::Function::New(env, ClientPullToFile));
    exports.Set(Napi::String::New(env, "clientGetStats"), Napi::Function::New(env, ClientGetStats));
    exports.Set(Napi::String::New(env, "clientDestroy"), Napi::Function::New(env, ClientDestroy));
    
    // Additional utility functions
    exports.Set(Napi::String::New(env, "setExposureRate"), Napi::Function::New(env, SetExposureRate));
    exports.Set(Napi::String::New(env, "adaptiveExposure"), Napi::Function::New(env, AdaptiveExposure));
    exports.Set(Napi::String::New(env, "getExposureStatus"), Napi::Function::New(env, GetExposureStatus));
    
    return exports;
}

NODE_API_MODULE(rgtp, Init)