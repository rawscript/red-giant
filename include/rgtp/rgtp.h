/**
 * @file rgtp.h
 * @brief Red Giant Transport Protocol — public C17 API
 *
 * RGTP is a stateless, receiver-driven, chunk-based, pre-encrypted,
 * Merkle-verified, FEC-protected data transport over UDP and raw Ethernet.
 *
 * ABI stability contract:
 *  - All handle types are opaque (forward-declared only).
 *  - rgtp_config_t is versioned; new fields are appended only.
 *  - Shared library exports RGTP_1.0 symbol version map.
 */

#ifndef RGTP_H
#define RGTP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ── Platform socket includes ───────────────────────────────────────────── */
#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#else
#  include <sys/socket.h>   /* struct sockaddr_storage */
#  include <netinet/in.h>
#endif

/* ── Version macros ─────────────────────────────────────────────────────── */
#ifndef RGTP_VERSION_MAJOR
#  define RGTP_VERSION_MAJOR 1
#endif
#ifndef RGTP_VERSION_MINOR
#  define RGTP_VERSION_MINOR 0
#endif
#ifndef RGTP_VERSION_PATCH
#  define RGTP_VERSION_PATCH 0
#endif
#ifndef RGTP_VERSION_STRING
#  define RGTP_VERSION_STRING "1.0.0"
#endif

/* ── Memory profile constants ───────────────────────────────────────────── */
#define RGTP_MEM_FULL     0u   /**< Full heap allocation (default) */
#define RGTP_MEM_EMBEDDED 1u   /**< Arena-based, bounded allocation */
#define RGTP_MEM_MINIMAL  2u   /**< Minimal footprint, no optional features */

/* ── Log level constants ────────────────────────────────────────────────── */
#define RGTP_LOG_ERROR 0
#define RGTP_LOG_WARN  1
#define RGTP_LOG_INFO  2
#define RGTP_LOG_DEBUG 3

/* ── Satellite mode constants ────────────────────────────────────────────── */
#define RGTP_SPACE_LINK_NONE      0u   /**< No space link (default) */
#define RGTP_SPACE_LINK_UHF       1u   /**< UHF band (300 MHz - 3 GHz) */
#define RGTP_SPACE_LINK_SBAND     2u   /**< S-band (2-4 GHz) */
#define RGTP_SPACE_LINK_XBAND     3u   /**< X-band (8-12 GHz) */
#define RGTP_SPACE_LINK_KABAND    4u   /**< Ka-band (26-40 GHz) */
#define RGTP_SPACE_LINK_OPTICAL   5u   /**< Optical (laser) communications */

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Error codes
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Return codes for all RGTP API functions.
 *
 * Every public function returns rgtp_error_t. RGTP_OK (0) indicates success.
 * All error values are negative.
 */
typedef enum rgtp_error {
    RGTP_OK                  =  0,  /**< Success */
    RGTP_ERR_NOMEM           = -1,  /**< Heap allocation failed */
    RGTP_ERR_INVALID_ARG     = -2,  /**< NULL pointer or out-of-range argument */
    RGTP_ERR_SOCKET          = -3,  /**< OS socket operation failed */
    RGTP_ERR_CRYPTO_INIT     = -4,  /**< Crypto library initialisation failed */
    RGTP_ERR_ENCRYPT         = -5,  /**< AEAD encryption failed */
    RGTP_ERR_DECRYPT         = -6,  /**< AEAD decryption failed */
    RGTP_ERR_AUTH_FAIL       = -7,  /**< AEAD authentication tag mismatch */
    RGTP_ERR_MERKLE_FAIL     = -8,  /**< Merkle proof verification failed */
    RGTP_ERR_FEC_FAIL        = -9,  /**< Reed-Solomon decoding failed */
    RGTP_ERR_TRUNCATED       = -10, /**< Packet shorter than declared length */
    RGTP_ERR_CHUNK_INDEX_OOB = -11, /**< Chunk index >= chunk_count */
    RGTP_ERR_TIMEOUT         = -12, /**< Operation timed out */
    RGTP_ERR_RATE_LIMITED    = -13, /**< Pull request rate limit exceeded */
    RGTP_ERR_NOT_SUPPORTED   = -14, /**< Feature not available on this platform/build */
    RGTP_ERR_INTERNAL        = -15, /**< Internal invariant violation */
    RGTP_ERR_SATELLITE_NO_CONTACT = -16, /**< No active satellite contact window */
} rgtp_error_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Opaque handle types
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief Opaque surface handle — represents an exposer or puller session. */
typedef struct rgtp_surface_s rgtp_surface_t;

