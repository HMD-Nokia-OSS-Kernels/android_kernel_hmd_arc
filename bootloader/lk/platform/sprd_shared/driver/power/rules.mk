LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

GLOBAL_INCLUDES += \
	$(LOCAL_DIR)/include
ifeq ($(PLATFORM),pike2)
MODULE_SRCS += \
	$(LOCAL_DIR)/regulator_2720.c
else ifeq ($(PLATFORM),sharkle)
MODULE_SRCS += \
	$(LOCAL_DIR)/regulator_2721.c
else ifeq ($(PLATFORM),sharkl3)
MODULE_SRCS += \
	$(LOCAL_DIR)/regulator_2721.c
else ifeq ($(PLATFORM),sharkl5)
MODULE_SRCS += \
	$(LOCAL_DIR)/regulator_2730.c
else ifeq ($(PLATFORM),sharkl5pro)
MODULE_SRCS += \
	$(LOCAL_DIR)/regulator_2730.c
else ifeq ($(PLATFORM),qogirl6)
MODULE_SRCS += \
	$(LOCAL_DIR)/regulator_2730.c
else ifeq ($(PLATFORM),qogirn6pro)
MODULE_SRCS += \
	$(LOCAL_DIR)/regulator_9620.c
else ifeq ($(PLATFORM),qogirn6l)
MODULE_SRCS += \
    $(LOCAL_DIR)/regulator_9621.c
endif

include make/module.mk
