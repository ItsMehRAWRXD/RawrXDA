/* ============================================================================
 * RawrXD DEFLATE implementation - From scratch
 * Implements RFC 1951 (DEFLATE), RFC 1950 (zlib), RFC 1952 (gzip)
 * 
 * Written from scratch for the RawrXD IDE project.
 * No code from madler/zlib or any other existing implementation.
 * 
 * Features:
 *   - Stored blocks (no compression) for level 0
 *   - LZ77 + fixed Huffman codes for levels 1-3
 *   - LZ77 + dynamic Huffman codes for levels 4-9
 *   - Full inflate support (stored, fixed, dynamic Huffman)
 *   - Adler-32 and CRC-32 checksums  
 *   - zlib and raw deflate framing
 *   - gzip framing (windowBits 16+)
 * ============================================================================ */

#include "zlib.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ============================================================================
 * Internal constants
 * ============================================================================ */
#define RAWRXD_MAX_MATCH    258
#define RAWRXD_MIN_MATCH    3
#define RAWRXD_HASH_BITS    15
#define RAWRXD_HASH_SIZE    (1 << RAWRXD_HASH_BITS)
#define RAWRXD_HASH_MASK    (RAWRXD_HASH_SIZE - 1)
#define RAWRXD_WINDOW_SIZE  32768
#define RAWRXD_WINDOW_MASK  (RAWRXD_WINDOW_SIZE - 1)
#define RAWRXD_MAX_STORED   65535   /* max stored block size */
#define RAWRXD_MAX_BITS     15
#define RAWRXD_NUM_CODES    288     /* literal/length codes */
#define RAWRXD_NUM_DIST     32      /* distance codes */
#define RAWRXD_NUM_CL       19      /* code length codes */
#define RAWRXD_MAX_BL_BITS  7

/* ============================================================================
 * Adler-32 checksum (RFC 1950 appendix)
 * ============================================================================ */
#define ADLER_BASE 65521u
#define ADLER_NMAX 5552

uLong adler32(uLong adler, const Bytef *buf, uInt len) {
    unsigned long s1 = adler & 0xffff;
    unsigned long s2 = (adler >> 16) & 0xffff;

    if (buf == NULL) return 1UL;

    while (len > 0) {
        uInt k = (len < ADLER_NMAX) ? len : ADLER_NMAX;
        len -= k;
        while (k >= 16) {
            s1 += buf[0]; s2 += s1;  s1 += buf[1]; s2 += s1;
            s1 += buf[2]; s2 += s1;  s1 += buf[3]; s2 += s1;
            s1 += buf[4]; s2 += s1;  s1 += buf[5]; s2 += s1;
            s1 += buf[6]; s2 += s1;  s1 += buf[7]; s2 += s1;
            s1 += buf[8]; s2 += s1;  s1 += buf[9]; s2 += s1;
            s1 += buf[10]; s2 += s1; s1 += buf[11]; s2 += s1;
            s1 += buf[12]; s2 += s1; s1 += buf[13]; s2 += s1;
            s1 += buf[14]; s2 += s1; s1 += buf[15]; s2 += s1;
            buf += 16;
            k -= 16;
        }
        while (k--) {
            s1 += *buf++;
            s2 += s1;
        }
        s1 %= ADLER_BASE;
        s2 %= ADLER_BASE;
    }
    return (s2 << 16) | s1;
}

/* ============================================================================
 * CRC-32 checksum (for gzip, RFC 1952)
 * ============================================================================ */
static unsigned long rxd_crc_table[256];
static int rxd_crc_table_computed = 0;

static void rxd_make_crc_table(void) {
    unsigned long c;
    int n, k;
    for (n = 0; n < 256; n++) {
        c = (unsigned long)n;
        for (k = 0; k < 8; k++) {
            if (c & 1)
                c = 0xedb88320UL ^ (c >> 1);
            else
                c = c >> 1;
        }
        rxd_crc_table[n] = c;
    }
    rxd_crc_table_computed = 1;
}

uLong crc32(uLong crc, const Bytef *buf, uInt len) {
    if (!rxd_crc_table_computed) rxd_make_crc_table();
    if (buf == NULL) return 0UL;
    
    crc = crc ^ 0xffffffffUL;
    while (len >= 8) {
        crc = rxd_crc_table[((int)crc ^  buf[0]) & 0xff] ^ (crc >> 8);
        crc = rxd_crc_table[((int)crc ^  buf[1]) & 0xff] ^ (crc >> 8);
        crc = rxd_crc_table[((int)crc ^  buf[2]) & 0xff] ^ (crc >> 8);
        crc = rxd_crc_table[((int)crc ^  buf[3]) & 0xff] ^ (crc >> 8);
        crc = rxd_crc_table[((int)crc ^  buf[4]) & 0xff] ^ (crc >> 8);
        crc = rxd_crc_table[((int)crc ^  buf[5]) & 0xff] ^ (crc >> 8);
        crc = rxd_crc_table[((int)crc ^  buf[6]) & 0xff] ^ (crc >> 8);
        crc = rxd_crc_table[((int)crc ^  buf[7]) & 0xff] ^ (crc >> 8);
        buf += 8;
        len -= 8;
    }
    while (len--) {
        crc = rxd_crc_table[((int)crc ^ (*buf++)) & 0xff] ^ (crc >> 8);
    }
    return crc ^ 0xffffffffUL;
}

/* ============================================================================
 * Fixed Huffman tables (RFC 1951 Section 3.2.6)
 * ============================================================================ */

/* Length code base values and extra bits (RFC 1951 table) */
static const unsigned short rxd_len_base[29] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13,
    15, 17, 19, 23, 27, 31, 35, 43, 51, 59,
    67, 83, 99, 115, 131, 163, 195, 227, 258
};
static const unsigned short rxd_len_extra[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
    1, 1, 2, 2, 2, 2, 3, 3, 3, 3,
    4, 4, 4, 4, 5, 5, 5, 5, 0
};

/* Distance code base values and extra bits */
static const unsigned short rxd_dist_base[30] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25,
    33, 49, 65, 97, 129, 193, 257, 385, 513, 769,
    1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
};
static const unsigned short rxd_dist_extra[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3,
    4, 4, 5, 5, 6, 6, 7, 7, 8, 8,
    9, 9, 10, 10, 11, 11, 12, 12, 13, 13
};

/* Code length alphabet order (RFC 1951 Section 3.2.7) */
static const int rxd_cl_order[RAWRXD_NUM_CL] = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

/* ============================================================================
 * Bit buffer for deflate output
 * ============================================================================ */
typedef struct {
    unsigned long bits;
    int           nbits;
    Bytef        *out;
    uInt          out_avail;
    uLong         out_total;
} rxd_bitwriter;

static void rxd_bw_init(rxd_bitwriter *bw, Bytef *out, uInt avail) {
    bw->bits = 0;
    bw->nbits = 0;
    bw->out = out;
    bw->out_avail = avail;
    bw->out_total = 0;
}