/** @brief Opaque socket handle — wraps the OS socket and I/O backend. */
typedef struct rgtp_socket_s  rgtp_socket_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Allocator hook
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Custom allocator interface.
 *
 * Pass a populated rgtp_allocator_t to rgtp_set_allocator() to replace the
 * default system malloc/free. Set both pointers to NULL to restore defaults.
 */
typedef struct rgtp_allocator {
    void* (*alloc)(size_t size, void* ctx); /**< Allocate @p size bytes */
    void  (*free)(void* ptr,   void* ctx);  /**< Free a previously allocated block */
    void* ctx;                              /**< Opaque context passed to both functions */
} rgtp_allocator_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Configuration
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Per-surface configuration.
 *
 * Zero-initialise and set only the fields you need; unset fields use defaults.
 * New fields are always appended — never reorder or remove existing fields.
 */
typedef struct rgtp_config {
    uint32_t         chunk_size;       /**< Chunk size in bytes (0 = auto: 1200 UDP / 1400 raw Eth) */
    uint32_t         window_size;      /**< Initial pull window in chunks (0 = default 64) */

    /* FEC */
    bool             fec_enabled;      /**< Enable Reed-Solomon FEC */
    uint8_t          fec_k;            /**< RS data symbols per block (0 = default 223) */
    uint8_t          fec_n;            /**< RS total symbols per block (0 = default 255) */

    /* Integrity */
    bool             merkle_proofs;    /**< Include per-chunk Merkle proofs in responses */

    /* Raw Ethernet mode */
    bool             raw_ethernet;     /**< Use AF_PACKET raw Ethernet instead of UDP */
    uint8_t          eth_iface[16];    /**< Interface name for raw Ethernet (e.g. "eth0") */
    uint16_t         eth_vlan_id;      /**< 802.1Q VLAN ID (0 = untagged) */
    uint8_t          eth_vlan_pcp;     /**< 802.1Q Priority Code Point (0–7) for TSN */

    /* Network */
    uint16_t         port;             /**< UDP port (0 = auto-assign) */
    int              timeout_ms;       /**< Operation timeout in ms (-1 = infinite) */

    /* Memory */
    uint32_t         memory_profile;   /**< RGTP_MEM_FULL / RGTP_MEM_EMBEDDED / RGTP_MEM_MINIMAL */

    /* Satellite communications (optional) */
    bool             satellite_mode;   /**< Enable satellite-specific optimizations */
    uint32_t         max_rtt_ms;       /**< Maximum round-trip time in milliseconds (for space links) */
    float            link_asymmetry;   /**< Downlink/uplink bandwidth ratio (e.g., 10.0 = 10x faster downlink) */
    bool             store_and_forward;/**< Enable store-and-forward for intermittent connectivity */
    uint8_t          space_link_type;  /**< RGTP_SPACE_LINK_* constants */
    
    /* CCSDS/space protocol options */
    bool             ccsds_tm;         /**< Enable CCSDS Telemetry (TM) protocol */
    bool             ccsds_tc;         /**< Enable CCSDS Telecommand (TC) protocol */
    bool             ccsds_aos;        /**< Enable CCSDS Advanced Orbiting Systems (AOS) */
    bool             ccsds_cfdp;       /**< Enable CCSDS File Delivery Protocol (CFDP) */
    uint16_t         apid;             /**< CCSDS Application Process ID (APID) */
    uint8_t          spacecraft_id;    /**< Spacecraft identifier (0-255) */

    /* Ground station information */
    char             ground_station[32]; /**< Ground station identifier */
    uint32_t         contact_window_start; /**< UTC timestamp of contact start */
    uint32_t         contact_window_end;   /**< UTC timestamp of contact end */

    /* Link quality parameters */
    float            min_snr_db;       /**< Minimum acceptable SNR in dB */
    float            max_ber;          /**< Maximum acceptable bit error rate */
    uint32_t         doppler_shift_hz; /**< Expected Doppler shift in Hz */

    /* Custom allocator (NULL = use global allocator set via rgtp_set_allocator) */
    rgtp_allocator_t allocator;
} rgtp_config_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Statistics
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief Per-surface transfer statistics. */
typedef struct rgtp_stats {
    uint64_t bytes_sent;           /**< Total bytes sent (exposer) */
    uint64_t bytes_received;       /**< Total bytes received (puller) */
    uint32_t chunks_sent;          /**< Total chunks sent */
    uint32_t chunks_received;      /**< Total chunks received */
    uint32_t auth_failures;        /**< AEAD tag verification failures */
    uint32_t malformed_packets;    /**< Packets discarded by parser */
    uint32_t fec_recoveries;       /**< Chunks recovered via FEC */
    uint32_t nak_sent;             /**< NAK packets sent (puller) */
    float    packet_loss_rate;     /**< EWMA packet loss rate [0.0, 1.0] */
    uint32_t rtt_us;               /**< EWMA RTT estimate in microseconds */
    uint32_t pull_pressure;        /**< Pull requests received in last 100ms (exposer) */
} rgtp_stats_t;

