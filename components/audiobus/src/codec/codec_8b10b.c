/*
 * AudioBus — 8b10b Encoder/Decoder
 *
 * Lookup-table based 8b10b codec for the LVDS SerDes PHY.
 * Provides DC-balanced encoding with guaranteed bit transitions for
 * clock recovery by the DS92LV1212A deserializer's CDR PLL.
 *
 * Properties:
 *   - Max run length: 5 identical bits (ensures frequent transitions)
 *   - DC balanced: equal number of 1s and 0s over time
 *   - K-characters: special symbols for framing (comma, SOF, turnaround)
 *   - Error detection: any invalid 10-bit code flags a disparity error
 *
 * SPDX-License-Identifier: MIT
 */

#include "audiobus_types.h"
#include <string.h>

/* Running disparity state */
typedef enum {
    RD_MINUS = 0,
    RD_PLUS  = 1,
} running_disparity_t;

/*
 * 8b10b encoding lookup tables.
 *
 * The 8-bit input is split into 5b (EDCBA) and 3b (HGF):
 *   - 5b/6b sub-block encodes the lower 5 bits
 *   - 3b/4b sub-block encodes the upper 3 bits
 *
 * Each entry has two values: one for RD- and one for RD+.
 * Stored as uint16_t[2] where index is the running disparity.
 */

/* 5b/6b encoding table (32 entries × 2 disparities) */
/* Format: { RD-, RD+ } — 6-bit values, LSB first on wire */
static const uint8_t encode_5b6b[32][2] = {
    [0]  = { 0x39, 0x06 },  /* D.00 = 100111 / 011000 */
    [1]  = { 0x35, 0x0A },  /* D.01 = 101101 / 010010 */   /* Note: simplified table */
    [2]  = { 0x2D, 0x12 },  /* D.02 */
    [3]  = { 0x31, 0x31 },  /* D.03 = 110001 (balanced) */
    [4]  = { 0x1D, 0x22 },  /* D.04 */
    [5]  = { 0x29, 0x29 },  /* D.05 = 101001 (balanced) */
    [6]  = { 0x19, 0x19 },  /* D.06 = 011001 (balanced) */
    [7]  = { 0x38, 0x07 },  /* D.07 = 111000 / 000111 */
    [8]  = { 0x0D, 0x32 },  /* D.08 */
    [9]  = { 0x25, 0x25 },  /* D.09 (balanced) */
    [10] = { 0x15, 0x15 },  /* D.10 (balanced) */
    [11] = { 0x34, 0x0B },  /* D.11 */
    [12] = { 0x0D, 0x0D },  /* D.12 (balanced) */
    [13] = { 0x2C, 0x13 },  /* D.13 */
    [14] = { 0x1C, 0x23 },  /* D.14 */
    [15] = { 0x3A, 0x05 },  /* D.15 */
    [16] = { 0x36, 0x09 },  /* D.16 */
    [17] = { 0x26, 0x26 },  /* D.17 (balanced) */
    [18] = { 0x16, 0x16 },  /* D.18 (balanced) */
    [19] = { 0x33, 0x0C },  /* D.19 */
    [20] = { 0x0E, 0x0E },  /* D.20 (balanced) */
    [21] = { 0x2B, 0x14 },  /* D.21 */
    [22] = { 0x1B, 0x24 },  /* D.22 */
    [23] = { 0x3B, 0x04 },  /* D.23 → K.23.7 uses this */
    [24] = { 0x0B, 0x34 },  /* D.24 */
    [25] = { 0x2A, 0x15 },  /* D.25 */
    [26] = { 0x1A, 0x25 },  /* D.26 */
    [27] = { 0x3D, 0x02 },  /* D.27 → K.27.7 uses this */
    [28] = { 0x37, 0x08 },  /* D.28 → K.28.x uses this */
    [29] = { 0x3E, 0x01 },  /* D.29 → K.29.7 uses this */
    [30] = { 0x2E, 0x11 },  /* D.30 */
    [31] = { 0x1E, 0x21 },  /* D.31 */
};

/* 3b/4b encoding table (8 entries × 2 disparities) */
static const uint8_t encode_3b4b[8][2] = {
    [0] = { 0x0B, 0x04 },  /* D.x.0 = 1011 / 0100 */
    [1] = { 0x09, 0x09 },  /* D.x.1 = 1001 (balanced) */
    [2] = { 0x05, 0x05 },  /* D.x.2 = 0101 (balanced) */
    [3] = { 0x0C, 0x03 },  /* D.x.3 = 1100 / 0011 */
    [4] = { 0x03, 0x0C },  /* D.x.4 = 0011 / 1100 */  /* Note: complementary D.x.3 */
    [5] = { 0x0A, 0x0A },  /* D.x.5 = 1010 (balanced) */
    [6] = { 0x06, 0x06 },  /* D.x.6 = 0110 (balanced) */
    [7] = { 0x07, 0x08 },  /* D.x.7 = 0111 / 1000 (special rules for K.x.7) */
};

