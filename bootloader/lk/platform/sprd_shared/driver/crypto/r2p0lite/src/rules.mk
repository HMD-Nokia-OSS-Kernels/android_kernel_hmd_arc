
#
# The MIT License (MIT)
# Copyright (c) 2008-2015 Travis Geiselbrecht
# Copyright (c) 2016, Spreadtrum Communications.
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files
# (the "Software"), to deal in the Software without restriction,
# including without limitation the rights to use, copy, modify, merge,
# publish, distribute, sublicense, and/or sell copies of the Software,
# and to permit persons to whom the Software is furnished to do so,
# subject to the following conditions:
#
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
# CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
# TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
# SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#

LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE_SRCS += \
	$(LOCAL_DIR)/sprd_ce.c \
	$(LOCAL_DIR)/sprd_crypto.c \
	$(LOCAL_DIR)/sprd_pka.c \

ifeq ($(strip $(WITH_CE_TEST)),true)
MODULE_SRCS += \
	$(LOCAL_DIR)/sprd_test.c
endif

include $(LOCAL_DIR)/hashes/rules.mk
include $(LOCAL_DIR)/pk/rules.mk
include $(LOCAL_DIR)/rngs/rules.mk
