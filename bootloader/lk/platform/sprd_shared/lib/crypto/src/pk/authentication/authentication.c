#include <config_gcm_sw.h>
#include <gcm.h>
#include <authentication.h>
#include <crypto/driver/sprd_rng.h>
#include <sprd_rsa.h>
#include <sprd_crypto_sw.h>
#include <sprdsha.h>
#include <chipram_env.h>
#include <sprd_common.h>

#define ENABLE_SELFTEST 0

#if ENABLE_SELFTEST
extern void crypto_hexdump(const char *title, uint8_t * data, int len);
#endif

struct {
    unsigned int data_len;
    unsigned char e[4];
    unsigned char n[RSA_2048_BUF_LEN];
    int n_bitlen;
    unsigned char R1[RSA_2048_BUF_LEN];
    unsigned char version[VERSION_LEN];
} rsa_authentic_pubkey = {
    RSA_2048_BUF_LEN,
    /* e */
    { 0x00, 0x01, 0x00, 0x01, },
    /* n */
    { 0xba, 0x76, 0xac, 0xae, 0x3e, 0xed, 0xd2, 0x09, 0xfb, 0x9a, 0x64, 0x7a, 0x3f, 0x9b, 0x62, 0x1b,
      0x68, 0xdf, 0x6d, 0x8a, 0x32, 0x19, 0x03, 0xd6, 0x34, 0xef, 0x67, 0x44, 0x7d, 0xd1, 0xf0, 0x85,
      0xd8, 0x9f, 0x17, 0x94, 0x7d, 0xfa, 0x19, 0xc5, 0x62, 0xa2, 0x9e, 0xe2, 0xeb, 0x93, 0x35, 0x32,
      0xfc, 0x2a, 0x7a, 0x7e, 0xd6, 0xbf, 0xe3, 0x22, 0xc8, 0x3a, 0x8a, 0x3f, 0xb4, 0xdf, 0x6e, 0x1c,
      0x9b, 0xc7, 0x72, 0xfb, 0x96, 0xb4, 0x93, 0x2b, 0xd4, 0x55, 0x2f, 0xe9, 0x18, 0x24, 0x45, 0x2f,
      0x3f, 0x0d, 0xa9, 0x4f, 0xd3, 0x8d, 0x0d, 0x62, 0x1d, 0xa7, 0x90, 0xfc, 0x45, 0xe9, 0xcb, 0x7e,
      0xb8, 0x93, 0xac, 0xf3, 0x56, 0xff, 0x8c, 0xf9, 0xe6, 0x0e, 0xe6, 0x2a, 0x5e, 0xc6, 0x7b, 0x50,
      0xa6, 0x61, 0x85, 0x66, 0x6d, 0x29, 0x77, 0x45, 0xd4, 0x44, 0x53, 0xeb, 0x77, 0x6c, 0x5d, 0x86,
      0x46, 0xbb, 0x27, 0xbf, 0xbc, 0xa2, 0x7a, 0xd6, 0x02, 0x4a, 0xe5, 0x31, 0x7c, 0x6a, 0x50, 0x02,
      0x2f, 0x1f, 0xaa, 0xfe, 0x2f, 0x0a, 0x57, 0x29, 0xb6, 0xc3, 0x5b, 0x1b, 0xda, 0x7d, 0x6b, 0x5f,
      0xd6, 0xb5, 0x6e, 0xb5, 0xff, 0x94, 0x61, 0x2d, 0x1f, 0x47, 0xde, 0xd7, 0xb5, 0xdc, 0x7b, 0xc4,
      0x1d, 0x5f, 0x80, 0x06, 0xf0, 0x41, 0xd6, 0x38, 0xca, 0x1b, 0x49, 0x3f, 0xab, 0xc3, 0xd3, 0x7a,
      0x47, 0x8c, 0x18, 0xcd, 0x39, 0xea, 0xae, 0xe5, 0xb2, 0x76, 0x0b, 0x93, 0xa5, 0x54, 0x8c, 0x19,
      0x9d, 0xfc, 0x41, 0x6d, 0x08, 0xc9, 0x42, 0x06, 0x9a, 0xec, 0xd4, 0x65, 0xe3, 0x58, 0x4c, 0x4b,
      0xd8, 0xd2, 0x4a, 0x07, 0x80, 0xbc, 0x15, 0x1f, 0x1e, 0x29, 0x1c, 0xa4, 0x08, 0x2f, 0x81, 0x3f,
      0x5f, 0x49, 0xa3, 0xfc, 0x9c, 0x52, 0xf3, 0x78, 0x1d, 0x2c, 0x08, 0xc9, 0xd8, 0xed, 0x64, 0x33,
    },
    /* n_bitlen */
    RSA_2048_BUF_LEN * 8,
    /* R1 */
    { 0 },
    /* verision */
    { 0 },
};