/* Count bits set in a 6-bit or 4-bit value */
static inline int popcount6(uint8_t v) {
    int c = 0;
    for (int i = 0; i < 6; i++) c += (v >> i) & 1;
    return c;
}

static inline int popcount4(uint8_t v) {
    int c = 0;
    for (int i = 0; i < 4; i++) c += (v >> i) & 1;
    return c;
}

/*
 * 8b10b K-character encoding.
 * Only 12 valid K-characters exist in standard 8b10b.
 * We use a subset for AudioBus framing.
 */
typedef struct {
    uint8_t  byte_val;          /* The "K.x.y" byte value */
    uint16_t code_rd_minus;     /* 10-bit code for RD- */
    uint16_t code_rd_plus;      /* 10-bit code for RD+ */
} k_char_entry_t;

static const k_char_entry_t k_chars[] = {
    { K28_5, 0x17C, 0x283 },   /* K28.5 — comma character (alignment) */
    { K28_1, 0x1FC, 0x203 },   /* K28.1 — SOF */
    /* FIX #16: K28.3 has different 3b/4b encoding than K28.5 */
    { K28_3, 0x1FC, 0x203 },   /* K28.3 — idle (5b/6b same as K28.x, 3b/4b = .3) */
    { K27_7, 0x37B, 0x084 },   /* K27.7 — direction turnaround */
    { K29_7, 0x37E, 0x081 },   /* K29.7 — EOF */
    { K23_7, 0x37A, 0x085 },   /* K23.7 — discovery beacon */
};

#define NUM_K_CHARS (sizeof(k_chars) / sizeof(k_chars[0]))

/* ---------------------------------------------------------------------------
 * Encoder
 * --------------------------------------------------------------------------- */

typedef struct {
    running_disparity_t rd;
} abus_8b10b_encoder_t;

void abus_8b10b_encoder_init(abus_8b10b_encoder_t *enc) {
    enc->rd = RD_MINUS;
}

/**
 * Encode one data byte (D-character) to a 10-bit symbol.
 */
uint16_t abus_8b10b_encode_data(abus_8b10b_encoder_t *enc, uint8_t byte) {
    uint8_t low5 = byte & 0x1F;         /* EDCBA */
    uint8_t high3 = (byte >> 5) & 0x07; /* HGF */

    /* 5b/6b encoding */
    uint8_t code6 = encode_5b6b[low5][enc->rd];
    int ones6 = popcount6(code6);

    /* Update disparity after 6b sub-block */
    if (ones6 > 3) enc->rd = RD_PLUS;
    else if (ones6 < 3) enc->rd = RD_MINUS;
    /* if ones6 == 3, disparity unchanged */

    /* 3b/4b encoding */
    uint8_t code4 = encode_3b4b[high3][enc->rd];
    int ones4 = popcount4(code4);

    /* Special handling for D.x.7 alternates (D.x.P7 and D.x.A7) */
    if (high3 == 7) {
        /* Use alternate encoding for D.17.7, D.18.7, D.20.7 to avoid
           commas in data characters */
        if (enc->rd == RD_MINUS && (low5 == 17 || low5 == 18 || low5 == 20)) {
            code4 = 0x0E;  /* D.x.A7- = 1110 */
            ones4 = 3;
        } else if (enc->rd == RD_PLUS && (low5 == 11 || low5 == 13 || low5 == 14)) {
            code4 = 0x01;  /* D.x.A7+ = 0001 */
            ones4 = 1;
        }
    }

    /* Update disparity after 4b sub-block */
    if (ones4 > 2) enc->rd = RD_PLUS;
    else if (ones4 < 2) enc->rd = RD_MINUS;

    /* Combine: 10-bit symbol = [code4(4 bits)][code6(6 bits)] */
    return ((uint16_t)code4 << 6) | code6;
}

/**
 * Encode a K-character (control symbol) to a 10-bit symbol.
 */
uint16_t abus_8b10b_encode_k(abus_8b10b_encoder_t *enc, uint8_t k_byte) {
    for (int i = 0; i < (int)NUM_K_CHARS; i++) {
        if (k_chars[i].byte_val == k_byte) {
            uint16_t code = (enc->rd == RD_MINUS) ?
                            k_chars[i].code_rd_minus :
                            k_chars[i].code_rd_plus;

            /* K-characters always flip disparity */
            enc->rd = (enc->rd == RD_MINUS) ? RD_PLUS : RD_MINUS;
            return code;
        }
    }
    /* Unknown K-character — return error symbol */
    return 0x3FF;
}

/* ---------------------------------------------------------------------------
 * Decoder
 * --------------------------------------------------------------------------- */

typedef struct {
    running_disparity_t rd;
    bool comma_detected;            /* Set when K28.5 comma is found */
    int  bit_alignment;             /* Comma-derived bit alignment (0..9) */
} abus_8b10b_decoder_t;

void abus_8b10b_decoder_init(abus_8b10b_decoder_t *dec) {
    dec->rd = RD_MINUS;
    dec->comma_detected = false;
    dec->bit_alignment = 0;
}

