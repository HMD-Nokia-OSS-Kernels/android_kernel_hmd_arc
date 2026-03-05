#for AWINIC AW87XXX Smart K PA
# obj-$(CONFIG_SND_SOC_AW87XXX) += aw87xxx/aw87xxx.o  aw87xxx/aw87xxx_device.o  aw87xxx/aw87xxx_monitor.o  aw87xxx/aw87xxx_bin_parse.o  aw87xxx/aw87xxx_dsp.o  aw87xxx/aw87xxx_acf_bin.o
#
# snd-soc-sprd-codec-aw87xxx_v214.ko
#
# Makefile: for external make invocation
#
# Note:
# - Build arguments(ARCH CROSS_COMPILE O BSP_MAKE_EXTRA_ARGS
#   INSTALL_MOD_PATH etc.) are passed by parent make
# - obj-m objects configuration is moved to Kbuild file used by
#   Kernel kbuild system
# - Please use these predefined name for keeping code:
#   KO_MODULE_NAME KO_MODULE_OUT KO_MODULE_KBUILD
# - For more kernel module building, please refer to kernel doc:
#   Documentation/kbuild/modules.txt
#

export KO_MODULE_NAME := snd-soc-sprd-codec-aw87xxx_v214
KO_MODULE_OUT := $(BSP_MODULES_OUT)/$(KO_MODULE_NAME)
KO_MODULE_KBUILD := $(CURDIR)/Kbuild

.PHONY: modules modules_install clean

modules:
	@mkdir -p $(KO_MODULE_OUT) && ln -snf $(KO_MODULE_KBUILD) $(KO_MODULE_OUT)/Kbuild
	@ln -snf $(CURDIR) $(KO_MODULE_OUT)/source
	$(MAKE) -C $(BSP_KERNEL_PATH) M=$(KO_MODULE_OUT) src=$(CURDIR) $@

modules_install:
	$(MAKE) -C $(BSP_KERNEL_PATH) M=$(KO_MODULE_OUT) $@

# Remove the out directory wholly
clean:
	@#$(MAKE) -C $(BSP_KERNEL_PATH) M=$(KO_MODULE_OUT) src=$(CURDIR) $@
	rm -rf $(KO_MODULE_OUT)
