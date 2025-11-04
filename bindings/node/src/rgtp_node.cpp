/**
 * @file rgtp_node.cpp
 * @brief Node.js N-API bindings for Red Giant Transport Protocol (RGTP)
 * 
 * This file implements Node.js bindings for RGTP using N-API (Node-API).
 * It provides a JavaScript interface to the Layer 4 RGTP transport protocol
 * while maintaining full protocol functionality.
 * 
 * @copyright MIT License
 */

#include <napi.h>
#include <memory>
#include <string>
#include <map>
#include <mutex>
#include <thread>
#include <chrono>

extern "C" {
#include "rgtp/rgtp_sdk.h"
}

// Forward declarations
class RGTPSession;
class RGTPClient;

// Global callback registry for thread safety
static std::map<void*, std::shared_ptr<Napi::ThreadSafeFunction>> g_progress_callbacks;
static std::map<void*, std::shared_ptr<Napi::ThreadSafeFunction>> g_error_callbacks;
static std::mutex g_callback_mutex;

// Utility functions
std::string ErrorCodeToString(int errorCode) {
    const char* errorStr = rgtp_error_string(errorCode);
    return errorStr ? std::string(errorStr) : "Unknown error";
}

// Progress callback bridge
void ProgressCallbackBridge(size_t bytesTransferred, size_t totalBytes, void* userData) {
    std::lock_guard<std::mutex> lock(g_callback_mutex);
    auto it = g_progress_callbacks.find(userData);
    if (it != g_progress_callbacks.end() && it->second) {
        auto callback = it->second;
        
        // Call JavaScript callback in a thread-safe manner
        callback->BlockingCall([bytesTransferred, totalBytes](Napi::Env env, Napi::Function jsCallback) {
            jsCallback.Call({
                Napi::Number::New(env, static_cast<double>(bytesTransferred)),
                Napi::Number::New(env, static_cast<double>(totalBytes))
            });
        });
    }
}

// Error callback bridge
void ErrorCallbackBridge(int errorCode, const char* errorMessage, void* userData) {
    std::lock_guard<std::mutex> lock(g_callback_mutex);
    auto it = g_error_callbacks.find(userData);
    if (it != g_error_callbacks.end() && it->second) {
        auto callback = it->second;
        
        // Call JavaScript callback in a thread-safe manner
        callback->BlockingCall([errorCode, errorMessage](Napi::Env env, Napi::Function jsCallback) {
            jsCallback.Call({
                Napi::Number::New(env, errorCode),
                Napi::String::New(env, errorMessage ? errorMessage : "Unknown error")
            });
        });
    }
}

// RGTP Session wrapper class
class RGTPSession : public Napi::ObjectWrap<RGTPSession> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports) {
        Napi::Function func = DefineClass(env, "Session", {
            InstanceMethod("exposeFile", &RGTPSession::ExposeFile),
            InstanceMethod("waitComplete", &RGTPSession::WaitComplete),
            InstanceMethod("getStats", &RGTPSession::GetStats),
            InstanceMethod("cancel", &RGTPSession::Cancel),
            InstanceMethod("close", &RGTPSession::Close)
        });

