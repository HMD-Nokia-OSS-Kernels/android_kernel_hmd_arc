TEECFG := $(GET_LOCAL_DIR)

MODULE_INCLUDES += \
    $(TEECFG)/include

GLOBAL_INCLUDES += \
    $(TEECFG)/include

MODULE_SRCS += \
    $(TEECFG)/teecfg_parse.c
