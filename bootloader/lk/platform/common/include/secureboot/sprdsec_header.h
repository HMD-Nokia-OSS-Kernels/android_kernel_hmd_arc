/*
# Copyright 2022 Unisoc (Shanghai) Technologies Co., Ltd.
# Licensed under the Unisoc General Software License, version 1.0 (the License);
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
# Software distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OF ANY KIND, either express or implied.
# See the Unisoc General Software License, version 1.0 for more details.
*/

#ifndef __SPRDSEC_HEADER_H
#define __SPRDSEC_HEADER_H

typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

#define MAGIC_SIZE 8
#define VERSION_EBL 0
#define VERSION_NBL 1
#define CERTTYPE_CONTENT 0
#define CERTTYPE_KEY 1
#define CERTTYPE_PRIMDBG 2
#define CERTTYPE_DEVELOPDBG 3

#define MAX_HASH_BITS_LEN (256)
#define MAX_HASH_BYTES_LEN (MAX_HASH_BITS_LEN>>3)

#define MAX_RSA2048_KEY_BITS_LEN (2048)
#define MAX_RSA3072_KEY_BITS_LEN (3072)
#define MAX_RSA4096_KEY_BITS_LEN (4096)

#define RSA_KEY_BYTE_LEN_MAX (MAX_RSA2048_KEY_BITS_LEN>>3)
#define MAX_RSA_KEY_BYTES_LEN (MAX_RSA4096_KEY_BITS_LEN>>3)

#define HASH_BYTE_LEN	 32
#define KEYCERT_V2_HASH_LEN 72
#define KEYCERT_V3_HASH_LEN 76 //72+4(verison)
#define CNTCERT_V2_HASH_LEN 40
#define CNTCERT_V3_HASH_LEN 44 //40+4(verison)
#define PRIMDBG_HASH_LEN 40
#define DEVEDBG_HASH_LEN 12

#define SPRD_KEYCERT_V2 0
#define SPRD_KEYCERT_V3 1

#define AES_HEADER_MAGIC	0x68656361
#define SPRD_HEADER_MAGIC	0x42544844
#define	AES_KEY_BYTE_LEN	16
#define	AES_IV_BYTE_LEN		16

#define UNENCRYPTED_FLAG	0
#define	ENCRYPT_FLAG_LEN	16
#define	AES_HEADER_SIZE		64
#define	TOSIMG_NAME_SIZE	7
#define	KCE_DECRYPT_FLAG	1

typedef enum {
  AES_GET_KEY_OK,
  AES_GET_KEY_UNENCRYPTED,
  AES_GET_KEY_ERROR
} AesGetKeyResult;

typedef struct sprd_aes_header {
	uint32_t magic_num; 	/*0x68656361*/
	uint32_t image_size;
	uint8_t reserved1[8];
	uint64_t encrypt_flag;	/*encrypt flag: 1:encrypt 0：no encrypt*/
	uint8_t reserved2[8];
	uint8_t rand_key[AES_KEY_BYTE_LEN];
	uint8_t iv[AES_IV_BYTE_LEN];
} sprd_aes_header;

typedef struct sprdsignedimageheader {

	/* Magic number */
	uint8_t magic[MAGIC_SIZE];
	/* Version of this header format */
	uint32_t header_version_major;
	/* Version of this header format */
	uint32_t header_version_minor;

	/*image body, plain or cipher text */
	uint64_t payload_size;
	uint64_t payload_offset;

	/*offset from itself start */
	/*content certification size,if 0,ignore */
	uint64_t cert_size;
	uint64_t cert_offset;

	/*(opt)private content size,if 0,ignore */
	uint64_t priv_size;
	uint64_t priv_offset;

	/*(opt)debug/rma certification primary size,if 0,ignore */
	uint64_t cert_dbg_prim_size;
	uint64_t cert_dbg_prim_offset;

	/*(opt)debug/rma certification second size,if 0,ignore */
	uint64_t cert_dbg_developer_size;
	uint64_t cert_dbg_developer_offset;

} sprdsignedimageheader;

#define MAX_ECC_KEY_BITS_LEN (384)
#define MAX_ECC_KEY_BYTES_LEN (MAX_ECC_KEY_BITS_LEN>>3)
#define MAX_ECC_KEY_RESERVED_BYTES_LEN (((MAX_RSA4096_KEY_BITS_LEN - (MAX_ECC_KEY_BITS_LEN<<1))>>3)+8)//424

