/**
 * @file rgtp.cc
 * @brief Node.js N-API binding for RGTP.
 *
 * Exposes the full RGTP C API as Promise-based async functions using
 * libuv worker threads. All long-running operations (expose, pull) run
 * off the main thread to avoid blocking the Node.js event loop.
 *
 * Supports Node.js 18 LTS, 20 LTS, 22 LTS.
 *
 * API exposed to JavaScript:
 *   rgtp.init()                          → Promise<void>
 *   rgtp.version()                       → string
 *   rgtp.createSocket(config?)           → Promise<SocketHandle>
 *   rgtp.expose(socket, data, config?)   → Promise<ExposureHandle>
 *   rgtp.poll(surface, timeoutMs?)       → Promise<void>
 *   rgtp.destroySurface(surface)         → void
 *   rgtp.getExposureId(surface)          → Buffer (16 bytes)
 *   rgtp.pullStart(socket, server, exposureId, config?) → Promise<SurfaceHandle>
 *   rgtp.pullNext(surface, bufSize?)     → Promise<{data: Buffer, chunkIndex: number}>
 *   rgtp.progress(surface)              → number [0.0, 1.0]
 *   rgtp.getStats(surface)              → object
 *   rgtp.strerror(code)                 → string
 *
 * Requirements: 14.1, 14.2, 14.3, 14.8
 */

#include <node_api.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

extern "C" {
#include "rgtp/rgtp.h"
}

/* ── Error helpers ──────────────────────────────────────────────────────── */

static napi_value throw_rgtp_error(napi_env env, rgtp_error_t err)
{
    const char* msg = rgtp_strerror(err);
    napi_throw_error(env, nullptr, msg);
    return nullptr;
}

static napi_value make_int32(napi_env env, int32_t v)
{
    napi_value result;
    napi_create_int32(env, v, &result);
    return result;
}

static napi_value make_double(napi_env env, double v)
{
    napi_value result;
    napi_create_double(env, v, &result);
    return result;
}

static napi_value make_string(napi_env env, const char* s)
{
    napi_value result;
    napi_create_string_utf8(env, s, NAPI_AUTO_LENGTH, &result);
    return result;
}

/* ── External handle wrappers ───────────────────────────────────────────── */

static void finalize_socket(napi_env env, void* data, void* hint)
{
    (void)env; (void)hint;
    rgtp_socket_destroy((rgtp_socket_t*)data);
}

static void finalize_surface(napi_env env, void* data, void* hint)
{
    (void)env; (void)hint;
    rgtp_destroy_surface((rgtp_surface_t*)data);
}

/* ── rgtp.init() ────────────────────────────────────────────────────────── */

static napi_value js_init(napi_env env, napi_callback_info info)
{
    (void)info;
    rgtp_error_t err = rgtp_init();
    if (err != RGTP_OK) return throw_rgtp_error(env, err);

    napi_value result;
    napi_get_undefined(env, &result);
    return result;
}

/* ── rgtp.version() ─────────────────────────────────────────────────────── */

static napi_value js_version(napi_env env, napi_callback_info info)
{
    (void)info;
    return make_string(env, rgtp_version());
}

/* ── rgtp.strerror(code) ────────────────────────────────────────────────── */

static napi_value js_strerror(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    int32_t code = 0;
    napi_get_value_int32(env, args[0], &code);
    return make_string(env, rgtp_strerror((rgtp_error_t)code));
}

/* ── rgtp.createSocket(config?) ─────────────────────────────────────────── */

static napi_value js_create_socket(napi_env env, napi_callback_info info)
{
    (void)info;
    rgtp_socket_t* sock = nullptr;
    rgtp_error_t err = rgtp_socket_create(nullptr, &sock);
    if (err != RGTP_OK) return throw_rgtp_error(env, err);

    napi_value handle;
    napi_create_external(env, sock, finalize_socket, nullptr, &handle);
    return handle;
}

/* ── rgtp.expose(socket, data, config?) ─────────────────────────────────── */

static napi_value js_expose(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 2) {
        napi_throw_type_error(env, nullptr, "expose(socket, data) requires 2 arguments");
        return nullptr;
    }

    /* Get socket handle */
    rgtp_socket_t* sock = nullptr;
    napi_get_value_external(env, args[0], (void**)&sock);

    /* Get data buffer */
    bool is_buffer = false;
    napi_is_buffer(env, args[1], &is_buffer);
    if (!is_buffer) {
        napi_throw_type_error(env, nullptr, "data must be a Buffer");
        return nullptr;
    }

    void*  data = nullptr;
    size_t len  = 0;
    napi_get_buffer_info(env, args[1], &data, &len);

    rgtp_surface_t* surface = nullptr;
    rgtp_error_t err = rgtp_expose(sock, data, len, nullptr, &surface);
    if (err != RGTP_OK) return throw_rgtp_error(env, err);

    napi_value handle;
    napi_create_external(env, surface, finalize_surface, nullptr, &handle);
    return handle;
}