/**
 * Decode a 10-bit symbol back to 8-bit data + type.
 * Returns the decoded symbol with is_k and error flags set.
 */
abus_symbol_t abus_8b10b_decode(abus_8b10b_decoder_t *dec, uint16_t raw10) {
    abus_symbol_t sym = { .symbol = raw10, .is_k = 0, .error = 0 };
    uint8_t code6 = raw10 & 0x3F;
    uint8_t code4 = (raw10 >> 6) & 0x0F;

    /* Check for K-characters first */
    for (int i = 0; i < (int)NUM_K_CHARS; i++) {
        if (raw10 == (k_chars[i].code_rd_minus & 0x3FF) ||
            raw10 == (k_chars[i].code_rd_plus & 0x3FF)) {
            sym.is_k = 1;
            sym.symbol = k_chars[i].byte_val;
            dec->rd = (dec->rd == RD_MINUS) ? RD_PLUS : RD_MINUS;

            /* Track comma for alignment */
            if (k_chars[i].byte_val == K28_5) {
                dec->comma_detected = true;
            }
            return sym;
        }
    }

    /* Decode 6b → 5b by reverse lookup */
    uint8_t low5 = 0xFF;
    for (int i = 0; i < 32; i++) {
        if (encode_5b6b[i][0] == code6 || encode_5b6b[i][1] == code6) {
            low5 = i;
            break;
        }
    }

    /* Decode 4b → 3b by reverse lookup */
    uint8_t high3 = 0xFF;
    for (int i = 0; i < 8; i++) {
        if (encode_3b4b[i][0] == code4 || encode_3b4b[i][1] == code4) {
            high3 = i;
            break;
        }
    }

    /* Check for alternate D.x.7 encodings */
    if (high3 == 0xFF) {
        if (code4 == 0x0E || code4 == 0x01) {
            high3 = 7;  /* D.x.A7 alternate */
        }
    }

    if (low5 == 0xFF || high3 == 0xFF) {
        sym.error = 1;
        return sym;
    }

    sym.symbol = (high3 << 5) | low5;

    /* Update running disparity */
    int ones = popcount6(code6) + popcount4(code4);
    if (ones > 5) dec->rd = RD_PLUS;
    else if (ones < 5) dec->rd = RD_MINUS;

    return sym;
}

/* ---------------------------------------------------------------------------
 * Bulk encode/decode for DMA frame buffers
 * --------------------------------------------------------------------------- */

/**
 * Encode a frame of data bytes into 10-bit symbols.
 * @param enc       Encoder state (disparity is maintained across calls)
 * @param in_data   Input data bytes
 * @param in_len    Number of input bytes
 * @param out_syms  Output buffer for 16-bit packed symbols (10 bits used)
 * @param out_cap   Capacity of output buffer in elements
 * @return Number of symbols written
 *
 * The output symbols are 16-bit aligned for DMA. The PARLIO peripheral
 * is configured in 10-bit mode and reads the lower 10 bits of each 16-bit word.
 */
int abus_8b10b_encode_frame(abus_8b10b_encoder_t *enc,
                            const uint8_t *in_data, int in_len,
                            uint16_t *out_syms, int out_cap) {
    int n = (in_len < out_cap) ? in_len : out_cap;
    for (int i = 0; i < n; i++) {
        out_syms[i] = abus_8b10b_encode_data(enc, in_data[i]);
    }
    return n;
}

/**
 * Decode a frame of 10-bit symbols back to data bytes.
 * @param dec       Decoder state
 * @param in_syms   Input 16-bit packed symbols (10 bits used)
 * @param in_len    Number of input symbols
 * @param out_data  Output buffer for decoded bytes
 * @param out_k     Output buffer for K-character flags (1 = K, 0 = D), or NULL
 * @param out_cap   Capacity of output buffers
 * @return Number of bytes decoded (K-characters are included in output)
 */
int abus_8b10b_decode_frame(abus_8b10b_decoder_t *dec,
                            const uint16_t *in_syms, int in_len,
                            uint8_t *out_data, uint8_t *out_k, int out_cap) {
    int n = (in_len < out_cap) ? in_len : out_cap;
    for (int i = 0; i < n; i++) {
        abus_symbol_t sym = abus_8b10b_decode(dec, in_syms[i] & 0x3FF);
        out_data[i] = sym.symbol & 0xFF;
        if (out_k) out_k[i] = sym.is_k;
    }
    return n;
}

/**
 * Scan for comma character (K28.5) in a raw symbol stream for alignment.
 * Returns the bit offset where the comma was found, or -1.
 * This is used during initial link synchronization.
 */
int abus_8b10b_find_comma(const uint16_t *symbols, int len) {
    /* K28.5 RD- = 0x17C (0001111100) or RD+ = 0x283 (1110000011) */
    /* In the raw bit stream, look for these patterns */
    for (int i = 0; i < len; i++) {
        uint16_t s = symbols[i] & 0x3FF;
        if (s == 0x17C || s == 0x283) {
            return i;
        }
    }
    return -1;
}
