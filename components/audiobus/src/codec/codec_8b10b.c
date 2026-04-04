/*
 * AudioBus — 8b10b Encoder/Decoder
 *
 * Lookup-table based 8b10b codec for the LVDS SerDes PHY.
 * Provides DC-balanced encoding with guaranteed bit transitions for
 * clock recovery by the DS92LV1212A deserializer's CDR PLL.
 *
 * Encoding tables are per IEEE 802.3 Clause 36, Tables 36-1 through 36-3.
 *
 * Properties:
 *   - Max run length: 5 identical bits (ensures frequent transitions)
 *   - DC balanced: equal number of 1s and 0s over time
 *   - K-characters: special symbols for framing (comma, SOF, turnaround)
 *   - Error detection: any invalid 10-bit code flags a disparity error
 *
 * Bit ordering convention (matches IEEE 802.3 Clause 36):
 *   The 10-bit symbol is composed as { abcdei fghj }.
 *   Bit 0 of the stored uint16_t = 'a' (first transmitted bit).
 *   The 6-bit sub-block (abcdei) occupies bits [5:0].
 *   The 4-bit sub-block (fghj)   occupies bits [9:6].
 *   So the stored 10-bit value = (fghj << 6) | abcdei.
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
 * 5b/6b encoding table — IEEE 802.3 Table 36-1
 *
 * Index = 5-bit data value (EDCBA, 0..31).
 * Each entry: { RD-, RD+ } stored as uint8_t.
 * Bit ordering: bit 0 = a, bit 1 = b, ..., bit 5 = i.
 *
 * "RD-" column = code used when running disparity entering the
 * sub-block is negative. Unbalanced codes (4 ones) appear in
 * the RD- column; their complements (2 ones) in the RD+ column.
 * Balanced codes (3 ones) are the same for both columns.
 */
static const uint8_t encode_5b6b[32][2] = {
    /*          RD-     RD+         abcdei (standard notation)         */
    [ 0] = { 0x39, 0x06 },  /* D.00: 100111 / 011000                  */
    [ 1] = { 0x2E, 0x11 },  /* D.01: 011101 / 100010                  */
    [ 2] = { 0x2D, 0x12 },  /* D.02: 101101 / 010010                  */
    [ 3] = { 0x23, 0x23 },  /* D.03: 110001            (balanced)      */
    [ 4] = { 0x2B, 0x14 },  /* D.04: 110101 / 001010                  */
    [ 5] = { 0x25, 0x25 },  /* D.05: 101001            (balanced)      */
    [ 6] = { 0x26, 0x26 },  /* D.06: 011001            (balanced)      */
    [ 7] = { 0x07, 0x38 },  /* D.07: 111000 / 000111                  */
    [ 8] = { 0x27, 0x18 },  /* D.08: 111001 / 000110                  */
    [ 9] = { 0x29, 0x29 },  /* D.09: 100101            (balanced)      */
    [10] = { 0x2A, 0x2A },  /* D.10: 010101            (balanced)      */
    [11] = { 0x0B, 0x0B },  /* D.11: 110100            (balanced)      */
    [12] = { 0x2C, 0x2C },  /* D.12: 001101            (balanced)      */
    [13] = { 0x0D, 0x0D },  /* D.13: 101100            (balanced)      */
    [14] = { 0x0E, 0x0E },  /* D.14: 011100            (balanced)      */
    [15] = { 0x3A, 0x05 },  /* D.15: 010111 / 101000                  */
    [16] = { 0x36, 0x09 },  /* D.16: 011011 / 100100                  */
    [17] = { 0x31, 0x31 },  /* D.17: 100011            (balanced)      */
    [18] = { 0x32, 0x32 },  /* D.18: 010011            (balanced)      */
    [19] = { 0x13, 0x13 },  /* D.19: 110010            (balanced)      */
    [20] = { 0x34, 0x34 },  /* D.20: 001011            (balanced)      */
    [21] = { 0x15, 0x15 },  /* D.21: 101010            (balanced)      */
    [22] = { 0x16, 0x16 },  /* D.22: 011010            (balanced)      */
    [23] = { 0x17, 0x28 },  /* D.23: 111010 / 000101  (K23.7 uses this) */
    [24] = { 0x33, 0x0C },  /* D.24: 110011 / 001100                  */
    [25] = { 0x19, 0x19 },  /* D.25: 100110            (balanced)      */
    [26] = { 0x1A, 0x1A },  /* D.26: 010110            (balanced)      */
    [27] = { 0x1B, 0x24 },  /* D.27: 110110 / 001001  (K27.7 uses this) */
    [28] = { 0x3C, 0x03 },  /* D.28: 001111 / 110000  (K28.x uses this) */
    [29] = { 0x1D, 0x22 },  /* D.29: 101110 / 010001  (K29.7 uses this) */
    [30] = { 0x1E, 0x21 },  /* D.30: 011110 / 100001                  */
    [31] = { 0x35, 0x0A },  /* D.31: 101011 / 010100                  */
};