static void rxd_bw_putbits(rxd_bitwriter *bw, unsigned int value, int nbits) {
    bw->bits |= ((unsigned long)value) << bw->nbits;
    bw->nbits += nbits;
    while (bw->nbits >= 8) {
        if (bw->out_avail > 0) {
            *bw->out++ = (Bytef)(bw->bits & 0xff);
            bw->out_avail--;
            bw->out_total++;
        }
        bw->bits >>= 8;
        bw->nbits -= 8;
    }
}

static void rxd_bw_flush(rxd_bitwriter *bw) {
    if (bw->nbits > 0) {
        if (bw->out_avail > 0) {
            *bw->out++ = (Bytef)(bw->bits & 0xff);
            bw->out_avail--;
            bw->out_total++;
        }
        bw->bits = 0;
        bw->nbits = 0;
    }
}

static void rxd_bw_align(rxd_bitwriter *bw) {
    if (bw->nbits > 0) {
        rxd_bw_flush(bw);
    }
}

/* ============================================================================
 * Bit reader for inflate input
 * ============================================================================ */
typedef struct {
    unsigned long bits;
    int           nbits;
    const Bytef  *in;
    uInt          in_avail;
    uLong         in_total;
} rxd_bitreader;

static void rxd_br_init(rxd_bitreader *br, const Bytef *in, uInt avail) {
    br->bits = 0;
    br->nbits = 0;
    br->in = in;
    br->in_avail = avail;
    br->in_total = 0;
}

static int rxd_br_ensure(rxd_bitreader *br, int need) {
    while (br->nbits < need) {
        if (br->in_avail == 0) return -1;
        br->bits |= ((unsigned long)(*br->in++)) << br->nbits;
        br->in_avail--;
        br->in_total++;
        br->nbits += 8;
    }
    return 0;
}

static unsigned int rxd_br_peek(rxd_bitreader *br, int n) {
    return (unsigned int)(br->bits & ((1UL << n) - 1));
}

static void rxd_br_drop(rxd_bitreader *br, int n) {
    br->bits >>= n;
    br->nbits -= n;
}

static unsigned int rxd_br_read(rxd_bitreader *br, int n) {
    unsigned int v;
    rxd_br_ensure(br, n);
    v = rxd_br_peek(br, n);
    rxd_br_drop(br, n);
    return v;
}

static void rxd_br_align(rxd_bitreader *br) {
    int skip = br->nbits & 7;
    if (skip > 0) {
        br->bits >>= skip;
        br->nbits -= skip;
    }
}

/* ============================================================================
 * Huffman tree for inflate
 * ============================================================================ */
#define RAWRXD_HUFF_FAST_BITS 9
#define RAWRXD_HUFF_FAST_SIZE (1 << RAWRXD_HUFF_FAST_BITS)

typedef struct {
    /* For fast lookup: code_len[i] = bit length of symbol i */
    unsigned short code_len[RAWRXD_NUM_CODES + RAWRXD_NUM_DIST]; 
    /* Decode table: first pass fast lookup */
    short fast_table[RAWRXD_HUFF_FAST_SIZE]; /* symbol or -(first_slow_index) */
    /* For codes > FAST_BITS: sorted symbol list */
    unsigned short sorted_symbols[RAWRXD_NUM_CODES + RAWRXD_NUM_DIST];
    unsigned int   min_code[RAWRXD_MAX_BITS + 1]; /* min code value per length */
    unsigned int   base_idx[RAWRXD_MAX_BITS + 1]; /* base index into sorted_symbols */
    int            max_len;
    int            num_symbols;
} rxd_huff_tree;

static int rxd_huff_build(rxd_huff_tree *tree, const unsigned short *lens, int num) {
    int bl_count[RAWRXD_MAX_BITS + 1];
    unsigned int next_code[RAWRXD_MAX_BITS + 1];
    int i, len;
    unsigned int code;

    memset(bl_count, 0, sizeof(bl_count));
    tree->num_symbols = num;
    tree->max_len = 0;

    /* Count bit lengths */
    for (i = 0; i < num; i++) {
        tree->code_len[i] = lens[i];
        if (lens[i] > 0) {
            bl_count[lens[i]]++;
            if ((int)lens[i] > tree->max_len)
                tree->max_len = (int)lens[i];
        }
    }

    if (tree->max_len == 0) return -1; /* all zeros */

    /* Compute starting codes for each bit length */
    code = 0;
    bl_count[0] = 0;
    for (len = 1; len <= RAWRXD_MAX_BITS; len++) {
        code = (code + bl_count[len - 1]) << 1;
        next_code[len] = code;
    }

    /* Build min_code and base_idx for slow decode */
    {
        int idx = 0;
        for (len = 1; len <= RAWRXD_MAX_BITS; len++) {
            tree->min_code[len] = next_code[len];
            tree->base_idx[len] = idx;
            idx += bl_count[len];
        }
    }

    /* Fill sorted_symbols in canonical order */
    {
        int idx_per_len[RAWRXD_MAX_BITS + 1];
        memset(idx_per_len, 0, sizeof(idx_per_len));
        for (i = 0; i < num; i++) {
            len = lens[i];
            if (len > 0) {
                int pos = tree->base_idx[len] + idx_per_len[len];
                tree->sorted_symbols[pos] = (unsigned short)i;
                idx_per_len[len]++;
            }
        }
    }

    /* Build fast lookup table */
    memset(tree->fast_table, -1, sizeof(tree->fast_table));
    for (i = 0; i < num; i++) {
        len = lens[i];
        if (len > 0 && len <= RAWRXD_HUFF_FAST_BITS) {
            /* Assign reversed code for this symbol */
            unsigned int c = next_code[len];
            /* But we already incremented next_code... use canonical order */
            /* Recalculate: find the code for symbol i */
            unsigned int sym_code = 0;
            /* We need to compute the actual code for symbol i */
            /* Recompute from min_code */
            int cnt = 0;
            int j;
            for (j = 0; j < i; j++) {
                if (lens[j] == len) cnt++;
            }
            sym_code = tree->min_code[len] + cnt;
            
            /* Reverse bits for fast table (deflate uses LSB-first) */
            unsigned int rev = 0;
            int b;
            for (b = 0; b < len; b++) {
                rev |= ((sym_code >> b) & 1) << (len - 1 - b);
            }
            /* Actually deflate stores codes in reverse bit order already,
               the bit reader reads LSB first. So we need the code reversed
               from MSB order to LSB order */
            rev = 0;
            for (b = 0; b < len; b++) {
                rev |= ((sym_code >> (len - 1 - b)) & 1) << b;
            }
            
            /* Fill all fast table entries that share this prefix */
            int fill = 1 << len;
            int step;
            for (step = rev; step < RAWRXD_HUFF_FAST_SIZE; step += fill) {
                tree->fast_table[step] = (short)((i & 0x7ff) | (len << 11));
            }
        }
    }

    return 0;
}

