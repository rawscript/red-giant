/**
 * @file rgtp_rs_simd.c
 * @brief SIMD-accelerated GF(2^8) multiplication dispatch.
 *
 * Uses 4-bit split lookup tables (VPSHUFB on x86, VTBL on ARM NEON) to
 * vectorise GF(2^8) multiplication over byte arrays.
 *
 * Platform detection at build time (CMake sets RGTP_ENABLE_SIMD):
 *   x86-64 with AVX2   → rgtp_gf_mul_region_avx2
 *   x86-64 with SSE4.2 → rgtp_gf_mul_region_sse42
 *   ARM with NEON      → rgtp_gf_mul_region_neon
 *   Fallback           → rgtp_gf_mul_region_scalar
 *
 * The function pointer rgtp_gf_mul_region is set at rgtp_init() time.
 *
 * Requirements: 4.8
 */

#include "rgtp_fec_internal.h"
#include "rgtp_gf256_internal.h"

#include <string.h>
#include <stddef.h>

/* ── Function pointer type ──────────────────────────────────────────────── */

/**
 * @brief Multiply each byte of @p src by GF scalar @p c, XOR into @p dst.
 *
 * dst[i] ^= gf_mul(c, src[i])  for i in [0, len)
 */
typedef void (*rgtp_gf_mul_region_fn)(uint8_t c,
                                       const uint8_t* src,
                                       uint8_t*       dst,
                                       size_t         len);

/** Global dispatch pointer — set by rgtp_simd_init(). */
rgtp_gf_mul_region_fn rgtp_gf_mul_region = NULL;

/* ── Scalar fallback ────────────────────────────────────────────────────── */

static void rgtp_gf_mul_region_scalar(uint8_t c,
                                       const uint8_t* src,
                                       uint8_t*       dst,
                                       size_t         len)
{
    if (c == 0) {
        /* XOR with zero is a no-op */
        return;
    }
    if (c == 1) {
        for (size_t i = 0; i < len; i++) {
            dst[i] ^= src[i];
        }
        return;
    }
    for (size_t i = 0; i < len; i++) {
        dst[i] ^= gf_mul(c, src[i]);
    }
}

/* ── x86 SSE4.2 path ────────────────────────────────────────────────────── */

#if defined(RGTP_ENABLE_SIMD) && defined(__SSE4_2__) && defined(__SSSE3__)
#include <immintrin.h>

static void rgtp_gf_mul_region_sse42(uint8_t c,
                                      const uint8_t* src,
                                      uint8_t*       dst,
                                      size_t         len)
{
    if (c == 0) return;
    if (c == 1) {
        for (size_t i = 0; i < len; i++) dst[i] ^= src[i];
        return;
    }

    /*
     * 4-bit split table approach:
     *   gf_mul(c, x) = gf_mul(c, x_lo) ^ gf_mul(c, x_hi << 4)
     * where x_lo = x & 0x0F, x_hi = (x >> 4) & 0x0F.
     *
     * Build 16-entry lookup tables for the low and high nibbles.
     */
    uint8_t tbl_lo[16], tbl_hi[16];
    for (int i = 0; i < 16; i++) {
        tbl_lo[i] = gf_mul(c, (uint8_t)i);
        tbl_hi[i] = gf_mul(c, (uint8_t)(i << 4));
    }

    __m128i lo_tbl = _mm_loadu_si128((const __m128i*)tbl_lo);
    __m128i hi_tbl = _mm_loadu_si128((const __m128i*)tbl_hi);
    __m128i mask_lo = _mm_set1_epi8(0x0F);

    size_t i = 0;
    for (; i + 16 <= len; i += 16) {
        __m128i v   = _mm_loadu_si128((const __m128i*)(src + i));
        __m128i d   = _mm_loadu_si128((const __m128i*)(dst + i));
        __m128i lo  = _mm_and_si128(v, mask_lo);
        __m128i hi  = _mm_and_si128(_mm_srli_epi16(v, 4), mask_lo);
        __m128i res = _mm_xor_si128(_mm_shuffle_epi8(lo_tbl, lo),
                                     _mm_shuffle_epi8(hi_tbl, hi));
        _mm_storeu_si128((__m128i*)(dst + i), _mm_xor_si128(d, res));
    }
    /* Scalar tail */
    for (; i < len; i++) {
        dst[i] ^= gf_mul(c, src[i]);
    }
}
#endif /* SSE4.2 */

/* ── x86 AVX2 path ──────────────────────────────────────────────────────── */

#if defined(RGTP_ENABLE_SIMD) && defined(__AVX2__)
#include <immintrin.h>

