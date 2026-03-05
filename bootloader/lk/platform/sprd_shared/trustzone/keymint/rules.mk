KEYMINT_CA := $(GET_LOCAL_DIR)

MODULE_INCLUDES += \
    $(KEYMINT_CA)/include

GLOBAL_INCLUDES += \
    $(KEYMINT_CA)/include/interface \
    $(KEYMINT_CA)/include/hardware \

MODULE_SRCS += \
    $(KEYMINT_CA)/ipc/lk_keymint_ipc.c \
    $(KEYMINT_CA)/lk_keymint_device.c \
    $(KEYMINT_CA)/lk_keymint_messages.c \
    $(KEYMINT_CA)/serializable.c \