        constructor = Napi::Persistent(func);
        constructor.SuppressDestruct();
        exports.Set("Session", func);
        return exports;
    }

    RGTPSession(const Napi::CallbackInfo& info) : Napi::ObjectWrap<RGTPSession>(info) {
        Napi::Env env = info.Env();

        // Initialize RGTP library if not already done
        static bool initialized = false;
        if (!initialized) {
            if (rgtp_init() != 0) {
                Napi::Error::New(env, "Failed to initialize RGTP library").ThrowAsJavaScriptException();
                return;
            }
            initialized = true;
        }

        rgtp_config_t config;
        rgtp_config_default(&config);

        // Parse configuration if provided
        if (info.Length() > 0 && info[0].IsObject()) {
            Napi::Object configObj = info[0].As<Napi::Object>();
            
            if (configObj.Has("chunkSize")) {
                config.chunk_size = configObj.Get("chunkSize").As<Napi::Number>().Uint32Value();
            }
            if (configObj.Has("exposureRate")) {
                config.exposure_rate = configObj.Get("exposureRate").As<Napi::Number>().Uint32Value();
            }
            if (configObj.Has("adaptiveMode")) {
                config.adaptive_mode = configObj.Get("adaptiveMode").As<Napi::Boolean>().Value();
            }
            if (configObj.Has("port")) {
                config.port = static_cast<uint16_t>(configObj.Get("port").As<Napi::Number>().Uint32Value());
            }
            if (configObj.Has("timeout")) {
                config.timeout_ms = configObj.Get("timeout").As<Napi::Number>().Int32Value();
            }

            // Set up callbacks
            if (configObj.Has("onProgress") && configObj.Get("onProgress").IsFunction()) {
                Napi::Function progressCallback = configObj.Get("onProgress").As<Napi::Function>();
                auto tsfn = Napi::ThreadSafeFunction::New(
                    env, progressCallback, "ProgressCallback", 0, 1);
                
                std::lock_guard<std::mutex> lock(g_callback_mutex);
                g_progress_callbacks[this] = std::make_shared<Napi::ThreadSafeFunction>(std::move(tsfn));
                
                config.progress_cb = ProgressCallbackBridge;
                config.user_data = this;
            }

            if (configObj.Has("onError") && configObj.Get("onError").IsFunction()) {
                Napi::Function errorCallback = configObj.Get("onError").As<Napi::Function>();
                auto tsfn = Napi::ThreadSafeFunction::New(
                    env, errorCallback, "ErrorCallback", 0, 1);
                
                std::lock_guard<std::mutex> lock(g_callback_mutex);
                g_error_callbacks[this] = std::make_shared<Napi::ThreadSafeFunction>(std::move(tsfn));
                
                config.error_cb = ErrorCallbackBridge;
                config.user_data = this;
            }
        }

        // Create RGTP session
        session_ = rgtp_session_create_with_config(&config);
        if (!session_) {
            Napi::Error::New(env, "Failed to create RGTP session").ThrowAsJavaScriptException();
            return;
        }
    }

    ~RGTPSession() {
        Close();
    }