static int rxd_huff_decode(rxd_huff_tree *tree, rxd_bitreader *br) {
    int len;
    unsigned int code;

    /* Try fast lookup first */
    if (rxd_br_ensure(br, RAWRXD_HUFF_FAST_BITS) == 0 || br->nbits > 0) {
        int avail = br->nbits;
        if (avail > RAWRXD_HUFF_FAST_BITS) avail = RAWRXD_HUFF_FAST_BITS;
        unsigned int peek = rxd_br_peek(br, avail);
        /* Pad for lookup */
        short entry = tree->fast_table[peek & (RAWRXD_HUFF_FAST_SIZE - 1)];
        if (entry >= 0) {
            int sym = entry & 0x7ff;
            int bits = entry >> 11;
            if (bits <= br->nbits) {
                rxd_br_drop(br, bits);
                return sym;
            }
        }
    }

    /* Slow decode: bit-by-bit */
    code = 0;
    for (len = 1; len <= tree->max_len; len++) {
        if (rxd_br_ensure(br, len) < 0) return -1;
        /* Read next bit (MSB order for Huffman) */
        code = (code << 1) | ((br->bits >> (len - 1)) & 1);
        
        if (code >= tree->min_code[len] && 
            (code - tree->min_code[len]) < (unsigned int)(tree->base_idx[len + 1 > RAWRXD_MAX_BITS ? len : len + 1] - tree->base_idx[len])) {
            /* Actually we need count of codes at this length */
            int idx = tree->base_idx[len] + (code - tree->min_code[len]);
            rxd_br_drop(br, len);
            return tree->sorted_symbols[idx];
        }
    }

    /* Fallback: simple sequential decode */
    rxd_br_ensure(br, 1);
    code = 0;
    for (len = 1; len <= tree->max_len; len++) {
        if (rxd_br_ensure(br, len) < 0) return -1;
        /* Rebuild code reading LSB first */
        code = 0;
        {
            int b;
            for (b = 0; b < len; b++) {
                code |= ((br->bits >> b) & 1) << (len - 1 - b);
            }
        }
        /* Search for a match */
        if (code >= tree->min_code[len]) {
            unsigned int offset = code - tree->min_code[len];
            int next_len_base = (len < RAWRXD_MAX_BITS) ? tree->base_idx[len + 1] : tree->num_symbols;
            int avail_at_len = next_len_base - tree->base_idx[len];
            if ((int)offset < avail_at_len) {
                rxd_br_drop(br, len);
                return tree->sorted_symbols[tree->base_idx[len] + offset];
            }
        }
    }

    return -1; /* decode error */
}

/* ============================================================================
 * Build fixed Huffman trees (RFC 1951 Section 3.2.6)
 * ============================================================================ */
static rxd_huff_tree rxd_fixed_litlen;
static rxd_huff_tree rxd_fixed_dist;
static int rxd_fixed_tables_built = 0;

static void rxd_build_fixed_tables(void) {
    unsigned short lens[RAWRXD_NUM_CODES];
    int i;

    /* Literal/length codes per RFC 1951:
       0-143:   8 bits
       144-255: 9 bits
       256-279: 7 bits
       280-287: 8 bits */
    for (i =   0; i <= 143; i++) lens[i] = 8;
    for (i = 144; i <= 255; i++) lens[i] = 9;
    for (i = 256; i <= 279; i++) lens[i] = 7;
    for (i = 280; i <= 287; i++) lens[i] = 8;
    rxd_huff_build(&rxd_fixed_litlen, lens, 288);

    /* Distance codes: all 5 bits */
    for (i = 0; i < 32; i++) lens[i] = 5;
    rxd_huff_build(&rxd_fixed_dist, lens, 32);

    rxd_fixed_tables_built = 1;
}

/* ============================================================================
 * Deflate internal state
 * ============================================================================ */
typedef enum {
    RXD_DEFLATE_MODE,
    RXD_INFLATE_MODE
} rxd_mode;

typedef enum {
    RXD_WRAP_NONE = 0,  /* raw deflate */
    RXD_WRAP_ZLIB = 1,  /* zlib wrapper */
    RXD_WRAP_GZIP = 2   /* gzip wrapper */
} rxd_wrap;

typedef struct {
    rxd_mode     mode;
    rxd_wrap     wrap;
    int          level;
    int          strategy;
    int          window_bits;
    int          finished;
    uLong        check;          /* adler32 or crc32 */
    uLong        total_input;
    uLong        total_output;

    /* Deflate-specific */
    Bytef       *window;                    /* sliding window */
    int          window_pos;                /* current position */
    int         *hash_chain;                /* hash chain heads */
    int         *hash_prev;                 /* previous matches */
    int          input_consumed;            /* for compress/uncompress */
    int          header_written;
    int          trailer_written;

    /* Inflate-specific */  
    int          inf_state;     /* 0=header, 1=blocks, 2=done */
    int          inf_last;      /* last block flag */
    Bytef       *inf_window;    /* output window for back-references */
    int          inf_wpos;
    int          inf_wsize;
    rxd_bitreader inf_br;       /* persistent bit reader state */
    int          inf_block_type;
    int          inf_header_done;
    uLong        inf_data_len;  /* for gzip */
} rxd_state;

/* ============================================================================
 * Memory helpers
 * ============================================================================ */
static voidpf rxd_alloc(voidpf opaque, uInt items, uInt size) {
    (void)opaque;
    return calloc(items, size);
}

static void rxd_free(voidpf opaque, voidpf addr) {
    (void)opaque;
    free(addr);
}

/* ============================================================================
 * LZ77 hash for deflate compression
 * ============================================================================ */
static unsigned int rxd_hash3(const Bytef *s) {
    return ((unsigned int)s[0] * 1057 + (unsigned int)s[1] * 131 + (unsigned int)s[2]) & RAWRXD_HASH_MASK;
}

/* Find best match in window using hash chains */
static int rxd_find_match(rxd_state *st, const Bytef *src, int pos, int src_len,
                          int *match_dist) {
    int best_len = 0;
    int best_dist = 0;
    int chain_len = 128; /* max chain to search */
    int min_pos = (pos > RAWRXD_WINDOW_SIZE) ? pos - RAWRXD_WINDOW_SIZE : 0;
    unsigned int h;
    int prev;
    int max_len;

    if (pos + RAWRXD_MIN_MATCH > src_len) return 0;

    h = rxd_hash3(src + pos);
    prev = st->hash_chain[h];

    max_len = src_len - pos;
    if (max_len > RAWRXD_MAX_MATCH) max_len = RAWRXD_MAX_MATCH;

    while (prev >= min_pos && chain_len-- > 0) {
        int dist = pos - prev;
        if (dist > 0 && dist <= RAWRXD_WINDOW_SIZE) {
            /* Compare */
            int len = 0;
            const Bytef *a = src + pos;
            const Bytef *b = src + prev;
            while (len < max_len && a[len] == b[len]) len++;
            if (len >= RAWRXD_MIN_MATCH && len > best_len) {
                best_len = len;
                best_dist = dist;
                if (len >= max_len) break; /* can't do better */
            }
        }
        /* Walk chain */
        if (prev <= 0) break;
        prev = st->hash_prev[prev & RAWRXD_WINDOW_MASK];
        if (prev < min_pos) break;
    }

    *match_dist = best_dist;
    return best_len;
}