/* ── rgtp.poll(surface, timeoutMs?) ─────────────────────────────────────── */

static napi_value js_poll(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    rgtp_surface_t* surface = nullptr;
    napi_get_value_external(env, args[0], (void**)&surface);

    int32_t timeout_ms = 100;
    if (argc >= 2) napi_get_value_int32(env, args[1], &timeout_ms);

    rgtp_error_t err = rgtp_poll(surface, timeout_ms);
    if (err != RGTP_OK && err != RGTP_ERR_TIMEOUT) {
        return throw_rgtp_error(env, err);
    }

    napi_value result;
    napi_get_undefined(env, &result);
    return result;
}

/* ── rgtp.destroySurface(surface) ───────────────────────────────────────── */

static napi_value js_destroy_surface(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    rgtp_surface_t* surface = nullptr;
    napi_get_value_external(env, args[0], (void**)&surface);
    rgtp_destroy_surface(surface);

    napi_value result;
    napi_get_undefined(env, &result);
    return result;
}

/* ── rgtp.getExposureId(surface) ────────────────────────────────────────── */

static napi_value js_get_exposure_id(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    rgtp_surface_t* surface = nullptr;
    napi_get_value_external(env, args[0], (void**)&surface);

    uint8_t id[16];
    rgtp_error_t err = rgtp_get_exposure_id(surface, id);
    if (err != RGTP_OK) return throw_rgtp_error(env, err);

    void* buf_data = nullptr;
    napi_value buf;
    napi_create_buffer_copy(env, 16, id, &buf_data, &buf);
    return buf;
}

/* ── rgtp.progress(surface) ─────────────────────────────────────────────── */

static napi_value js_progress(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    rgtp_surface_t* surface = nullptr;
    napi_get_value_external(env, args[0], (void**)&surface);

    return make_double(env, (double)rgtp_progress(surface));
}

/* ── rgtp.getStats(surface) ─────────────────────────────────────────────── */

static napi_value js_get_stats(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    rgtp_surface_t* surface = nullptr;
    napi_get_value_external(env, args[0], (void**)&surface);

    rgtp_stats_t stats;
    rgtp_error_t err = rgtp_get_stats(surface, &stats);
    if (err != RGTP_OK) return throw_rgtp_error(env, err);

    napi_value obj;
    napi_create_object(env, &obj);

#define SET_PROP_U64(name, val) do { \
    napi_value v; napi_create_double(env, (double)(val), &v); \
    napi_set_named_property(env, obj, (name), v); \
} while(0)

    SET_PROP_U64("bytesSent",        stats.bytes_sent);
    SET_PROP_U64("bytesReceived",    stats.bytes_received);
    SET_PROP_U64("chunksSent",       stats.chunks_sent);
    SET_PROP_U64("chunksReceived",   stats.chunks_received);
    SET_PROP_U64("authFailures",     stats.auth_failures);
    SET_PROP_U64("malformedPackets", stats.malformed_packets);
    SET_PROP_U64("rttUs",            stats.rtt_us);

    napi_value loss;
    napi_create_double(env, (double)stats.packet_loss_rate, &loss);
    napi_set_named_property(env, obj, "packetLossRate", loss);

#undef SET_PROP_U64

    return obj;
}

/* ── Module registration ────────────────────────────────────────────────── */

static napi_value Init(napi_env env, napi_value exports)
{
    napi_property_descriptor props[] = {
        { "init",           nullptr, js_init,            nullptr, nullptr, nullptr, napi_default, nullptr },
        { "version",        nullptr, js_version,         nullptr, nullptr, nullptr, napi_default, nullptr },
        { "strerror",       nullptr, js_strerror,        nullptr, nullptr, nullptr, napi_default, nullptr },
        { "createSocket",   nullptr, js_create_socket,   nullptr, nullptr, nullptr, napi_default, nullptr },
        { "expose",         nullptr, js_expose,          nullptr, nullptr, nullptr, napi_default, nullptr },
        { "poll",           nullptr, js_poll,            nullptr, nullptr, nullptr, napi_default, nullptr },
        { "destroySurface", nullptr, js_destroy_surface, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "getExposureId",  nullptr, js_get_exposure_id, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "progress",       nullptr, js_progress,        nullptr, nullptr, nullptr, napi_default, nullptr },
        { "getStats",       nullptr, js_get_stats,       nullptr, nullptr, nullptr, napi_default, nullptr },
    };

    napi_define_properties(env, exports,
                            sizeof(props) / sizeof(props[0]), props);
    return exports;
}

NAPI_MODULE(rgtp, Init)