private:
    static Napi::FunctionReference constructor;
    rgtp_session_t* session_ = nullptr;

    void Close() {
        if (session_) {
            // Clean up callbacks
            std::lock_guard<std::mutex> lock(g_callback_mutex);
            
            auto progressIt = g_progress_callbacks.find(this);
            if (progressIt != g_progress_callbacks.end()) {
                progressIt->second->Release();
                g_progress_callbacks.erase(progressIt);
            }
            
            auto errorIt = g_error_callbacks.find(this);
            if (errorIt != g_error_callbacks.end()) {
                errorIt->second->Release();
                g_error_callbacks.erase(errorIt);
            }

            rgtp_session_destroy(session_);
            session_ = nullptr;
        }
    }

    Napi::Value ExposeFile(const Napi::CallbackInfo& info) {
        Napi::Env env = info.Env();

        if (info.Length() < 1 || !info[0].IsString()) {
            Napi::TypeError::New(env, "Expected filename as string").ThrowAsJavaScriptException();
            return env.Null();
        }

        if (!session_) {
            Napi::Error::New(env, "Session is closed").ThrowAsJavaScriptException();
            return env.Null();
        }

        std::string filename = info[0].As<Napi::String>().Utf8Value();
        
        int result = rgtp_session_expose_file(session_, filename.c_str());
        if (result != 0) {
            std::string errorMsg = "Failed to expose file: " + filename + " (" + ErrorCodeToString(result) + ")";
            Napi::Error::New(env, errorMsg).ThrowAsJavaScriptException();
            return env.Null();
        }

        return env.Undefined();
    }

    Napi::Value WaitComplete(const Napi::CallbackInfo& info) {
        Napi::Env env = info.Env();

        if (!session_) {
            Napi::Error::New(env, "Session is closed").ThrowAsJavaScriptException();
            return env.Null();
        }

        // Run in async worker to avoid blocking the event loop
        auto promise = Napi::Promise::Deferred::New(env);
        
        std::thread([this, promise]() mutable {
            int result = rgtp_session_wait_complete(session_);
            
            promise.Env().ExecuteCallback([promise, result](Napi::Env env) mutable {
                if (result == 0) {
                    promise.Resolve(env.Undefined());
                } else {
                    std::string errorMsg = "Wait complete failed: " + ErrorCodeToString(result);
                    promise.Reject(Napi::Error::New(env, errorMsg).Value());
                }
            });
        }).detach();

        return promise.Promise();
    }

    Napi::Value GetStats(const Napi::CallbackInfo& info) {
        Napi::Env env = info.Env();

        if (!session_) {
            Napi::Error::New(env, "Session is closed").ThrowAsJavaScriptException();
            return env.Null();
        }

        rgtp_stats_t stats;
        int result = rgtp_session_get_stats(session_, &stats);
        if (result != 0) {
            std::string errorMsg = "Failed to get statistics: " + ErrorCodeToString(result);
            Napi::Error::New(env, errorMsg).ThrowAsJavaScriptException();
            return env.Null();
        }

        Napi::Object statsObj = Napi::Object::New(env);
        statsObj.Set("bytesTransferred", Napi::Number::New(env, static_cast<double>(stats.bytes_transferred)));
        statsObj.Set("totalBytes", Napi::Number::New(env, static_cast<double>(stats.total_bytes)));
        statsObj.Set("throughputMbps", Napi::Number::New(env, stats.throughput_mbps));
        statsObj.Set("avgThroughputMbps", Napi::Number::New(env, stats.avg_throughput_mbps));
        statsObj.Set("chunksTransferred", Napi::Number::New(env, stats.chunks_transferred));
        statsObj.Set("totalChunks", Napi::Number::New(env, stats.total_chunks));
        statsObj.Set("retransmissions", Napi::Number::New(env, stats.retransmissions));
        statsObj.Set("completionPercent", Napi::Number::New(env, stats.completion_percent));
        statsObj.Set("elapsedMs", Napi::Number::New(env, static_cast<double>(stats.elapsed_ms)));
        statsObj.Set("estimatedRemainingMs", Napi::Number::New(env, static_cast<double>(stats.estimated_remaining_ms)));

        return statsObj;
    }

    Napi::Value Cancel(const Napi::CallbackInfo& info) {
        Napi::Env env = info.Env();

        if (!session_) {
            Napi::Error::New(env, "Session is closed").ThrowAsJavaScriptException();
            return env.Null();
        }

        int result = rgtp_session_cancel(session_);
        if (result != 0) {
            std::string errorMsg = "Failed to cancel session: " + ErrorCodeToString(result);
            Napi::Error::New(env, errorMsg).ThrowAsJavaScriptException();
            return env.Null();
        }

        return env.Undefined();
    }

    Napi::Value Close(const Napi::CallbackInfo& info) {
        Close();
        return info.Env().Undefined();
    }
};

// RGTP Client wrapper class
class RGTPClient : public Napi::ObjectWrap<RGTPClient> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports) {
        Napi::Function func = DefineClass(env, "Client", {
            InstanceMethod("pullToFile", &RGTPClient::PullToFile),
            InstanceMethod("getStats", &RGTPClient::GetStats),
            InstanceMethod("cancel", &RGTPClient::Cancel),
            InstanceMethod("close", &RGTPClient::Close)
        });