static void rgtp_gf_mul_region_avx2(uint8_t c,
                                     const uint8_t* src,
                                     uint8_t*       dst,
                                     size_t         len)
{
    if (c == 0) return;
    if (c == 1) {
        for (size_t i = 0; i < len; i++) dst[i] ^= src[i];
        return;
    }

    uint8_t tbl_lo[16], tbl_hi[16];
    for (int i = 0; i < 16; i++) {
        tbl_lo[i] = gf_mul(c, (uint8_t)i);
        tbl_hi[i] = gf_mul(c, (uint8_t)(i << 4));
    }

    /* Broadcast 16-byte tables to 32-byte AVX2 registers */
    __m256i lo_tbl = _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)tbl_lo));
    __m256i hi_tbl = _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)tbl_hi));
    __m256i mask_lo = _mm256_set1_epi8(0x0F);

    size_t i = 0;
    for (; i + 32 <= len; i += 32) {
        __m256i v   = _mm256_loadu_si256((const __m256i*)(src + i));
        __m256i d   = _mm256_loadu_si256((const __m256i*)(dst + i));
        __m256i lo  = _mm256_and_si256(v, mask_lo);
        __m256i hi  = _mm256_and_si256(_mm256_srli_epi16(v, 4), mask_lo);
        __m256i res = _mm256_xor_si256(_mm256_shuffle_epi8(lo_tbl, lo),
                                        _mm256_shuffle_epi8(hi_tbl, hi));
        _mm256_storeu_si256((__m256i*)(dst + i), _mm256_xor_si256(d, res));
    }
    /* SSE4.2 tail (handles 16-byte chunks) */
    for (; i + 16 <= len; i += 16) {
        __m128i v   = _mm_loadu_si128((const __m128i*)(src + i));
        __m128i d   = _mm_loadu_si128((const __m128i*)(dst + i));
        __m128i lo  = _mm_and_si128(v, _mm256_castsi256_si128(mask_lo));
        __m128i hi  = _mm_and_si128(_mm_srli_epi16(v, 4),
                                     _mm256_castsi256_si128(mask_lo));
        __m128i res = _mm_xor_si128(
            _mm_shuffle_epi8(_mm256_castsi256_si128(lo_tbl), lo),
            _mm_shuffle_epi8(_mm256_castsi256_si128(hi_tbl), hi));
        _mm_storeu_si128((__m128i*)(dst + i), _mm_xor_si128(d, res));
    }
    /* Scalar tail */
    for (; i < len; i++) {
        dst[i] ^= gf_mul(c, src[i]);
    }
}
#endif /* AVX2 */

/* ── ARM NEON path ──────────────────────────────────────────────────────── */

#if defined(RGTP_ENABLE_SIMD) && defined(__ARM_NEON)
#include <arm_neon.h>

static void rgtp_gf_mul_region_neon(uint8_t c,
                                     const uint8_t* src,
                                     uint8_t*       dst,
                                     size_t         len)
{
    if (c == 0) return;
    if (c == 1) {
        for (size_t i = 0; i < len; i++) dst[i] ^= src[i];
        return;
    }

    uint8_t tbl_lo[16], tbl_hi[16];
    for (int i = 0; i < 16; i++) {
        tbl_lo[i] = gf_mul(c, (uint8_t)i);
        tbl_hi[i] = gf_mul(c, (uint8_t)(i << 4));
    }

    uint8x16_t lo_tbl = vld1q_u8(tbl_lo);
    uint8x16_t hi_tbl = vld1q_u8(tbl_hi);
    uint8x16_t mask_lo = vdupq_n_u8(0x0F);

    size_t i = 0;
    for (; i + 16 <= len; i += 16) {
        uint8x16_t v   = vld1q_u8(src + i);
        uint8x16_t d   = vld1q_u8(dst + i);
        uint8x16_t lo  = vandq_u8(v, mask_lo);
        uint8x16_t hi  = vandq_u8(vshrq_n_u8(v, 4), mask_lo);
        uint8x16_t res = veorq_u8(vqtbl1q_u8(lo_tbl, lo),
                                   vqtbl1q_u8(hi_tbl, hi));
        vst1q_u8(dst + i, veorq_u8(d, res));
    }
    for (; i < len; i++) {
        dst[i] ^= gf_mul(c, src[i]);
    }
}
#endif /* NEON */

/* ── Runtime dispatch initialisation ───────────────────────────────────── */

void rgtp_simd_init(void)
{
#if defined(RGTP_ENABLE_SIMD)
#  if defined(__AVX2__)
    rgtp_gf_mul_region = rgtp_gf_mul_region_avx2;
#  elif defined(__SSE4_2__) && defined(__SSSE3__)
    rgtp_gf_mul_region = rgtp_gf_mul_region_sse42;
#  elif defined(__ARM_NEON)
    rgtp_gf_mul_region = rgtp_gf_mul_region_neon;
#  else
    rgtp_gf_mul_region = rgtp_gf_mul_region_scalar;
#  endif
#else
    rgtp_gf_mul_region = rgtp_gf_mul_region_scalar;
#endif
}