/*
 * 3b/4b encoding table — IEEE 802.3 Table 36-2
 *
 * Index = 3-bit data value (HGF, 0..7).
 * Each entry: { RD-, RD+ } stored as uint8_t.
 * Bit ordering: bit 0 = f, bit 1 = g, bit 2 = h, bit 3 = j.
 *
 * "RD-" column = code used when the intermediate running disparity
 * (after the 6-bit sub-block) is negative. For unbalanced codes
 * (3 ones), the RD- entry pushes disparity positive; the RD+ entry
 * (1 one) pushes it negative. Balanced codes (2 ones) don't change
 * disparity but still have distinct patterns for each input RD to
 * control bit-run lengths.
 *
 * D.x.P7 is the primary .7 encoding. The alternate D.x.A7 encoding
 * is applied by the encoder for specific data values (see encoder).
 */
static const uint8_t encode_3b4b[8][2] = {
    /*        RD-   RD+       fghj (standard notation)   */
    [0] = { 0x0D, 0x02 },  /* D.x.0: 1011 / 0100        */
    [1] = { 0x06, 0x09 },  /* D.x.1: 0110 / 1001        */
    [2] = { 0x05, 0x0A },  /* D.x.2: 1010 / 0101        */
    [3] = { 0x03, 0x0C },  /* D.x.3: 1100 / 0011        */
    [4] = { 0x0B, 0x04 },  /* D.x.4: 1101 / 0010        */
    [5] = { 0x0A, 0x05 },  /* D.x.5: 0101 / 1010        */
    [6] = { 0x09, 0x06 },  /* D.x.6: 1001 / 0110        */
    [7] = { 0x07, 0x08 },  /* D.x.P7: 1110 / 0001       */
};

/*
 * D.x.A7 alternate 3b/4b codes (used by encoder for specific values
 * and by K23.7, K27.7, K29.7 special characters):
 *   A7 RD-: fghj = 0111 → 0x0E  (3 ones)
 *   A7 RD+: fghj = 1000 → 0x01  (1 one)
 */
#define A7_RD_MINUS  0x0E
#define A7_RD_PLUS   0x01

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
 * K-character encoding — IEEE 802.3 Table 36-3
 *
 * K-characters use special combinations of 5b/6b and 3b/4b sub-blocks
 * that cannot appear in any valid D-character encoding.
 *
 * K28.x characters use the D.28 5b/6b code (001111/110000) which has
 * a distinctive run of 4 identical bits, combined with normal 3b/4b.
 *
 * K23.7, K27.7, K29.7 use their respective D.x 5b/6b codes combined
 * with the alternate A7 3b/4b code (0111/1000 instead of 1110/0001).
 *
 * The 10-bit values here are in our internal format: (fghj << 6) | abcdei.
 *
 * Derivation:
 *   K28.5 RD-: 6b=D28.RD-(0x3C,4ones→intRD+), 4b=.5.RD+(0x05)
 *              → (0x05<<6)|0x3C = 0x17C.  Pattern: 0011111010
 *   K28.5 RD+: 6b=D28.RD+(0x03,2ones→intRD-), 4b=.5.RD-(0x0A)
 *              → (0x0A<<6)|0x03 = 0x283.  Pattern: 1100000101
 *
 *   K28.1 RD-: (0x09<<6)|0x3C = 0x27C.  Pattern: 0011111001
 *   K28.1 RD+: (0x06<<6)|0x03 = 0x183.  Pattern: 1100000110
 *
 *   K28.3 RD-: (0x0C<<6)|0x3C = 0x33C.  Pattern: 0011110011
 *   K28.3 RD+: (0x03<<6)|0x03 = 0x0C3.  Pattern: 1100001100
 *
 *   K27.7 RD-: 6b=D27.RD-(0x1B,4ones→intRD+), 4b=A7.RD+(0x01)
 *              → (0x01<<6)|0x1B = 0x05B.  Pattern: 1101101000
 *   K27.7 RD+: 6b=D27.RD+(0x24,2ones→intRD-), 4b=A7.RD-(0x0E)
 *              → (0x0E<<6)|0x24 = 0x3A4.  Pattern: 0010010111
 *
 *   K29.7 RD-: (0x01<<6)|0x1D = 0x05D.  Pattern: 1011101000
 *   K29.7 RD+: (0x0E<<6)|0x22 = 0x3A2.  Pattern: 0100010111
 *
 *   K23.7 RD-: (0x01<<6)|0x17 = 0x057.  Pattern: 1110101000
 *   K23.7 RD+: (0x0E<<6)|0x28 = 0x3A8.  Pattern: 0001010111
 */