//rsa2048
typedef struct sprd_rsa2048_pubkey {
	uint32_t keybit_len;	//1024/2048,max 2048
	uint32_t e;
	uint8_t mod[RSA_KEY_BYTE_LEN_MAX];
} sprd_rsa2048_pubkey;

//rsa4096 or rsa3072
typedef struct sprd_rsa4096_pubkey{
    uint32_t keybit_len;
    uint32_t e;
    uint8_t mod[MAX_RSA_KEY_BYTES_LEN];
} sprd_rsa4096_pubkey;

//ecc384
typedef struct  {
    uint8_t key_x[MAX_ECC_KEY_BYTES_LEN];
    uint8_t key_y[MAX_ECC_KEY_BYTES_LEN];
    uint8_t key_reserved[MAX_ECC_KEY_RESERVED_BYTES_LEN];
} ecc_pubkey_t;

#define  SPRD_PUBKLEN_V2  sizeof(sprd_rsa2048_pubkey)
#define  SPRD_PUBKLEN_V3  sizeof(sprd_rsa4096_pubkey) //ecc rsa3072 rsa4096 pubkeylen
#define MAX_ECC_SIGNATURE_RESERVED_BYTES_LEN (MAX_RSA_KEY_BYTES_LEN-2*MAX_ECC_KEY_BYTES_LEN)
typedef struct  {
    uint8_t ecc_r[MAX_ECC_KEY_BYTES_LEN];
    uint8_t ecc_s[MAX_ECC_KEY_BYTES_LEN];
    uint8_t signature_reserved[MAX_ECC_SIGNATURE_RESERVED_BYTES_LEN];
} ecc_signature_t;

//rsa2048 keycert
typedef struct sprd_keycert_v2 {
	uint32_t certtype;	//1:key cert
	sprd_rsa2048_pubkey pubkey;	//pubkey for this cert, to verify signature in this cert
	uint8_t hash_data[HASH_BYTE_LEN];	//hash of current image data
	uint8_t hash_key[HASH_BYTE_LEN];	// hash of pubkey in next cert
	uint32_t algorithm;
	uint32_t version;
	uint8_t signature[RSA_KEY_BYTE_LEN_MAX];	//signature of hash_data+hash_key
} sprd_keycert_v2;

//rsa3072 or 4096 keycert
typedef struct {
    uint32_t certtype;	//1:key cert
    //pubkey for this cert, to verify signature in this cert
    union {
        sprd_rsa4096_pubkey rsa_pubkey;
        ecc_pubkey_t ecc_pubkey;
    }_pubkey;

    uint8_t hash_data[MAX_HASH_BYTES_LEN];	//hash of current image payload
    uint8_t hash_key[MAX_HASH_BYTES_LEN];	// hash of pubkey to verify next stage image
    uint32_t algorithm;
    uint64_t version;
    //signature of [payload_hash ~ next_pubkey_hash ~ algorithm ~ version]
    union {
        uint8_t rsa_signature[MAX_RSA_KEY_BYTES_LEN];
        ecc_signature_t ecc_signature;
    }_signature;
} sprd_keycert_v3;


//rsa2048 contentcert
typedef struct sprd_contentcert_v2 {
	uint32_t certtype;	//0:content cert
	sprd_rsa2048_pubkey pubkey;	//pubkey for this cert, to verify signature in this cert
	uint8_t hash_data[HASH_BYTE_LEN];	//hash of image component
	uint32_t algorithm;
	uint32_t version;
	uint8_t signature[RSA_KEY_BYTE_LEN_MAX];	//signature of hash_data
} sprd_contentcert_v2;

//rsa3072 or 4096 contentcert
typedef struct {
    uint32_t certtype;	//0:content cert
    //pubkey for this cert, to verify signature in this cert
    union {
        sprd_rsa4096_pubkey rsa_pubkey;
        ecc_pubkey_t ecc_pubkey;
    }_pubkey;

    uint8_t hash_data[MAX_HASH_BYTES_LEN];	//hash of image payload
    uint32_t algorithm;
    uint64_t version;

    //signature of [payload_hash ~ algorithm ~ version]
    union {
        uint8_t rsa_signature[MAX_RSA_KEY_BYTES_LEN];
        ecc_signature_t ecc_signature;
    }_signature;
} sprd_contentcert_v3;

#endif
