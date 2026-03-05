#ifndef AES_LOCL_H
#define AES_LOCL_H

#include "aes.h"
#define GETU32(p) (((u32)(p)[3]) ^ ((u32)(p)[2] << 8) ^ ((u32)(p)[1] << 16) ^ ((u32)(p)[0] << 24))

#define PUTU32(c, s) { (c)[3] = (u8)(s); (c)[2] = (u8)((s)>>8); (c)[1] = (u8)((s)>>16); (c)[0] = (u8)((s)>>24); }

//typedef unsigned long u32;
//typedef unsigned short u16;
//typedef unsigned char u8;

#define MAXKC   (256/32)
#define MAXKB   (256/8)
#define MAXNR   14

#define STRICT_ALIGNMENT 1

static inline size_t load_word_le(const void *in) {
   size_t v;
   sprd_pal_memcpy(&v, in, sizeof(v));
   return v;
}

static inline void store_word_le(void *out, size_t v) {
   sprd_pal_memcpy(out, &v, sizeof(v));
}

typedef void (*block128_f)(const uint8_t in[16], uint8_t out[16], const void *key);

typedef void (*ctr128_f)(const uint8_t *in, uint8_t *out, size_t blocks,
                                 const void *key, const uint8_t ivec[16]);

void CRYPTO_cbc128_encrypt(const uint8_t *in, uint8_t *out, size_t len, const void *key, uint8_t ivec[16], block128_f block);
void CRYPTO_ctr128_encrypt(const uint8_t *in, uint8_t *out, size_t len,
                           const AES_KEY *key, uint8_t ivec[16],
                           uint8_t ecount_buf[16], unsigned int *num,
                           block128_f block);
void CRYPTO_cbc128_decrypt(const uint8_t *in, uint8_t *out, size_t len,
                           const void *key, uint8_t ivec[16],
                           block128_f block);
#undef FULL_UNROLL

#endif