        constructor = Napi::Persistent(func);
        constructor.SuppressDestruct();
        exports.Set("Client", func);
        return exports;
    }

    RGTPClient(const Napi::CallbackInfo& info) : Napi::ObjectWrap<RGTPClient>(info) {
        Napi::Env env = info.Env();

        rgtp_config_t config;
        rgtp_config_default(&config);

        // Parse configuration if provided
        if (info.Length() > 0 && info[0].IsObject()) {
            Napi::Object configObj = info[0].As<Napi::Object>();
            
            if (configObj.Has("chunkSize")) {
                config.chunk_size = configObj.Get("chunkSize").As<Napi::Number>().Uint32Value();
            }
            if (configObj.Has("timeout")) {
                config.timeout_ms = configObj.Get("timeout").As<Napi::Number>().Int32Value();
            }
            if (configObj.Has("adaptiveMode")) {
                config.adaptive_mode = configObj.Get("adaptiveMode").As<Napi::Boolean>().Value();
            }

            // Set up callbacks
            if (configObj.Has("onProgress") && configObj.Get("onProgress").IsFunction()) {
                Napi::Function progressCallback = configObj.Get("onProgress").As<Napi::Function>();
                auto tsfn = Napi::ThreadSafeFunction::New(
                    env, progressCallback, "ProgressCallback", 0, 1);
                
                std::lock_guard<std::mutex> lock(g_callback_mutex);
                g_progress_callbacks[this] = std::make_shared<Napi::ThreadSafeFunction>(std::move(tsfn));
                
                config.progress_cb = ProgressCallbackBridge;
                config.user_data = this;
            }

            if (configObj.Has("onError") && configObj.Get("onError").IsFunction()) {
                Napi::Function errorCallback = configObj.Get("onError").As<Napi::Function>();
                auto tsfn = Napi::ThreadSafeFunction::New(
                    env, errorCallback, "ErrorCallback", 0, 1);
                
                std::lock_guard<std::mutex> lock(g_callback_mutex);
                g_error_callbacks[this] = std::make_shared<Napi::ThreadSafeFunction>(std::move(tsfn));
                
                config.error_cb = ErrorCallbackBridge;
                config.user_data = this;
            }
        }

        // Create RGTP client
        client_ = rgtp_client_create_with_config(&config);
        if (!client_) {
            Napi::Error::New(env, "Failed to create RGTP client").ThrowAsJavaScriptException();
            return;
        }
    }

    ~RGTPClient() {
        Close();
    }

private:
    static Napi::FunctionReference constructor;
    rgtp_client_t* client_ = nullptr;

    void Close() {
        if (client_) {
            // Clean up callbacks
            std::lock_guard<std::mutex> lock(g_callback_mutex);
            
            auto progressIt = g_progress_callbacks.find(this);
            if (progressIt != g_progress_callbacks.end()) {
                progressIt->second->Release();
                g_progress_callbacks.erase(progressIt);
            }
            
            auto errorIt = g_error_callbacks.find(this);
            if (errorIt != g_error_callbacks.end()) {
                errorIt->second->Release();
                g_error_callbacks.erase(errorIt);
            }

            rgtp_client_destroy(client_);
            client_ = nullptr;
        }
    }

    Napi::Value PullToFile(const Napi::CallbackInfo& info) {
        Napi::Env env = info.Env();

        if (info.Length() < 3 || !info[0].IsString() || !info[1].IsNumber() || !info[2].IsString()) {
            Napi::TypeError::New(env, "Expected (host: string, port: number, filename: string)").ThrowAsJavaScriptException();
            return env.Null();
        }

        if (!client_) {
            Napi::Error::New(env, "Client is closed").ThrowAsJavaScriptException();
            return env.Null();
        }

        std::string host = info[0].As<Napi::String>().Utf8Value();
        uint16_t port = static_cast<uint16_t>(info[1].As<Napi::Number>().Uint32Value());
        std::string filename = info[2].As<Napi::String>().Utf8Value();

        // Run in async worker to avoid blocking the event loop
        auto promise = Napi::Promise::Deferred::New(env);
        
        std::thread([this, promise, host, port, filename]() mutable {
            int result = rgtp_client_pull_to_file(client_, host.c_str(), port, filename.c_str());
            
            promise.Env().ExecuteCallback([promise, result, host, port](Napi::Env env) mutable {
                if (result == 0) {
                    promise.Resolve(env.Undefined());
                } else {
                    std::string errorMsg = "Failed to pull from " + host + ":" + std::to_string(port) + 
                                         " (" + ErrorCodeToString(result) + ")";
                    promise.Reject(Napi::Error::New(env, errorMsg).Value());
                }
            });
        }).detach();

        return promise.Promise();
    }

    Napi::Value GetStats(const Napi::CallbackInfo& info) {
        Napi::Env env = info.Env();

        if (!client_) {
            Napi::Error::New(env, "Client is closed").ThrowAsJavaScriptException();
            return env.Null();
        }

        rgtp_stats_t stats;
        int result = rgtp_client_get_stats(client_, &stats);
        if (result != 0) {
            std::string errorMsg = "Failed to get statistics: " + ErrorCodeToString(result);
            Napi::Error::New(env, errorMsg).ThrowAsJavaScriptException();
            return env.Null();
        }

        Napi::Object statsObj = Napi::Object::New(env);
        statsObj.Set("bytesTransferred", Napi::Number::New(env, static_cast<double>(stats.bytes_transferred)));
        statsObj.Set("totalBytes", Napi::Number::New(env, static_cast<double>(stats.total_bytes)));
        statsObj.Set("throughputMbps", Napi::Number::New(env, stats.throughput_mbps));
        statsObj.Set("avgThroughputMbps", Napi::Number::New(env, stats.avg_throughput_mbps));
        statsObj.Set("chunksTransferred", Napi::Number::New(env, stats.chunks_transferred));
        statsObj.Set("totalChunks", Napi::Number::New(env, stats.total_chunks));
        statsObj.Set("retransmissions", Napi::Number::New(env, stats.retransmissions));
        statsObj.Set("completionPercent", Napi::Number::New(env, stats.completion_percent));
        statsObj.Set("elapsedMs", Napi::Number::New(env, static_cast<double>(stats.elapsed_ms)));
        statsObj.Set("estimatedRemainingMs", Napi::Number::New(env, static_cast<double>(stats.estimated_remaining_ms)));

        return statsObj;
    }

    Napi::Value Cancel(const Napi::CallbackInfo& info) {
        Napi::Env env = info.Env();

        if (!client_) {
            Napi::Error::New(env, "Client is closed").ThrowAsJavaScriptException();
            return env.Null();
        }

        int result = rgtp_client_cancel(client_);
        if (result != 0) {
            std::string errorMsg = "Failed to cancel client: " + ErrorCodeToString(result);
            Napi::Error::New(env, errorMsg).ThrowAsJavaScriptException();
            return env.Null();
        }

        return env.Undefined();
    }

    Napi::Value Close(const Napi::CallbackInfo& info) {
        Close();
        return info.Env().Undefined();
    }
};

