/*
* Copyright 2022 Unisoc (Shanghai) Technologies Co., Ltd.
* Licensed under the Unisoc General Software License, version 1.0 (the License);
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
* https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
* Software distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OF ANY KIND, either express or implied.
* See the Unisoc General Software License, version 1.0 for more details.
*/

#ifndef KEYMINT_DEFS_H_
#define KEYMINT_DEFS_H_

#include <trusty/sysdeps.h>

typedef enum {
    KM_VERIFIED_BOOT_VERIFIED = 0,    /* Full chain of trust extending from the bootloader to
                                       * verified partitions, including the bootloader, boot
                                       * partition, and all verified partitions*/
    KM_VERIFIED_BOOT_SELF_SIGNED = 1, /* The boot partition has been verified using the embedded
                                       * certificate, and the signature is valid. The bootloader
                                       * displays a warning and the fingerprint of the public
                                       * key before allowing the boot process to continue.*/
    KM_VERIFIED_BOOT_UNVERIFIED = 2,  /* The device may be freely modified. Device integrity is left
                                       * to the user to verify out-of-band. The bootloader
                                       * displays a warning to the user before allowing the boot
                                       * process to continue */
    KM_VERIFIED_BOOT_FAILED = 3,      /* The device failed verification. The bootloader displays a
                                       * warning and stops the boot process, so no keymaster
                                       * implementation should ever actually return this value,
                                       * since it should not run.  Included here only for
                                       * completeness. */
} keymaster_verified_boot_t;

typedef enum {
    KM_DEVICE_UNLOCKED = 0,
    KM_DEVICE_LOCKED = 1,
} keymaster_device_locked_t;

typedef enum {
    KM_ERROR_OK = 0,
    KM_ERROR_ROOT_OF_TRUST_ALREADY_SET = -1,
    KM_ERROR_UNSUPPORTED_PURPOSE = -2,
    KM_ERROR_INCOMPATIBLE_PURPOSE = -3,
    KM_ERROR_UNSUPPORTED_ALGORITHM = -4,
    KM_ERROR_INCOMPATIBLE_ALGORITHM = -5,
    KM_ERROR_UNSUPPORTED_KEY_SIZE = -6,
    KM_ERROR_UNSUPPORTED_BLOCK_MODE = -7,
    KM_ERROR_INCOMPATIBLE_BLOCK_MODE = -8,
    KM_ERROR_UNSUPPORTED_MAC_LENGTH = -9,
    KM_ERROR_UNSUPPORTED_PADDING_MODE = -10,
    KM_ERROR_INCOMPATIBLE_PADDING_MODE = -11,
    KM_ERROR_UNSUPPORTED_DIGEST = -12,
    KM_ERROR_INCOMPATIBLE_DIGEST = -13,
    KM_ERROR_INVALID_EXPIRATION_TIME = -14,
    KM_ERROR_INVALID_USER_ID = -15,
    KM_ERROR_INVALID_AUTHORIZATION_TIMEOUT = -16,
    KM_ERROR_UNSUPPORTED_KEY_FORMAT = -17,
    KM_ERROR_INCOMPATIBLE_KEY_FORMAT = -18,
    /* For PKCS8 & PKCS12 */
    KM_ERROR_UNSUPPORTED_KEY_ENCRYPTION_ALGORITHM = -19,
    /* For PKCS8 & PKCS12 */
    KM_ERROR_UNSUPPORTED_KEY_VERIFICATION_ALGORITHM = -20,
    KM_ERROR_INVALID_INPUT_LENGTH = -21,
    KM_ERROR_KEY_EXPORT_OPTIONS_INVALID = -22,
    KM_ERROR_DELEGATION_NOT_ALLOWED = -23,
    KM_ERROR_KEY_NOT_YET_VALID = -24,
    KM_ERROR_KEY_EXPIRED = -25,
    KM_ERROR_KEY_USER_NOT_AUTHENTICATED = -26,
    KM_ERROR_OUTPUT_PARAMETER_NULL = -27,
    KM_ERROR_INVALID_OPERATION_HANDLE = -28,
    KM_ERROR_INSUFFICIENT_BUFFER_SPACE = -29,
    KM_ERROR_VERIFICATION_FAILED = -30,
    KM_ERROR_TOO_MANY_OPERATIONS = -31,
    KM_ERROR_UNEXPECTED_NULL_POINTER = -32,
    KM_ERROR_INVALID_KEY_BLOB = -33,
    KM_ERROR_IMPORTED_KEY_NOT_ENCRYPTED = -34,
    KM_ERROR_IMPORTED_KEY_DECRYPTION_FAILED = -35,
    KM_ERROR_IMPORTED_KEY_NOT_SIGNED = -36,
    KM_ERROR_IMPORTED_KEY_VERIFICATION_FAILED = -37,
    KM_ERROR_INVALID_ARGUMENT = -38,
    KM_ERROR_UNSUPPORTED_TAG = -39,
    KM_ERROR_INVALID_TAG = -40,
    KM_ERROR_MEMORY_ALLOCATION_FAILED = -41,
    KM_ERROR_IMPORT_PARAMETER_MISMATCH = -44,
    KM_ERROR_SECURE_HW_ACCESS_DENIED = -45,
    KM_ERROR_OPERATION_CANCELLED = -46,
    KM_ERROR_CONCURRENT_ACCESS_CONFLICT = -47,
    KM_ERROR_SECURE_HW_BUSY = -48,
    KM_ERROR_SECURE_HW_COMMUNICATION_FAILED = -49,
    KM_ERROR_UNSUPPORTED_EC_FIELD = -50,
    KM_ERROR_MISSING_NONCE = -51,
    KM_ERROR_INVALID_NONCE = -52,
    KM_ERROR_MISSING_MAC_LENGTH = -53,
    KM_ERROR_KEY_RATE_LIMIT_EXCEEDED = -54,
    KM_ERROR_CALLER_NONCE_PROHIBITED = -55,
    KM_ERROR_KEY_MAX_OPS_EXCEEDED = -56,
    KM_ERROR_INVALID_MAC_LENGTH = -57,
    KM_ERROR_MISSING_MIN_MAC_LENGTH = -58,
    KM_ERROR_UNSUPPORTED_MIN_MAC_LENGTH = -59,
    KM_ERROR_UNSUPPORTED_KDF = -60,
    KM_ERROR_UNSUPPORTED_EC_CURVE = -61,
    KM_ERROR_KEY_REQUIRES_UPGRADE = -62,
    KM_ERROR_ATTESTATION_CHALLENGE_MISSING = -63,
    KM_ERROR_KEYMASTER_NOT_CONFIGURED = -64,

    KM_ERROR_UNIMPLEMENTED = -100,
    KM_ERROR_VERSION_MISMATCH = -101,

    KM_ERROR_UNKNOWN_ERROR = -1000,
} keymaster_error_t;


#endif /* KEYMINT_DEFS_H_ */