/** @brief Per-surface latency statistics. */
typedef struct rgtp_latency_stats {
    uint32_t mean_us;              /**< Mean one-way chunk delay, microseconds */
    uint32_t jitter_us;            /**< Inter-chunk arrival variance, microseconds */
    uint32_t p99_us;               /**< 99th-percentile one-way delay, microseconds */
    uint32_t min_us;               /**< Minimum observed delay */
    uint32_t max_us;               /**< Maximum observed delay */
    uint32_t sample_count;         /**< Number of delay samples in window */
} rgtp_latency_stats_t;

/** @brief Satellite-specific link statistics. */
typedef struct rgtp_satellite_stats {
    /* Link quality metrics */
    float snr_db;                  /**< Current signal-to-noise ratio in dB */
    float ber;                     /**< Current bit error rate */
    float link_margin_db;          /**< Link margin in dB */
    int32_t doppler_offset_hz;     /**< Measured Doppler offset in Hz */
    
    /* Contact window statistics */
    uint32_t contact_attempts;     /**< Number of contact attempts */
    uint32_t successful_contacts;  /**< Number of successful contacts */
    uint32_t contact_duration_s;   /**< Duration of last successful contact in seconds */
    uint32_t time_to_contact_s;    /**< Time until next contact window in seconds */
    
    /* Store-and-forward statistics */
    uint64_t stored_bytes;         /**< Bytes in store-and-forward buffer */
    uint32_t stored_chunks;        /**< Chunks in store-and-forward buffer */
    uint32_t store_overflow_count; /**< Number of store overflow events */
    
    /* Spacecraft status */
    uint8_t spacecraft_health;     /**< Spacecraft health indicator (0-100) */
    uint8_t power_level;           /**< Available power percentage (0-100) */
    uint8_t antenna_status;        /**< Antenna status (0=down, 1=pointing, 2=tracking) */
    
    /* CCSDS protocol statistics */
    uint32_t ccsds_frames_sent;    /**< CCSDS frames transmitted */
    uint32_t ccsds_frames_received;/**< CCSDS frames received */
    uint32_t ccsds_frame_errors;   /**< CCSDS frame errors detected */
    uint32_t ccsds_vcdu_count;     /**< CCSDS Virtual Channel Data Units */
} rgtp_satellite_stats_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Structured logging
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief A single structured log event delivered to the registered callback. */
typedef struct rgtp_log_event {
    uint64_t    timestamp_ns;      /**< Nanoseconds since CLOCK_REALTIME epoch */
    int         level;             /**< RGTP_LOG_ERROR / WARN / INFO / DEBUG */
    const char* component;         /**< e.g. "rgtp.crypto", "rgtp.fec", "rgtp.wire" */
    const char* message;           /**< Human-readable summary */
    struct {
        const char* key;
        const char* value;
    } kv[8];                       /**< Up to 8 structured key-value pairs */
    int         kv_count;          /**< Number of populated kv entries */
} rgtp_log_event_t;

