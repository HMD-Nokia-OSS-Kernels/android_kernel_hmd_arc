QL_TIPC := $(GET_LOCAL_DIR)

GLOBAL_INCLUDES += \
	$(QL_TIPC)/include \
	$(QL_TIPC)/interface/include

MODULE_SRCS += \
	$(QL_TIPC)/arch/arm/trusty_mem.c \
	$(QL_TIPC)/arch/arm/trusty_dev.c

MODULE_SRCS += \
	$(QL_TIPC)/util.c \
	$(QL_TIPC)/sysdeps_lk.c \
	$(QL_TIPC)/trusty_dev_common.c \
	$(QL_TIPC)/ipc.c \
	$(QL_TIPC)/ipc_dev.c

MODULE_SRCS += \
	$(QL_TIPC)/crypto.c \
	$(QL_TIPC)/test/test_crypto.c \
	$(QL_TIPC)/test/trusty_ql_cademo.c \