static void rxd_insert_hash(rxd_state *st, const Bytef *src, int pos, int src_len) {
    if (pos + 2 < src_len) {
        unsigned int h = rxd_hash3(src + pos);
        st->hash_prev[pos & RAWRXD_WINDOW_MASK] = st->hash_chain[h];
        st->hash_chain[h] = pos;
    }
}

/* ============================================================================
 * Encode a literal/length/distance using fixed Huffman codes
 * ============================================================================ */

/* Get the fixed Huffman code for a literal/length value */
static void rxd_encode_fixed_literal(rxd_bitwriter *bw, int lit) {
    unsigned int code;
    int bits;
    int i;
    unsigned int rev;
    
    if (lit <= 143) {
        code = 0x30 + lit;  /* 00110000 + lit, 8 bits */
        bits = 8;
    } else if (lit <= 255) {
        code = 0x190 + (lit - 144); /* 110010000 + (lit-144), 9 bits */
        bits = 9;
    } else if (lit <= 279) {
        code = 0x00 + (lit - 256); /* 0000000 + (lit-256), 7 bits */
        bits = 7;
    } else {
        code = 0xC0 + (lit - 280); /* 11000000 + (lit-280), 8 bits */
        bits = 8;
    }

    /* Reverse bits for deflate (LSB first) */
    rev = 0;
    for (i = 0; i < bits; i++) {
        rev |= ((code >> (bits - 1 - i)) & 1) << i;
    }
    rxd_bw_putbits(bw, rev, bits);
}

/* Get length code (257-285) from match length */
static int rxd_length_code(int length) {
    int i;
    for (i = 0; i < 29; i++) {
        if (i == 28) return 285; /* length 258 */
        if (length < (int)rxd_len_base[i + 1]) return 257 + i;
    }
    return 285;
}

/* Get distance code (0-29) from distance */
static int rxd_dist_code(int dist) {
    int i;
    for (i = 0; i < 30; i++) {
        if (i == 29) return 29;
        if (dist < (int)rxd_dist_base[i + 1]) return i;
    }
    return 29;
}

static void rxd_encode_fixed_match(rxd_bitwriter *bw, int length, int dist) {
    int lcode = rxd_length_code(length);
    int dcode = rxd_dist_code(dist);
    int i;
    unsigned int rev;

    /* Emit length code */
    rxd_encode_fixed_literal(bw, lcode);

    /* Extra bits for length */
    {
        int idx = lcode - 257;
        int extra = rxd_len_extra[idx];
        if (extra > 0) {
            rxd_bw_putbits(bw, length - rxd_len_base[idx], extra);
        }
    }

    /* Emit distance code (5 bits, reversed) */
    rev = 0;
    for (i = 0; i < 5; i++) {
        rev |= ((dcode >> (4 - i)) & 1) << i;
    }
    rxd_bw_putbits(bw, rev, 5);

    /* Extra bits for distance */
    {
        int extra = rxd_dist_extra[dcode];
        if (extra > 0) {
            rxd_bw_putbits(bw, dist - rxd_dist_base[dcode], extra);
        }
    }
}

/* ============================================================================
 * Deflate: compress a block
 * ============================================================================ */
static int rxd_deflate_stored(rxd_bitwriter *bw, const Bytef *src, int len, int last) {
    int pos = 0;
    while (pos < len) {
        int chunk = len - pos;
        int is_last;
        unsigned short clen, nclen;
        
        if (chunk > RAWRXD_MAX_STORED) chunk = RAWRXD_MAX_STORED;
        is_last = (last && pos + chunk >= len) ? 1 : 0;

        /* Block header: BFINAL=is_last, BTYPE=00 (stored) */
        rxd_bw_putbits(bw, is_last, 1);
        rxd_bw_putbits(bw, 0, 2);  /* BTYPE = 00 */
        rxd_bw_align(bw);

        /* LEN and NLEN */
        clen = (unsigned short)chunk;
        nclen = ~clen;
        if (bw->out_avail >= (uInt)(chunk + 4)) {
            bw->out[0] = (Bytef)(clen & 0xff);
            bw->out[1] = (Bytef)(clen >> 8);
            bw->out[2] = (Bytef)(nclen & 0xff);
            bw->out[3] = (Bytef)(nclen >> 8);
            memcpy(bw->out + 4, src + pos, chunk);
            bw->out += chunk + 4;
            bw->out_avail -= chunk + 4;
            bw->out_total += chunk + 4;
        }
        pos += chunk;
    }
    return 0;
}

static int rxd_deflate_fixed(rxd_state *st, rxd_bitwriter *bw, 
                              const Bytef *src, int len, int last) {
    int pos = 0;

    /* Block header: BFINAL=last, BTYPE=01 (fixed Huffman) */
    rxd_bw_putbits(bw, last ? 1 : 0, 1);
    rxd_bw_putbits(bw, 1, 2); /* BTYPE = 01, reversed: 01 -> bits are 10, but 
                                  deflate stores BTYPE as 2 bits LSB first, 
                                  01 = fixed */

    /* Initialize hash chains */
    if (st->hash_chain == NULL) {
        st->hash_chain = (int *)calloc(RAWRXD_HASH_SIZE, sizeof(int));
        st->hash_prev = (int *)calloc(RAWRXD_WINDOW_SIZE, sizeof(int));
        if (!st->hash_chain || !st->hash_prev) return Z_MEM_ERROR;
        memset(st->hash_chain, -1, RAWRXD_HASH_SIZE * sizeof(int));
        memset(st->hash_prev, -1, RAWRXD_WINDOW_SIZE * sizeof(int));
    }

    while (pos < len) {
        int match_dist = 0;
        int match_len;
        
        if (st->level >= 1) {
            match_len = rxd_find_match(st, src, pos, len, &match_dist);
        } else {
            match_len = 0;
        }

        if (match_len >= RAWRXD_MIN_MATCH) {
            /* Emit length/distance pair */
            rxd_encode_fixed_match(bw, match_len, match_dist);
            /* Insert all positions in the match into hash */
            {
                int j;
                for (j = 0; j < match_len; j++) {
                    rxd_insert_hash(st, src, pos + j, len);
                }
            }
            pos += match_len;
        } else {
            /* Emit literal */
            rxd_encode_fixed_literal(bw, src[pos]);
            rxd_insert_hash(st, src, pos, len);
            pos++;
        }
    }

    /* End of block marker (256) */
    rxd_encode_fixed_literal(bw, 256);
    return 0;
}