typedef struct {
    uint8_t  byte_val;          /* The "K.x.y" byte value */
    uint16_t code_rd_minus;     /* 10-bit code for initial RD- */
    uint16_t code_rd_plus;      /* 10-bit code for initial RD+ */
} k_char_entry_t;

static const k_char_entry_t k_chars[] = {
    { K28_5, 0x17C, 0x283 },   /* K28.5 — comma character (alignment) */
    { K28_1, 0x27C, 0x183 },   /* K28.1 — SOF */
    { K28_3, 0x33C, 0x0C3 },   /* K28.3 — idle */
    { K27_7, 0x05B, 0x3A4 },   /* K27.7 — direction turnaround */
    { K29_7, 0x05D, 0x3A2 },   /* K29.7 — EOF */
    { K23_7, 0x057, 0x3A8 },   /* K23.7 — discovery beacon */
};

#define NUM_K_CHARS (sizeof(k_chars) / sizeof(k_chars[0]))

/* ---------------------------------------------------------------------------
 * Decode lookup table — 1024 entries indexed by 10-bit symbol
 *
 * Each entry contains:
 *   - data:  8-bit decoded value (byte for D-chars, K-byte for K-chars)
 *   - flags: bit 0 = is_k (K-character), bit 1 = error (invalid code)
 *
 * Built once at decoder init time from the encode tables. Any 10-bit
 * value not produced by a valid encoding is marked as an error.
 * --------------------------------------------------------------------------- */

#define DECODE_FLAG_K     0x01
#define DECODE_FLAG_ERR   0x02

typedef struct {
    uint8_t data;
    uint8_t flags;
} decode_entry_t;

static decode_entry_t decode_table[1024];
static bool decode_table_built = false;

