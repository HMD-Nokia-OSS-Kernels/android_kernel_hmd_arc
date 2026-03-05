TOOL_CHAIN_ROOT=$(BSP_ROOT_DIR)/toolchain

BSP_LK_CLANG_COMPILE ?= false
LK_CROSS_COMPILE_CLANG := $(TOOL_CHAIN_ROOT)/prebuilts/clang/host/linux-x86/$(BSP_CLANG_VERSION)/bin/

ifeq ($(BSP_BOARD_ARCH), arm64)
LK_ARCH := arm64
ifeq ($(BSP_LK_CLANG_COMPILE), true)
LK_CROSS_COMPILE := $(LK_CROSS_COMPILE_CLANG)
else
LK_CROSS_COMPILE := $(TOOL_CHAIN_ROOT)/prebuilts/gcc/linaro-x86/aarch64/gcc-linaro-4.8/gcc-linaro-4.8-2015.06-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-
endif

else ifeq ($(BSP_BOARD_ARCH), arm)
LK_ARCH := arm
ifeq ($(BSP_LK_CLANG_COMPILE), true)
LK_CROSS_COMPILE := $(LK_CROSS_COMPILE_CLANG)
else
LK_CROSS_COMPILE := $(TOOL_CHAIN_ROOT)/prebuilts/gcc/linux-x86/arm/arm-eabi-4.8/bin/arm-eabi-
endif

else ifeq ($(BSP_BOARD_ARCH),)
$(error $(BSP_BOARD_ARCH) is not null)
endif

LK_BUILD_OPTION := \
-f makefile \
TOOLCHAIN_PREFIX=$(LK_CROSS_COMPILE) \
ARCH_$(LK_ARCH)_TOOLCHAIN_PREFIX=$(LK_CROSS_COMPILE) \
BUILDROOT=$(BSP_LK_OUT) \
BSP_LK_CLANG_COMPILE=$(BSP_LK_CLANG_COMPILE) \
DEFAULT_PROJECT=$(BSP_LK_DEFCONFIG)

#LK_HEAP_IMPLEMENTATION=miniheap

LK_CLANG_BUILD_OPTION := \
-f makefile \
TOOLCHAIN_PREFIX=$(LK_CROSS_COMPILE_CLANG) \
ARCH_$(LK_ARCH)_TOOLCHAIN_PREFIX=$(LK_CROSS_COMPILE_CLANG) \
BUILDROOT=$(BSP_LK_OUT) \
BSP_LK_CLANG_COMPILE=true \
DEFAULT_PROJECT=$(BSP_LK_DEFCONFIG)

# project should be built pass with both gcc and clang
define check_clang_build
	@if [ true != $(BSP_LK_CLANG_COMPILE) ]; then \
		if [ ! -e $(LK_CROSS_COMPILE_CLANG)clang ]; then \
			echo "skip lk clang build check"; \
			exit 0; \
		fi; \
		echo "check lk building with clang..."; \
		rm -rf $(BSP_LK_OUT) 2>/dev/null; \
		rm -rf $(BSP_LK_DIST) 2>/dev/null; \
		mkdir -p $(BSP_LK_DIST); \
		$(MAKE) AUTOBOOT_FLAG=true $(LK_CLANG_BUILD_OPTION); \
		mkdir -p $(BSP_LK_OUT)/.out && \
		$(MAKE) $(LK_CLANG_BUILD_OPTION) clean; \
		$(MAKE) $(LK_CLANG_BUILD_OPTION); \
		if [ ! -e "$(BSP_LK_OUT)/build-$(BSP_LK_DEFCONFIG)/lk.bin" ]; then \
			echo "lk clang build fail"; \
			exit 1; \
		fi; \
		rm -rf $(BSP_LK_OUT) 2>/dev/null; \
		rm -rf $(BSP_LK_DIST) 2>/dev/null; \
	fi
endef

all:
	$(call check_clang_build)
	@echo "lk building..."
	@mkdir -p $(BSP_LK_DIST)
	$(MAKE)  AUTOBOOT_FLAG=true $(LK_BUILD_OPTION)
	@mkdir -p $(BSP_LK_OUT)/.out
	@cp $(BSP_LK_OUT)/build-$(BSP_LK_DEFCONFIG)/lk.bin $(BSP_LK_OUT)/.out/lk_autopoweron.bin
	@cp $(BSP_LK_OUT)/build-$(BSP_LK_DEFCONFIG)/lk.elf $(BSP_LK_OUT)/.out/lk_autopoweron.elf
	@cp $(BSP_LK_OUT)/build-$(BSP_LK_DEFCONFIG)/lk.elf.map $(BSP_LK_OUT)/.out/lk.elf_autopoweron.map
	@cp $(BSP_LK_OUT)/build-$(BSP_LK_DEFCONFIG)/config.h $(BSP_LK_OUT)/.out/config_autopoweron.h
	$(MAKE)  $(LK_BUILD_OPTION) clean
	$(MAKE)  $(LK_BUILD_OPTION)
	@mv $(BSP_LK_OUT)/.out/* $(BSP_LK_OUT)/build-$(BSP_LK_DEFCONFIG)
	@rm -rf $(BSP_LK_OUT)/.out