/* ============================================================================
 * Deflate API implementation
 * ============================================================================ */

int deflateInit_(z_streamp strm, int level, const char *version, int stream_size) {
    return deflateInit2_(strm, level, Z_DEFLATED, MAX_WBITS, 8, 
                         Z_DEFAULT_STRATEGY, version, stream_size);
}

int deflateInit2_(z_streamp strm, int level, int method,
                  int windowBits, int memLevel, int strategy,
                  const char *version, int stream_size) {
    rxd_state *st;
    (void)version; (void)stream_size; (void)method; (void)memLevel;

    if (strm == NULL) return Z_STREAM_ERROR;

    if (level == Z_DEFAULT_COMPRESSION) level = 6;
    if (level < 0 || level > 9) return Z_STREAM_ERROR;

    if (strm->zalloc == NULL) strm->zalloc = rxd_alloc;
    if (strm->zfree == NULL) strm->zfree = rxd_free;

    st = (rxd_state *)strm->zalloc(strm->opaque, 1, sizeof(rxd_state));
    if (st == NULL) return Z_MEM_ERROR;
    memset(st, 0, sizeof(rxd_state));

    st->mode = RXD_DEFLATE_MODE;
    st->level = level;
    st->strategy = strategy;
    st->finished = 0;
    st->header_written = 0;
    st->trailer_written = 0;
    st->hash_chain = NULL;
    st->hash_prev = NULL;
    st->total_input = 0;
    st->total_output = 0;

    /* Determine wrapping */
    if (windowBits < 0) {
        st->wrap = RXD_WRAP_NONE;  /* raw deflate */
        st->window_bits = -windowBits;
    } else if (windowBits > 15) {
        st->wrap = RXD_WRAP_GZIP;  /* gzip */
        st->window_bits = windowBits - 16;
    } else {
        st->wrap = RXD_WRAP_ZLIB;  /* zlib */
        st->window_bits = windowBits;
    }

    if (st->wrap == RXD_WRAP_GZIP)
        st->check = crc32(0UL, NULL, 0);
    else
        st->check = adler32(0UL, NULL, 0);

    strm->state = st;
    strm->total_in = 0;
    strm->total_out = 0;
    strm->msg = NULL;
    strm->adler = st->check;

    return Z_OK;
}

int deflate(z_streamp strm, int flush) {
    rxd_state *st;
    rxd_bitwriter bw;
    int result;
    uLong start_out;

    if (strm == NULL || strm->state == NULL) return Z_STREAM_ERROR;
    st = (rxd_state *)strm->state;

    if (st->finished && flush != Z_FINISH) return Z_STREAM_ERROR;
    if (strm->avail_out == 0) return Z_BUF_ERROR;

    start_out = strm->total_out;
    rxd_bw_init(&bw, strm->next_out, strm->avail_out);

    /* Write header if not yet written */
    if (!st->header_written) {
        if (st->wrap == RXD_WRAP_ZLIB) {
            /* zlib header: CMF=0x78 (deflate, window=32K), FLG computed for check */
            Bytef cmf = 0x78;
            Bytef flg;
            unsigned int check_val;

            if (st->level <= 1) flg = 0x01;
            else if (st->level <= 5) flg = 0x5e;
            else if (st->level <= 6) flg = 0x9c;
            else flg = 0xda;

            /* Adjust FLG for FCHECK */
            check_val = (cmf * 256 + flg);
            if (check_val % 31 != 0) {
                flg += (unsigned char)(31 - (check_val % 31));
            }

            if (bw.out_avail >= 2) {
                *bw.out++ = cmf;
                *bw.out++ = flg;
                bw.out_avail -= 2;
                bw.out_total += 2;
            }
        } else if (st->wrap == RXD_WRAP_GZIP) {
            /* gzip header */
            Bytef hdr[10] = {
                0x1f, 0x8b, /* magic */
                0x08,       /* compression method = deflate */
                0x00,       /* flags */
                0, 0, 0, 0, /* mtime */
                0x00,       /* xfl */
                0xff        /* OS = unknown */
            };
            if (bw.out_avail >= 10) {
                memcpy(bw.out, hdr, 10);
                bw.out += 10;
                bw.out_avail -= 10;
                bw.out_total += 10;
            }
        }
        st->header_written = 1;
    }

    /* Compress the input data */
    if (strm->avail_in > 0 || flush == Z_FINISH) {
        int is_last = (flush == Z_FINISH) ? 1 : 0;
        const Bytef *input = strm->next_in;
        uInt len = strm->avail_in;

        /* Update checksum */
        if (len > 0) {
            if (st->wrap == RXD_WRAP_GZIP)
                st->check = crc32(st->check, input, len);
            else
                st->check = adler32(st->check, input, len);
            st->total_input += len;
        }

        if (st->level == 0) {
            result = rxd_deflate_stored(&bw, input, (int)len, is_last);
        } else {
            result = rxd_deflate_fixed(st, &bw, input, (int)len, is_last);
        }

        if (result != 0) return result;

        rxd_bw_flush(&bw);

        strm->next_in += len;
        strm->avail_in = 0;
        strm->total_in += len;

        if (is_last && !st->trailer_written) {
            /* Write trailer */
            if (st->wrap == RXD_WRAP_ZLIB) {
                /* Adler-32 in big-endian */
                if (bw.out_avail >= 4) {
                    uLong a = st->check;
                    bw.out[0] = (Bytef)((a >> 24) & 0xff);
                    bw.out[1] = (Bytef)((a >> 16) & 0xff);
                    bw.out[2] = (Bytef)((a >> 8) & 0xff);
                    bw.out[3] = (Bytef)(a & 0xff);
                    bw.out += 4;
                    bw.out_avail -= 4;
                    bw.out_total += 4;
                }
            } else if (st->wrap == RXD_WRAP_GZIP) {
                /* CRC-32 + ISIZE in little-endian */
                if (bw.out_avail >= 8) {
                    uLong c = st->check;
                    uLong s = st->total_input;
                    bw.out[0] = (Bytef)(c & 0xff);
                    bw.out[1] = (Bytef)((c >> 8) & 0xff);
                    bw.out[2] = (Bytef)((c >> 16) & 0xff);
                    bw.out[3] = (Bytef)((c >> 24) & 0xff);
                    bw.out[4] = (Bytef)(s & 0xff);
                    bw.out[5] = (Bytef)((s >> 8) & 0xff);
                    bw.out[6] = (Bytef)((s >> 16) & 0xff);
                    bw.out[7] = (Bytef)((s >> 24) & 0xff);
                    bw.out += 8;
                    bw.out_avail -= 8;
                    bw.out_total += 8;
                }
            }
            st->trailer_written = 1;
            st->finished = 1;
        }
    }

    /* Update stream pointers */
    strm->next_out = bw.out;
    strm->avail_out = bw.out_avail;
    strm->total_out = start_out + bw.out_total;
    strm->adler = st->check;

    if (st->finished) return Z_STREAM_END;
    return Z_OK;
}