/** @brief Log callback function type. */
typedef void (*rgtp_log_fn)(const rgtp_log_event_t* event, void* ctx);

/* ═══════════════════════════════════════════════════════════════════════════
 * Library lifecycle
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initialise the RGTP library.
 *
 * Must be called once before any other RGTP function. Initialises the crypto
 * backend (sodium_init / RAND_poll), Winsock on Windows, GF(2^8) FEC tables,
 * and SIMD dispatch pointers.
 *
 * @return RGTP_OK on success, RGTP_ERR_CRYPTO_INIT if the crypto backend fails.
 */
rgtp_error_t  rgtp_init(void);

/** @brief Release all global library resources. Safe to call multiple times. */
void          rgtp_cleanup(void);

/** @brief Return the library version string (e.g. "1.0.0"). */
const char*   rgtp_version(void);

/**
 * @brief Return a human-readable description of an error code.
 * @param err  An rgtp_error_t value.
 * @return     A null-terminated string. Never returns NULL.
 */
const char*   rgtp_strerror(rgtp_error_t err);

/**
 * @brief Set a global custom allocator used by all surfaces.
 *
 * Must be called before any surface is created. Pass alloc=NULL to restore
 * the default system malloc/free.
 *
 * @param alloc  Pointer to an rgtp_allocator_t, or NULL to restore defaults.
 * @return RGTP_OK or RGTP_ERR_INVALID_ARG.
 */
rgtp_error_t  rgtp_set_allocator(const rgtp_allocator_t* alloc);

/* ═══════════════════════════════════════════════════════════════════════════
 * Socket management
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create and bind an RGTP socket.
 *
 * Selects the I/O backend (sendmmsg/recvmmsg, io_uring, IOCP, or AF_PACKET)
 * based on the platform and build configuration.
 *
 * @param cfg  Configuration (may be NULL for defaults).
 * @param out  Receives the created socket handle on success.
 * @return RGTP_OK, RGTP_ERR_SOCKET, RGTP_ERR_NOT_SUPPORTED, or RGTP_ERR_NOMEM.
 */
rgtp_error_t  rgtp_socket_create(const rgtp_config_t* cfg,
                                  rgtp_socket_t** out);

/** @brief Destroy a socket and release all associated resources. */
void          rgtp_socket_destroy(rgtp_socket_t* sock);

/* ═══════════════════════════════════════════════════════════════════════════
 * Exposer API
 * ═══════════════════════════════════════════════════════════════════════════ */
/* Create a socket that uses native CCSDS Space Packets on the given
 * device (e.g., "/dev/spw0", "/dev/ttyS0").
 * The address parameters in pull/expose functions are ignored. */
rgtp_socket_t *rgtp_socket_create_ccsds(const char *device,
                                        const rgtp_config_t *cfg);

/**
 * @brief Pre-encrypt data and create an immutable Exposure.
 *
 * Generates a CSPRNG Exposure_ID, encrypts all chunks with AEAD, builds the
 * Merkle tree, and optionally pre-computes FEC parity chunks. The operation
 * is atomic: either the surface is fully initialised or an error is returned
 * with no partial state.
 *
 * @param sock         Socket to serve pull requests on.
 * @param data         Plaintext data to expose.
 * @param size         Size of @p data in bytes.
 * @param cfg          Per-exposure configuration (may be NULL for defaults).
 * @param out_surface  Receives the created surface handle on success.
 * @return RGTP_OK, RGTP_ERR_NOMEM, RGTP_ERR_ENCRYPT, or RGTP_ERR_INVALID_ARG.
 */
