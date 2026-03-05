#ifndef _AUTHENTICATION_H_
#define _AUTHENTICATION_H_

#define GET_RNG_FAILED          -1
#define GET_UID_FAILED          -2
#define ENCRYPT_FAILED          -3
#define DECRYPT_FAILED          -4
#define AUTH_FAILED             -5
#define AUTH_SUCCESS            1
#define BAD_INPUT               -6

/* arguments for function authentication() */
#define ENCRYPTION              1
#define VERIFICATION            0

/* the size of result after encrypting with RSA-2048 */
#define ENCRYPT_SUCCESSED       256
#define RSA_2048_BUF_LEN        256

/* the size of version */
#define VERSION_LEN             4

/* the max size of RNG*/
#define CRYPTO_RNG_MAX_SIZE     512

/* the user-defined size of RNG */
#define DEFINE_RNG_LEN          32

/* the size of sha256-hashed data*/
#define HASH_SHA256_BUF_LEN     32

/* the size of RSA-2048 verify rs */
#define RSA_2048_VERIFY_BUF_LEN 64

/* get rand */
extern int prand_gen(u8 *rand, size_t rand_len);

/* get socid */
extern int sprd_get_chip_hex_uid (char *buf);

/* SHA256 */
extern void sha256_csum_wd_sw (const unsigned char *input,unsigned int ilen,unsigned char *output,unsigned int chunk_sz);

/*
 * flg = 1 : respond to authentication requests
 * [OUT]Msg : M1, the encrypted M1(version||rand||socid) with rsa_pubkey
 *Return value:
 * sizeof(M1): respond successed
 * <= 0 : respond failed
 *
 * flg = 0 for return authentication result
 * [IN]Msg : M2, the data to verify with rsa_pubkey
 *Return value:
 * 0 : verify successed
 * <0 : verify failed
 */
extern int authentication (unsigned char *msg, int flg);

#endif