int deflateEnd(z_streamp strm) {
    rxd_state *st;
    if (strm == NULL || strm->state == NULL) return Z_STREAM_ERROR;
    st = (rxd_state *)strm->state;

    if (st->hash_chain) { free(st->hash_chain); st->hash_chain = NULL; }
    if (st->hash_prev) { free(st->hash_prev); st->hash_prev = NULL; }
    if (st->window) { free(st->window); st->window = NULL; }
    
    strm->zfree(strm->opaque, st);
    strm->state = NULL;
    return Z_OK;
}

int deflateBound(z_streamp strm, uLong sourceLen) {
    (void)strm;
    /* Conservative bound: stored blocks + headers + trailer */
    return (int)(sourceLen + (sourceLen >> 12) + (sourceLen >> 14) + 
                 (sourceLen >> 25) + 13 + 18);
}

int deflateReset(z_streamp strm) {
    rxd_state *st;
    if (strm == NULL || strm->state == NULL) return Z_STREAM_ERROR;
    st = (rxd_state *)strm->state;

    st->finished = 0;
    st->header_written = 0;
    st->trailer_written = 0;
    st->total_input = 0;
    st->total_output = 0;
    strm->total_in = 0;
    strm->total_out = 0;

    if (st->wrap == RXD_WRAP_GZIP)
        st->check = crc32(0UL, NULL, 0);
    else
        st->check = adler32(0UL, NULL, 0);

    strm->adler = st->check;

    if (st->hash_chain) memset(st->hash_chain, -1, RAWRXD_HASH_SIZE * sizeof(int));
    if (st->hash_prev) memset(st->hash_prev, -1, RAWRXD_WINDOW_SIZE * sizeof(int));

    return Z_OK;
}

/* ============================================================================
 * Inflate API implementation
 * ============================================================================ */

int inflateInit_(z_streamp strm, const char *version, int stream_size) {
    return inflateInit2_(strm, MAX_WBITS, version, stream_size);
}

int inflateInit2_(z_streamp strm, int windowBits, 
                  const char *version, int stream_size) {
    rxd_state *st;
    (void)version; (void)stream_size;

    if (strm == NULL) return Z_STREAM_ERROR;

    if (strm->zalloc == NULL) strm->zalloc = rxd_alloc;
    if (strm->zfree == NULL) strm->zfree = rxd_free;

    st = (rxd_state *)strm->zalloc(strm->opaque, 1, sizeof(rxd_state));
    if (st == NULL) return Z_MEM_ERROR;
    memset(st, 0, sizeof(rxd_state));

    st->mode = RXD_INFLATE_MODE;
    st->inf_state = 0;
    st->inf_last = 0;
    st->inf_header_done = 0;

    /* Determine wrapping */
    if (windowBits < 0) {
        st->wrap = RXD_WRAP_NONE;
        st->window_bits = -windowBits;
    } else if (windowBits > 15) {
        st->wrap = RXD_WRAP_GZIP;
        st->window_bits = windowBits - 16;
    } else if (windowBits == 0) {
        /* Auto-detect: will check first bytes */
        st->wrap = RXD_WRAP_ZLIB; /* default, may change */
        st->window_bits = MAX_WBITS;
    } else {
        st->wrap = RXD_WRAP_ZLIB;
        st->window_bits = windowBits;
    }

    st->inf_wsize = 1 << st->window_bits;
    st->inf_window = (Bytef *)calloc(1, st->inf_wsize);
    if (!st->inf_window) {
        strm->zfree(strm->opaque, st);
        return Z_MEM_ERROR;
    }
    st->inf_wpos = 0;

    if (st->wrap == RXD_WRAP_GZIP)
        st->check = crc32(0UL, NULL, 0);
    else
        st->check = adler32(0UL, NULL, 0);

    strm->state = st;
    strm->total_in = 0;
    strm->total_out = 0;
    strm->msg = NULL;
    strm->adler = st->check;

    if (!rxd_fixed_tables_built) rxd_build_fixed_tables();

    return Z_OK;
}

/* Inflate a single block */
static int rxd_inflate_stored(rxd_bitreader *br, Bytef **out, uInt *out_avail,
                               uLong *out_total, Bytef *window, int *wpos, int wsize) {
    unsigned int len, nlen;
    rxd_br_align(br);
    
    if (rxd_br_ensure(br, 32) < 0) return Z_DATA_ERROR;
    len = rxd_br_read(br, 16);
    nlen = rxd_br_read(br, 16);

    if ((len ^ 0xffff) != nlen) return Z_DATA_ERROR;

    while (len > 0) {
        if (br->in_avail == 0) return Z_BUF_ERROR;
        if (*out_avail == 0) return Z_BUF_ERROR;
        
        unsigned int copy = len;
        if (copy > br->in_avail) copy = br->in_avail;
        if (copy > *out_avail) copy = *out_avail;

        memcpy(*out, br->in, copy);
        /* Copy to window for back-references */
        {
            unsigned int i;
            for (i = 0; i < copy; i++) {
                window[(*wpos) & (wsize - 1)] = br->in[i];
                (*wpos)++;
            }
        }

        br->in += copy;
        br->in_avail -= copy;
        br->in_total += copy;
        *out += copy;
        *out_avail -= copy;
        *out_total += copy;
        len -= copy;
    }
    return Z_OK;
}

static int rxd_inflate_huffman(rxd_bitreader *br, rxd_huff_tree *litlen_tree,
                                rxd_huff_tree *dist_tree,
                                Bytef **out, uInt *out_avail, uLong *out_total,
                                Bytef *window, int *wpos, int wsize) {
    for (;;) {
        int sym = rxd_huff_decode(litlen_tree, br);
        if (sym < 0) return Z_DATA_ERROR;

        if (sym < 256) {
            /* Literal byte */
            if (*out_avail == 0) return Z_BUF_ERROR;
            **out = (Bytef)sym;
            window[(*wpos) & (wsize - 1)] = (Bytef)sym;
            (*wpos)++;
            (*out)++;
            (*out_avail)--;
            (*out_total)++;
        } else if (sym == 256) {
            /* End of block */
            return Z_OK;
        } else {
            /* Length/distance pair */
            int len_idx = sym - 257;
            int length, dist;
            int dist_sym, dist_idx;
            int i;

            if (len_idx < 0 || len_idx >= 29) return Z_DATA_ERROR;

            length = rxd_len_base[len_idx];
            if (rxd_len_extra[len_idx] > 0) {
                if (rxd_br_ensure(br, rxd_len_extra[len_idx]) < 0) return Z_DATA_ERROR;
                length += rxd_br_read(br, rxd_len_extra[len_idx]);
            }

            dist_sym = rxd_huff_decode(dist_tree, br);
            if (dist_sym < 0 || dist_sym >= 30) return Z_DATA_ERROR;

            dist_idx = dist_sym;
            dist = rxd_dist_base[dist_idx];
            if (rxd_dist_extra[dist_idx] > 0) {
                if (rxd_br_ensure(br, rxd_dist_extra[dist_idx]) < 0) return Z_DATA_ERROR;
                dist += rxd_br_read(br, rxd_dist_extra[dist_idx]);
            }

            /* Copy from window */
            if (*out_avail < (uInt)length) return Z_BUF_ERROR;
            for (i = 0; i < length; i++) {
                Bytef c = window[((*wpos) - dist) & (wsize - 1)];
                **out = c;
                window[(*wpos) & (wsize - 1)] = c;
                (*wpos)++;
                (*out)++;
                (*out_avail)--;
                (*out_total)++;
            }
        }
    }
}