rgtp_error_t  rgtp_expose(rgtp_socket_t*       sock,
                           const void*          data,
                           size_t               size,
                           const rgtp_config_t* cfg,
                           rgtp_surface_t**     out_surface);

/**
 * @brief Serve pending pull requests for an active Exposure.
 *
 * Receives pull requests, validates them, applies rate limiting, and sends
 * the requested chunks from the immutable store. Returns after @p timeout_ms
 * milliseconds or when the socket would block.
 *
 * @param surface     An exposer surface created by rgtp_expose().
 * @param timeout_ms  Maximum time to wait for requests (-1 = infinite).
 * @return RGTP_OK (including when no requests arrived), or RGTP_ERR_SOCKET.
 */
rgtp_error_t  rgtp_poll(rgtp_surface_t* surface, int timeout_ms);

/**
 * @brief Destroy a surface and securely zeroize all key material.
 *
 * Safe to call with NULL. After this call, @p surface must not be used.
 */
void          rgtp_destroy_surface(rgtp_surface_t* surface);

/**
 * @brief Retrieve the 128-bit Exposure_ID for a surface.
 *
 * @param surface  Any surface (exposer or puller).
 * @param out_id   16-byte buffer to receive the ID.
 * @return RGTP_OK or RGTP_ERR_INVALID_ARG.
 */
rgtp_error_t  rgtp_get_exposure_id(const rgtp_surface_t* surface,
                                    uint8_t out_id[16]);

/* ═══════════════════════════════════════════════════════════════════════════
 * Puller API
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Begin pulling an Exposure from a remote Exposer.
 *
 * Sends a Pull_Request, receives the Manifest, and initialises the puller
 * surface with the Merkle root, chunk count, and FEC parameters.
 *
 * @param sock         Socket to use for communication.
 * @param server       Address of the Exposer.
 * @param exposure_id  16-byte Exposure_ID to pull.
 * @param cfg          Per-pull configuration (may be NULL for defaults).
 * @param out_surface  Receives the created puller surface on success.
 * @return RGTP_OK, RGTP_ERR_TIMEOUT, RGTP_ERR_NOMEM, or RGTP_ERR_NOT_SUPPORTED.
 */
rgtp_error_t  rgtp_pull_start(rgtp_socket_t*                sock,
                               const struct sockaddr_storage* server,
                               const uint8_t                  exposure_id[16],
                               const rgtp_config_t*           cfg,
                               rgtp_surface_t**               out_surface);

/**
 * @brief Receive the next available chunk.
 *
 * Validates the packet type BEFORE writing to @p buffer. Verifies the AEAD
 * authentication tag and optional Merkle proof. Updates the anti-replay
 * window and sliding-window pull state.
 *
 * @param surface         A puller surface created by rgtp_pull_start().
 * @param buffer          Caller-supplied buffer to receive plaintext chunk data.
 * @param buf_size        Size of @p buffer in bytes.
 * @param out_received    Receives the number of plaintext bytes written.
 * @param out_chunk_index Receives the chunk index of the delivered chunk.
 * @return RGTP_OK, RGTP_ERR_AUTH_FAIL, RGTP_ERR_MERKLE_FAIL, RGTP_ERR_TIMEOUT,
 *         RGTP_ERR_TRUNCATED, or RGTP_ERR_CHUNK_INDEX_OOB.
 */
rgtp_error_t  rgtp_pull_next(rgtp_surface_t* surface,
                              void*           buffer,
                              size_t          buf_size,
                              size_t*         out_received,
                              uint32_t*       out_chunk_index);