/*
 * flg = 1 : respond to authentication requests
 * Msg : [out] M1, the encrypted R1(version||rand||socid) with rsa_pubkey
 * flg = 0 for return authentication result
 * Msg : [in] M2, the data to verify with rsa_pubkey
 */

int authentication (unsigned char *msg, int flg)
{
    int res = BAD_INPUT;
    int temp_len = 0;
    unsigned char rng[DEFINE_RNG_LEN] = {0};
    unsigned char uid[HASH_SHA256_BUF_LEN] = {0};
    unsigned char hash_uid[HASH_SHA256_BUF_LEN + 1] = {0};
    unsigned char buf[RSA_2048_BUF_LEN] = {0};
    boot_mode_t boot_role = get_boot_role();
    int err = 0;

    if (msg == NULL) {
        errorf("authentication: bad input!\n");
        return BAD_INPUT;
    }

    switch (flg) {
    case VERIFICATION:
        res = RSA_Verify(rsa_authentic_pubkey.e, rsa_authentic_pubkey.n, rsa_authentic_pubkey.n_bitlen, msg, buf);

        if (res != HASH_SHA256_BUF_LEN || memcmp(rsa_authentic_pubkey.R1, buf, RSA_2048_VERIFY_BUF_LEN)){
            crypto_hexdump("R1:", rsa_authentic_pubkey.R1, RSA_2048_VERIFY_BUF_LEN);
            crypto_hexdump("buf:", buf, RSA_2048_VERIFY_BUF_LEN);
            return AUTH_FAILED;
        } else {
            memset(msg, 0, RSA_2048_BUF_LEN);
            return AUTH_SUCCESS;
        }
        break;

    case ENCRYPTION:
        /* 1.version */
        memcpy(rsa_authentic_pubkey.R1, rsa_authentic_pubkey.version, VERSION_LEN);
        temp_len += VERSION_LEN;

        /* 2.random R0 */
        switch (boot_role) {
        case BOOTLOADER_MODE_LOAD:
            err = prand_gen(rng, DEFINE_RNG_LEN);
            break;
        case BOOTLOADER_MODE_DOWNLOAD:
            err = sprd_rng_gen(rng, DEFINE_RNG_LEN);
            break;
        default:
            SPRD_CRYPTO_LOG_ERR("unknown uboot role\n");
            return GET_RNG_FAILED;
        }
        if (err != SPRD_CRYPTO_SUCCESS) {
            errorf("GET_RNG_FAILED\n");
            return GET_RNG_FAILED;
        }
        crypto_hexdump("rng:", rng, DEFINE_RNG_LEN);
        memcpy(rsa_authentic_pubkey.R1+temp_len, rng, DEFINE_RNG_LEN);
        memset(rng, 0, DEFINE_RNG_LEN);
        temp_len += DEFINE_RNG_LEN;

        /* 3.hashed uid instead socid */
        if (sprd_get_chip_hex_uid (uid)) {
            return GET_UID_FAILED;
        }
        sha256_csum_wd_sw(uid, HASH_SHA256_BUF_LEN, hash_uid, NULL);
        memset(uid, 0, HASH_SHA256_BUF_LEN);
        memcpy(rsa_authentic_pubkey.R1+temp_len, hash_uid, HASH_SHA256_BUF_LEN);
        memset(hash_uid, 0, HASH_SHA256_BUF_LEN+1);
        temp_len += HASH_SHA256_BUF_LEN;
        crypto_hexdump("R1:", rsa_authentic_pubkey.R1, temp_len);

        /* 4. rsa encrypt with pkcs1.5 padding */
        res = RSA_PubEnc(rsa_authentic_pubkey.e, rsa_authentic_pubkey.n,
                rsa_authentic_pubkey.n_bitlen, rsa_authentic_pubkey.R1, temp_len, msg);

        if (0 != res) {
            return ENCRYPT_FAILED;
        } else {
            /* save the hashed R1 */
            sha256_csum_wd_sw(rsa_authentic_pubkey.R1, temp_len, buf, NULL);
            memset(rsa_authentic_pubkey.R1, 0, temp_len);
            memcpy(rsa_authentic_pubkey.R1, buf, HASH_SHA256_BUF_LEN);
            return ENCRYPT_SUCCESSED;
        }
        break;

    default:
        dprintf(INFO,"unsupported flg!\n");
    }

    return res;
}
/* do not use this */
#if ENABLE_SELFTEST
void authentication_test(void)
{
    unsigned char msg[ENCRYPT_SUCCESSED] = {0};
    int ret = -1;
    ret = authentication (msg, 1);
    dprintf(INFO,"ret = %d\n", ret);
    ret = authentication (msg, 1);
    dprintf(INFO,"ret = %d\n", ret);
    ret = authentication (msg, 0);
    dprintf(INFO,"ret = %d\n", ret);
    ret = authentication (msg, 2);
    dprintf(INFO,"ret = %d\n", ret);
    return 0;
 }
#endif