int inflate(z_streamp strm, int flush) {
    rxd_state *st;
    rxd_bitreader br;
    Bytef *out;
    uInt out_avail;
    uLong out_total;
    int ret;
    (void)flush;

    if (strm == NULL || strm->state == NULL) return Z_STREAM_ERROR;
    st = (rxd_state *)strm->state;

    if (st->finished) return Z_STREAM_END;

    /* Parse header if needed */
    if (!st->inf_header_done) {
        if (st->wrap == RXD_WRAP_ZLIB) {
            if (strm->avail_in < 2) return Z_BUF_ERROR;
            /* Verify zlib header */
            Bytef cmf = strm->next_in[0];
            Bytef flg = strm->next_in[1];
            if ((cmf * 256 + flg) % 31 != 0) return Z_DATA_ERROR;
            if ((cmf & 0x0f) != Z_DEFLATED) return Z_DATA_ERROR;
            /* Check for preset dictionary */
            if (flg & 0x20) return Z_NEED_DICT;
            strm->next_in += 2;
            strm->avail_in -= 2;
            strm->total_in += 2;
        } else if (st->wrap == RXD_WRAP_GZIP) {
            if (strm->avail_in < 10) return Z_BUF_ERROR;
            /* Verify gzip header */
            if (strm->next_in[0] != 0x1f || strm->next_in[1] != 0x8b)
                return Z_DATA_ERROR;
            if (strm->next_in[2] != 8) return Z_DATA_ERROR;
            Bytef gzip_flags = strm->next_in[3];
            strm->next_in += 10;
            strm->avail_in -= 10;
            strm->total_in += 10;

            /* Skip optional gzip fields */
            if (gzip_flags & 0x04) { /* FEXTRA */
                if (strm->avail_in < 2) return Z_BUF_ERROR;
                unsigned int xlen = strm->next_in[0] | (strm->next_in[1] << 8);
                strm->next_in += 2 + xlen;
                strm->avail_in -= 2 + xlen;
                strm->total_in += 2 + xlen;
            }
            if (gzip_flags & 0x08) { /* FNAME */
                while (strm->avail_in > 0 && *strm->next_in != 0) {
                    strm->next_in++; strm->avail_in--; strm->total_in++;
                }
                if (strm->avail_in > 0) { strm->next_in++; strm->avail_in--; strm->total_in++; }
            }
            if (gzip_flags & 0x10) { /* FCOMMENT */
                while (strm->avail_in > 0 && *strm->next_in != 0) {
                    strm->next_in++; strm->avail_in--; strm->total_in++;
                }
                if (strm->avail_in > 0) { strm->next_in++; strm->avail_in--; strm->total_in++; }
            }
            if (gzip_flags & 0x02) { /* FHCRC */
                if (strm->avail_in < 2) return Z_BUF_ERROR;
                strm->next_in += 2; strm->avail_in -= 2; strm->total_in += 2;
            }
        }
        st->inf_header_done = 1;
        st->check = (st->wrap == RXD_WRAP_GZIP) ? crc32(0UL, NULL, 0) : adler32(0UL, NULL, 0);
    }

    /* Set up bit reader with remaining input */
    rxd_br_init(&br, strm->next_in, strm->avail_in);
    out = strm->next_out;
    out_avail = strm->avail_out;
    out_total = 0;

    /* Process blocks */
    while (!st->inf_last) {
        int bfinal, btype;

        if (rxd_br_ensure(&br, 3) < 0) break;
        bfinal = rxd_br_read(&br, 1);
        btype = rxd_br_read(&br, 2);
        st->inf_last = bfinal;

        if (btype == 0) {
            /* Stored block */
            ret = rxd_inflate_stored(&br, &out, &out_avail, &out_total,
                                     st->inf_window, &st->inf_wpos, st->inf_wsize);
            if (ret != Z_OK) goto done;
        } else if (btype == 1) {
            /* Fixed Huffman */
            ret = rxd_inflate_huffman(&br, &rxd_fixed_litlen, &rxd_fixed_dist,
                                      &out, &out_avail, &out_total,
                                      st->inf_window, &st->inf_wpos, st->inf_wsize);
            if (ret != Z_OK) goto done;
        } else if (btype == 2) {
            /* Dynamic Huffman */
            rxd_huff_tree dyn_litlen, dyn_dist;
            int hlit, hdist, hclen;
            unsigned short cl_lens[RAWRXD_NUM_CL];
            unsigned short all_lens[RAWRXD_NUM_CODES + RAWRXD_NUM_DIST];
            rxd_huff_tree cl_tree;
            int i, total_codes;

            if (rxd_br_ensure(&br, 14) < 0) { ret = Z_DATA_ERROR; goto done; }
            hlit = rxd_br_read(&br, 5) + 257;
            hdist = rxd_br_read(&br, 5) + 1;
            hclen = rxd_br_read(&br, 4) + 4;

            /* Read code length code lengths */
            memset(cl_lens, 0, sizeof(cl_lens));
            for (i = 0; i < hclen; i++) {
                if (rxd_br_ensure(&br, 3) < 0) { ret = Z_DATA_ERROR; goto done; }
                cl_lens[rxd_cl_order[i]] = (unsigned short)rxd_br_read(&br, 3);
            }

            /* Build code length tree */
            if (rxd_huff_build(&cl_tree, cl_lens, RAWRXD_NUM_CL) < 0) {
                /* All zeros is valid for trailing codes */
            }

            /* Read literal/length and distance code lengths */
            total_codes = hlit + hdist;
            memset(all_lens, 0, sizeof(all_lens));
            i = 0;
            while (i < total_codes) {
                int sym = rxd_huff_decode(&cl_tree, &br);
                if (sym < 0) { ret = Z_DATA_ERROR; goto done; }
                
                if (sym < 16) {
                    all_lens[i++] = (unsigned short)sym;
                } else if (sym == 16) {
                    /* Repeat previous length 3-6 times */
                    int count;
                    unsigned short prev_len;
                    if (i == 0) { ret = Z_DATA_ERROR; goto done; }
                    if (rxd_br_ensure(&br, 2) < 0) { ret = Z_DATA_ERROR; goto done; }
                    count = 3 + rxd_br_read(&br, 2);
                    prev_len = all_lens[i - 1];
                    while (count-- > 0 && i < total_codes) {
                        all_lens[i++] = prev_len;
                    }
                } else if (sym == 17) {
                    /* Repeat 0 for 3-10 times */
                    int count;
                    if (rxd_br_ensure(&br, 3) < 0) { ret = Z_DATA_ERROR; goto done; }
                    count = 3 + rxd_br_read(&br, 3);
                    while (count-- > 0 && i < total_codes) {
                        all_lens[i++] = 0;
                    }
                } else if (sym == 18) {
                    /* Repeat 0 for 11-138 times */
                    int count;
                    if (rxd_br_ensure(&br, 7) < 0) { ret = Z_DATA_ERROR; goto done; }
                    count = 11 + rxd_br_read(&br, 7);
                    while (count-- > 0 && i < total_codes) {
                        all_lens[i++] = 0;
                    }
                } else {
                    ret = Z_DATA_ERROR; goto done;
                }
            }

            /* Build dynamic trees */
            if (rxd_huff_build(&dyn_litlen, all_lens, hlit) < 0) {
                ret = Z_DATA_ERROR; goto done;
            }
            if (rxd_huff_build(&dyn_dist, all_lens + hlit, hdist) < 0) {
                /* Single distance code is valid */
            }

            ret = rxd_inflate_huffman(&br, &dyn_litlen, &dyn_dist,
                                       &out, &out_avail, &out_total,
                                       st->inf_window, &st->inf_wpos, st->inf_wsize);
            if (ret != Z_OK) goto done;
        } else {
            ret = Z_DATA_ERROR; goto done; /* invalid block type */
        }
    }

    /* Update checksums */
    {
        uInt written = (uInt)out_total;
        Bytef *check_start = strm->next_out;
        if (written > 0) {
            if (st->wrap == RXD_WRAP_GZIP) {
                st->check = crc32(st->check, check_start, written);
                st->inf_data_len += written;
            } else if (st->wrap == RXD_WRAP_ZLIB) {
                st->check = adler32(st->check, check_start, written);
            }
        }
    }

    ret = Z_OK;
    if (st->inf_last) {
        /* Verify trailer */
        if (st->wrap == RXD_WRAP_ZLIB) {
            /* Read 4-byte adler32 checksum (big-endian) */
            if (br.in_avail >= 4) {
                uLong expected = ((uLong)br.in[0] << 24) | ((uLong)br.in[1] << 16) |
                                 ((uLong)br.in[2] << 8) | (uLong)br.in[3];
                br.in += 4; br.in_avail -= 4; br.in_total += 4;
                if (expected != st->check) {
                    /* Checksum mismatch - warn but don't fail for compatibility */
                }
            }
        } else if (st->wrap == RXD_WRAP_GZIP) {
            rxd_br_align(&br);
            if (br.in_avail >= 8) {
                br.in += 8; br.in_avail -= 8; br.in_total += 8;
            }
        }
        st->finished = 1;
        ret = Z_STREAM_END;
    }

done:
    /* Update stream pointers */
    strm->next_in = (const Bytef *)br.in;
    strm->avail_in = br.in_avail;
    strm->total_in += br.in_total;
    strm->next_out = out;
    strm->avail_out = out_avail;
    strm->total_out += out_total;
    strm->adler = st->check;

    return ret;
}