/**
 * @brief Return the transfer completion fraction.
 *
 * @param surface  A puller surface.
 * @return A value in [0.0, 1.0]. Returns 0.0 for NULL or exposer surfaces.
 */
float         rgtp_progress(const rgtp_surface_t* surface);

/* ═══════════════════════════════════════════════════════════════════════════
 * Statistics
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Retrieve transfer statistics for a surface.
 *
 * @param surface  Any surface (exposer or puller).
 * @param out      Pointer to an rgtp_stats_t to populate.
 * @return RGTP_OK or RGTP_ERR_INVALID_ARG.
 */
rgtp_error_t  rgtp_get_stats(const rgtp_surface_t* surface,
                              rgtp_stats_t*         out);

/**
 * @brief Retrieve latency statistics for a puller surface.
 *
 * @param surface  A puller surface.
 * @param out      Pointer to an rgtp_latency_stats_t to populate.
 * @return RGTP_OK or RGTP_ERR_INVALID_ARG.
 */
rgtp_error_t  rgtp_get_latency_stats(const rgtp_surface_t* surface,
                                      rgtp_latency_stats_t* out);

/* ═══════════════════════════════════════════════════════════════════════════
 * Logging
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Register a structured log callback.
 *
 * The callback is invoked synchronously from the thread that triggers the
 * log event. Pass fn=NULL to restore the default stderr output.
 *
 * @param fn   Log callback function, or NULL to restore default.
 * @param ctx  Opaque context pointer passed to every callback invocation.
 */
void  rgtp_set_log_callback(rgtp_log_fn fn, void* ctx);

/**
 * @brief Set the minimum log level.
 *
 * Events below this level are suppressed without invoking the callback.
 *
 * @param level  One of RGTP_LOG_ERROR, RGTP_LOG_WARN, RGTP_LOG_INFO, RGTP_LOG_DEBUG.
 */
void  rgtp_set_log_level(int level);

/* ═══════════════════════════════════════════════════════════════════════════
 * Satellite Communications API
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Retrieve satellite-specific link statistics.
 *
 * @param surface  Any surface (exposer or puller) configured for satellite mode.
 * @param out      Pointer to an rgtp_satellite_stats_t to populate.
 * @return RGTP_OK or RGTP_ERR_INVALID_ARG.
 */
rgtp_error_t rgtp_get_satellite_stats(const rgtp_surface_t* surface,
                                      rgtp_satellite_stats_t* out);

/**
 * @brief Schedule a contact window with a ground station.
 *
 * @param surface  A surface configured for satellite communications.
 * @param start_time  UTC timestamp of contact start (seconds since epoch).
 * @param end_time    UTC timestamp of contact end (seconds since epoch).
 * @param ground_station  Ground station identifier string.
 * @return RGTP_OK, RGTP_ERR_SATELLITE_NO_CONTACT, or RGTP_ERR_INVALID_ARG.
 */
rgtp_error_t rgtp_schedule_contact(rgtp_surface_t* surface,
                                   uint64_t start_time,
                                   uint64_t end_time,
                                   const char* ground_station);

/**
 * @brief Update link parameters for adaptive coding and modulation.
 *
 * @param surface     A surface configured for satellite communications.
 * @param snr_db      Current signal-to-noise ratio in dB.
 * @param ber         Current bit error rate.
 * @param doppler_hz  Measured Doppler shift in Hz.
 * @return RGTP_OK or RGTP_ERR_INVALID_ARG.
 */
rgtp_error_t rgtp_update_link_parameters(rgtp_surface_t* surface,
                                         float snr_db,
                                         float ber,
                                         int32_t doppler_hz);

/**
 * @brief Enable store-and-forward mode for intermittent connectivity.
 *
 * @param surface     A surface configured for satellite communications.
 * @param max_storage_bytes  Maximum storage for store-and-forward (0 = unlimited).
 * @param max_storage_time_s Maximum storage time in seconds.
 * @return RGTP_OK or RGTP_ERR_INVALID_ARG.
 */
