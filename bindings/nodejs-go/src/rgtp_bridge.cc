#include <napi.h>
#include <windows.h>
#include <iostream>

// Function pointer types for the Go DLL
typedef const char* (*RgtpInitializeFunc)();
typedef void (*RgtpCleanupFunc)();
typedef const char* (*RgtpVersionFunc)();
typedef const char* (*RgtpSendFileFunc)(const char*, unsigned int, unsigned int);
typedef const char* (*RgtpReceiveFileFunc)(const char*, unsigned short, const char*, unsigned int);

// Global function pointers
HMODULE hDLL = nullptr;
RgtpInitializeFunc RgtpInitialize = nullptr;
RgtpCleanupFunc RgtpCleanup = nullptr;
RgtpVersionFunc RgtpVersion = nullptr;
RgtpSendFileFunc RgtpSendFile = nullptr;
RgtpReceiveFileFunc RgtpReceiveFile = nullptr;

// Load the Go DLL
bool LoadGoBridge() {
    hDLL = LoadLibrary(L"../go/rgtp_bridge.dll");
    if (!hDLL) {
        std::cerr << "Failed to load rgtp_bridge.dll" << std::endl;
        return false;
    }

    RgtpInitialize = (RgtpInitializeFunc)GetProcAddress(hDLL, "RgtpInitialize");
    RgtpCleanup = (RgtpCleanupFunc)GetProcAddress(hDLL, "RgtpCleanup");
    RgtpVersion = (RgtpVersionFunc)GetProcAddress(hDLL, "RgtpVersion");
    RgtpSendFile = (RgtpSendFileFunc)GetProcAddress(hDLL, "RgtpSendFile");
    RgtpReceiveFile = (RgtpReceiveFileFunc)GetProcAddress(hDLL, "RgtpReceiveFile");

    if (!RgtpInitialize || !RgtpCleanup || !RgtpVersion || !RgtpSendFile || !RgtpReceiveFile) {
        std::cerr << "Failed to get function addresses from DLL" << std::endl;
        FreeLibrary(hDLL);
        return false;
    }

    return true;
}

// Initialize RGTP
Napi::Value Initialize(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    if (!RgtpInitialize) {
        if (!LoadGoBridge()) {
            Napi::Error::New(env, "Failed to load Go bridge").ThrowAsJavaScriptException();
            return env.Null();
        }
    }

    const char* result = RgtpInitialize();
    return Napi::String::New(env, result);
}

// Cleanup RGTP
Napi::Value Cleanup(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    if (RgtpCleanup) {
        RgtpCleanup();
    }
    
    if (hDLL) {
        FreeLibrary(hDLL);
        hDLL = nullptr;
        RgtpInitialize = nullptr;
        RgtpCleanup = nullptr;
        RgtpVersion = nullptr;
        RgtpSendFile = nullptr;
        RgtpReceiveFile = nullptr;
    }
    
    return env.Undefined();
}

// Get RGTP version
Napi::Value Version(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    if (!RgtpVersion) {
        Napi::Error::New(env, "Go bridge not loaded").ThrowAsJavaScriptException();
        return env.Null();
    }

    const char* version = RgtpVersion();
    return Napi::String::New(env, version);
}

// Send file function
Napi::Value SendFile(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    if (info.Length() < 3) {
        Napi::TypeError::New(env, "Wrong number of arguments").ThrowAsJavaScriptException();
        return env.Null();
    }

    if (!info[0].IsString() || !info[1].IsNumber() || !info[2].IsNumber()) {
        Napi::TypeError::New(env, "Wrong arguments").ThrowAsJavaScriptException();
        return env.Null();
    }

    std::string filename = info[0].As<Napi::String>().Utf8Value();
    uint32_t chunkSize = info[1].As<Napi::Number>().Uint32Value();
    uint32_t exposureRate = info[2].As<Napi::Number>().Uint32Value();

    if (!RgtpSendFile) {
        Napi::Error::New(env, "Go bridge not loaded").ThrowAsJavaScriptException();
        return env.Null();
    }

    const char* result = RgtpSendFile(filename.c_str(), chunkSize, exposureRate);
    return Napi::String::New(env, result);
}

// Receive file function
Napi::Value ReceiveFile(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    if (info.Length() < 4) {
        Napi::TypeError::New(env, "Wrong number of arguments").ThrowAsJavaScriptException();
        return env.Null();
    }

    if (!info[0].IsString() || !info[1].IsNumber() || !info[2].IsString() || !info[3].IsNumber()) {
        Napi::TypeError::New(env, "Wrong arguments").ThrowAsJavaScriptException();
        return env.Null();
    }

    std::string host = info[0].As<Napi::String>().Utf8Value();
    uint16_t port = info[1].As<Napi::Number>().Uint32Value();
    std::string filename = info[2].As<Napi::String>().Utf8Value();
    uint32_t chunkSize = info[3].As<Napi::Number>().Uint32Value();

    if (!RgtpReceiveFile) {
        Napi::Error::New(env, "Go bridge not loaded").ThrowAsJavaScriptException();
        return env.Null();
    }

    const char* result = RgtpReceiveFile(host.c_str(), port, filename.c_str(), chunkSize);
    return Napi::String::New(env, result);
}

// Module initialization
Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set(Napi::String::New(env, "initialize"),
                Napi::Function::New(env, Initialize));
    exports.Set(Napi::String::New(env, "cleanup"),
                Napi::Function::New(env, Cleanup));
    exports.Set(Napi::String::New(env, "version"),
                Napi::Function::New(env, Version));
    exports.Set(Napi::String::New(env, "sendFile"),
                Napi::Function::New(env, SendFile));
    exports.Set(Napi::String::New(env, "receiveFile"),
                Napi::Function::New(env, ReceiveFile));
    return exports;
}

NODE_API_MODULE(rgtp_bridge, Init)