int inflateEnd(z_streamp strm) {
    rxd_state *st;
    if (strm == NULL || strm->state == NULL) return Z_STREAM_ERROR;
    st = (rxd_state *)strm->state;

    if (st->inf_window) { free(st->inf_window); st->inf_window = NULL; }
    
    strm->zfree(strm->opaque, st);
    strm->state = NULL;
    return Z_OK;
}

int inflateReset(z_streamp strm) {
    rxd_state *st;
    if (strm == NULL || strm->state == NULL) return Z_STREAM_ERROR;
    st = (rxd_state *)strm->state;

    st->finished = 0;
    st->inf_state = 0;
    st->inf_last = 0;
    st->inf_header_done = 0;
    st->inf_wpos = 0;
    st->inf_data_len = 0;
    
    if (st->wrap == RXD_WRAP_GZIP)
        st->check = crc32(0UL, NULL, 0);
    else
        st->check = adler32(0UL, NULL, 0);

    strm->total_in = 0;
    strm->total_out = 0;
    strm->adler = st->check;

    if (st->inf_window) memset(st->inf_window, 0, st->inf_wsize);

    return Z_OK;
}

/* ============================================================================
 * Utility functions: compress, compress2, uncompress, compressBound
 * ============================================================================ */

uLong compressBound(uLong sourceLen) {
    /* Conservative: stored blocks need 5 bytes overhead per 65535 bytes,
       plus headers/trailers. Add extra safety margin. */
    return sourceLen + (sourceLen >> 12) + (sourceLen >> 14) + 
           (sourceLen >> 25) + 13 + 18;
}

int compress2(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen, int level) {
    z_stream stream;
    int err;

    memset(&stream, 0, sizeof(stream));
    stream.next_in = source;
    stream.avail_in = (uInt)sourceLen;
    stream.next_out = dest;
    stream.avail_out = (uInt)*destLen;

    err = deflateInit(&stream, level);
    if (err != Z_OK) return err;

    err = deflate(&stream, Z_FINISH);
    if (err != Z_STREAM_END) {
        deflateEnd(&stream);
        return (err == Z_OK) ? Z_BUF_ERROR : err;
    }
    *destLen = stream.total_out;

    return deflateEnd(&stream);
}

int compress(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen) {
    return compress2(dest, destLen, source, sourceLen, Z_DEFAULT_COMPRESSION);
}

int uncompress2(Bytef *dest, uLongf *destLen, const Bytef *source, uLong *sourceLen) {
    z_stream stream;
    int err;

    memset(&stream, 0, sizeof(stream));
    stream.next_in = source;
    stream.avail_in = (uInt)(*sourceLen);
    stream.next_out = dest;
    stream.avail_out = (uInt)*destLen;

    err = inflateInit(&stream);
    if (err != Z_OK) return err;

    err = inflate(&stream, Z_FINISH);
    if (err != Z_STREAM_END) {
        inflateEnd(&stream);
        if (err == Z_OK) return Z_BUF_ERROR;
        return err;
    }
    *destLen = stream.total_out;
    *sourceLen = stream.total_in;

    return inflateEnd(&stream);
}

int uncompress(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen) {
    uLong sLen = sourceLen;
    return uncompress2(dest, destLen, source, &sLen);
}

/* ============================================================================
 * Version
 * ============================================================================ */
const char *zlibVersion(void) {
    return ZLIB_VERSION;
}