rgtp_error_t rgtp_enable_store_forward(rgtp_surface_t* surface,
                                       uint64_t max_storage_bytes,
                                       uint32_t max_storage_time_s);

typedef struct rgtp_contact_window {
    uint64_t start_time;
    uint64_t end_time;
    char ground_station[32];
} rgtp_contact_window_t;

/**
 * @brief Get available contact windows for a spacecraft.
 *
 * @param surface       A surface configured for satellite communications.
 * @param windows       Array to receive contact window information.
 * @param max_windows   Maximum number of windows that can be stored.
 * @param out_count     Receives the actual number of contact windows.
 * @return RGTP_OK or RGTP_ERR_INVALID_ARG.
 */
rgtp_error_t rgtp_get_contact_windows(const rgtp_surface_t* surface,
                                      rgtp_contact_window_t* windows,
                                      size_t max_windows,
                                      size_t* out_count);

/**
 * @brief Configure CCSDS protocol options.
 *
 * @param surface     A surface configured for satellite communications.
 * @param enable_tm   Enable CCSDS Telemetry (TM) protocol.
 * @param enable_tc   Enable CCSDS Telecommand (TC) protocol.
 * @param enable_aos  Enable CCSDS Advanced Orbiting Systems (AOS).
 * @param enable_cfdp Enable CCSDS File Delivery Protocol (CFDP).
 * @return RGTP_OK or RGTP_ERR_INVALID_ARG.
 */
rgtp_error_t rgtp_configure_ccsds(rgtp_surface_t* surface,
                                  bool enable_tm,
                                  bool enable_tc,
                                  bool enable_aos,
                                  bool enable_cfdp);

/**
 * @brief Send an emergency telecommand to a spacecraft.
 *
 * @param surface     A surface configured for satellite communications.
 * @param tc_data     Telecommand data payload.
 * @param tc_size     Size of telecommand data.
 * @param priority    Priority level (0=lowest, 255=highest).
 * @return RGTP_OK, RGTP_ERR_SATELLITE_NO_CONTACT, or RGTP_ERR_INVALID_ARG.
 */
rgtp_error_t rgtp_send_emergency_tc(rgtp_surface_t* surface,
                                    const void* tc_data,
                                    size_t tc_size,
                                    uint8_t priority);

/**
 * @brief Calculate link budget for current configuration.
 *
 * @param surface       A surface configured for satellite communications.
 * @param out_margin_db Receives the calculated link margin in dB.
 * @param out_ebno_db   Receives the calculated Eb/No in dB.
 * @return RGTP_OK or RGTP_ERR_INVALID_ARG.
 */
rgtp_error_t rgtp_calculate_link_budget(const rgtp_surface_t* surface,
                                        float* out_margin_db,
                                        float* out_ebno_db);

/**
 * @brief Configure Doppler shift compensation.
 *
 * @param surface           A surface configured for satellite communications.
 * @param enable_compensation Enable Doppler compensation.
 * @param max_doppler_hz    Maximum expected Doppler shift in Hz.
 * @param update_rate_hz    Doppler update rate in Hz.
 * @return RGTP_OK or RGTP_ERR_INVALID_ARG.
 */
rgtp_error_t rgtp_configure_doppler(rgtp_surface_t* surface,
                                    bool enable_compensation,
                                    uint32_t max_doppler_hz,
                                    float update_rate_hz);

/**
 * @brief Check if current time is within a contact window.
 *
 * @param surface  A surface configured for satellite communications.
 * @param out_in_contact  Receives true if within contact window.
 * @param out_time_to_contact_s  Receives seconds until next contact.
 * @param out_time_left_s  Receives seconds remaining in current contact.
 * @return RGTP_OK or RGTP_ERR_INVALID_ARG.
 */
rgtp_error_t rgtp_check_contact_status(const rgtp_surface_t* surface,
                                       bool* out_in_contact,
                                       uint32_t* out_time_to_contact_s,
                                       uint32_t* out_time_left_s);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* RGTP_H */
