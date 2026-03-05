/*
*  Copyright 2016 (c) Spreadtrum Communications Inc.
*
*  This software is protected by copyright, international treaties and various patents.
*  Any copy, reproduction or otherwise use of this software must be authorized in a
*  license agreement and include this Copyright Notice and any other notices specified
*  in the license agreement. Any redistribution in binary form must be authorized in the
*  license agreement and include this Copyright Notice and any other notices specified
*  in the license agreement and/or in materials provided with the binary distribution.
*
*  created by vee.zhang <2016.10.15>
*/

#ifndef SPRD_CRYPTO_SPECIAL_H_
#define SPRD_CRYPTO_SPECIAL_H_

int AES_CBC_Dec(unsigned char *ct, unsigned int ct_len, unsigned char *pt, unsigned int *pt_len,
        unsigned char *key, unsigned int key_len, unsigned char *iv, unsigned int iv_len);

int RSA_PubDec(unsigned char *pub_E, unsigned char *mod_N, int bitLen_N, unsigned char *from, unsigned char *to) ;

void sha256_csum_wd(const unsigned char *input,unsigned int ilen,unsigned char *output,unsigned int chunk_sz) ;

int sprd_socid_get(unsigned char *uid_ptr);

#endif