static void build_decode_table(void) {
    if (decode_table_built) return;

    /* Mark all entries as errors initially */
    for (int i = 0; i < 1024; i++) {
        decode_table[i].data = 0;
        decode_table[i].flags = DECODE_FLAG_ERR;
    }

    /* Populate D-character entries.
     * For each (5b_value, 3b_value), try all combinations of 6b and 4b
     * disparity variants. Each valid combination produces a unique 10-bit
     * symbol that maps back to the original 8-bit data byte. */
    for (int d5 = 0; d5 < 32; d5++) {
        for (int d3 = 0; d3 < 8; d3++) {
            uint8_t byte_val = (uint8_t)((d3 << 5) | d5);

            for (int rd6 = 0; rd6 < 2; rd6++) {
                uint8_t code6 = encode_5b6b[d5][rd6];

                for (int rd4 = 0; rd4 < 2; rd4++) {
                    uint8_t code4 = encode_3b4b[d3][rd4];
                    uint16_t sym10 = ((uint16_t)code4 << 6) | code6;

                    if (sym10 < 1024) {
                        decode_table[sym10].data = byte_val;
                        decode_table[sym10].flags = 0;
                    }
                }
            }

            /* D.x.A7 alternate encodings for d3 == 7.
             * These are used for D.17.7, D.18.7, D.20.7 (RD-) and
             * D.11.7, D.13.7, D.14.7 (RD+) to avoid comma-like patterns.
             * All A7 variants must be decodable. */
            if (d3 == 7) {
                for (int rd6 = 0; rd6 < 2; rd6++) {
                    uint8_t code6 = encode_5b6b[d5][rd6];

                    uint16_t sym_a7m = ((uint16_t)A7_RD_MINUS << 6) | code6;
                    uint16_t sym_a7p = ((uint16_t)A7_RD_PLUS  << 6) | code6;

                    if (sym_a7m < 1024) {
                        decode_table[sym_a7m].data = byte_val;
                        decode_table[sym_a7m].flags = 0;
                    }
                    if (sym_a7p < 1024) {
                        decode_table[sym_a7p].data = byte_val;
                        decode_table[sym_a7p].flags = 0;
                    }
                }
            }
        }
    }

    /* Populate K-character entries (override any D-character collisions).
     * K-characters occupy code points that differ from all D-characters;
     * the override here is a safety measure. */
    for (int i = 0; i < (int)NUM_K_CHARS; i++) {
        uint16_t rdm = k_chars[i].code_rd_minus & 0x3FF;
        uint16_t rdp = k_chars[i].code_rd_plus  & 0x3FF;

        decode_table[rdm].data  = k_chars[i].byte_val;
        decode_table[rdm].flags = DECODE_FLAG_K;

        decode_table[rdp].data  = k_chars[i].byte_val;
        decode_table[rdp].flags = DECODE_FLAG_K;
    }

    decode_table_built = true;
}

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
 *
 * The 8-bit input is split into 5b (EDCBA = bits 4:0) and 3b (HGF = bits 7:5).
 * The 5b sub-block selects a 6-bit code, then the intermediate running
 * disparity is updated, and the 3b sub-block selects a 4-bit code.
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

    /* D.x.A7 alternate encoding (IEEE 802.3 Table 36-2 note).
     *
     * When encoding D.x.7, the alternate A7 code is used instead of
     * the primary P7 code for certain 5b values to prevent the creation
     * of comma-like bit patterns in the data stream:
     *   A7 with RD-: used for D.17.7, D.18.7, D.20.7
     *   A7 with RD+: used for D.11.7, D.13.7, D.14.7
     */
    if (high3 == 7) {
        if (enc->rd == RD_MINUS && (low5 == 17 || low5 == 18 || low5 == 20)) {
            code4 = A7_RD_MINUS;  /* 0x0E = fghj 0111 */
            ones4 = 3;
        } else if (enc->rd == RD_PLUS && (low5 == 11 || low5 == 13 || low5 == 14)) {
            code4 = A7_RD_PLUS;   /* 0x01 = fghj 1000 */
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
 *
 * K-characters are looked up from the k_chars[] table which contains
 * pre-composed 10-bit codes for each initial running disparity.
 * Disparity is updated based on the actual weight of the symbol.
 */
uint16_t abus_8b10b_encode_k(abus_8b10b_encoder_t *enc, uint8_t k_byte) {
    for (int i = 0; i < (int)NUM_K_CHARS; i++) {
        if (k_chars[i].byte_val == k_byte) {
            uint16_t code = (enc->rd == RD_MINUS) ?
                            k_chars[i].code_rd_minus :
                            k_chars[i].code_rd_plus;

            /* Update disparity based on the symbol's bit weight.
             * K28.x characters have 6 or 4 ones → always change disparity.
             * K.x.7 characters have 5 ones → balanced, no change. */
            int ones = 0;
            for (int b = 0; b < 10; b++) ones += (code >> b) & 1;
            if (ones > 5)      enc->rd = RD_PLUS;
            else if (ones < 5) enc->rd = RD_MINUS;
            /* ones == 5: K.x.7 characters, disparity unchanged */

            return code;
        }
    }
    /* Unknown K-character — return all-ones error symbol */
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

    /* Ensure the O(1) decode lookup table is built */
    build_decode_table();
}

/**
 * Decode a 10-bit symbol back to 8-bit data + type.
 *
 * Uses a 1024-entry O(1) lookup table built from the encode tables.
 * Returns the decoded symbol with is_k and error flags.
 */
abus_symbol_t abus_8b10b_decode(abus_8b10b_decoder_t *dec, uint16_t raw10) {
    abus_symbol_t sym = { .symbol = raw10, .is_k = 0, .error = 0 };
    uint16_t idx = raw10 & 0x3FF;

    const decode_entry_t *entry = &decode_table[idx];

    if (entry->flags & DECODE_FLAG_ERR) {
        sym.error = 1;
        return sym;
    }

    sym.symbol = entry->data;
    sym.is_k = (entry->flags & DECODE_FLAG_K) ? 1 : 0;

    /* Track comma for alignment */
    if (sym.is_k && entry->data == K28_5) {
        dec->comma_detected = true;
    }

    /* Update running disparity based on symbol weight */
    int ones = 0;
    for (int b = 0; b < 10; b++) ones += (idx >> b) & 1;
    if (ones > 5)      dec->rd = RD_PLUS;
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
 * Returns the index where the comma was found, or -1.
 * This is used during initial link synchronization.
 *
 * K28.5 codes in our format:
 *   RD- = 0x17C  (transmission pattern: 0011111010)
 *   RD+ = 0x283  (transmission pattern: 1100000101)
 */
int abus_8b10b_find_comma(const uint16_t *symbols, int len) {
    for (int i = 0; i < len; i++) {
        uint16_t s = symbols[i] & 0x3FF;
        if (s == 0x17C || s == 0x283) {
            return i;
        }
    }
    return -1;
}