// Static member definitions
Napi::FunctionReference RGTPSession::constructor;
Napi::FunctionReference RGTPClient::constructor;

// Utility functions
Napi::Value GetVersion(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    const char* version = rgtp_version();
    return Napi::String::New(env, version ? version : "1.0.0");
}

// Configuration helpers
Napi::Value CreateLANConfig(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    Napi::Object config = Napi::Object::New(env);
    config.Set("chunkSize", Napi::Number::New(env, 1024 * 1024)); // 1MB
    config.Set("exposureRate", Napi::Number::New(env, 10000));    // High rate for LAN
    config.Set("adaptiveMode", Napi::Boolean::New(env, true));
    config.Set("timeout", Napi::Number::New(env, 30000));         // 30 seconds
    
    return config;
}

Napi::Value CreateWANConfig(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    Napi::Object config = Napi::Object::New(env);
    config.Set("chunkSize", Napi::Number::New(env, 64 * 1024));   // 64KB
    config.Set("exposureRate", Napi::Number::New(env, 1000));     // Conservative for WAN
    config.Set("adaptiveMode", Napi::Boolean::New(env, true));
    config.Set("timeout", Napi::Number::New(env, 60000));         // 60 seconds
    
    return config;
}

Napi::Value CreateMobileConfig(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    Napi::Object config = Napi::Object::New(env);
    config.Set("chunkSize", Napi::Number::New(env, 16 * 1024));   // 16KB
    config.Set("exposureRate", Napi::Number::New(env, 100));      // Very conservative
    config.Set("adaptiveMode", Napi::Boolean::New(env, true));
    config.Set("timeout", Napi::Number::New(env, 120000));        // 2 minutes
    
    return config;
}

// Module initialization
Napi::Object Init(Napi::Env env, Napi::Object exports) {
    // Initialize classes
    RGTPSession::Init(env, exports);
    RGTPClient::Init(env, exports);
    
    // Export utility functions
    exports.Set("getVersion", Napi::Function::New(env, GetVersion));
    exports.Set("createLANConfig", Napi::Function::New(env, CreateLANConfig));
    exports.Set("createWANConfig", Napi::Function::New(env, CreateWANConfig));
    exports.Set("createMobileConfig", Napi::Function::New(env, CreateMobileConfig));
    
    return exports;
}

NODE_API_MODULE(rgtp_native, Init)