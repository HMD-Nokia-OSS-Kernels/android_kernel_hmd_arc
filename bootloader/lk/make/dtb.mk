# *.dts file absolute path
DTS_S := $(LKROOT)/platform/common/dts

ifneq ($(strip $(DEVICE_TREE)),)
DEST_DTS := $(DTS_S)/$(DEVICE_TREE).dts
else
DEST_DTS := $(DTS_S)/$(PROJECT).dts
endif

ifeq ($(wildcard $(DEST_DTS)),)
$(error Not found "$(DEST_DTS)" in the $(DTS_S)/ directory.)
endif

DTC=$(LKROOT)/tools/dtc/dtc

ifeq ($(wildcard $(DTC)),)
$(error DTC:$(BSP_ROOT_DIR)/tools/scan_dts/dtc does not exist)
endif

DTB_TEMP := $(BUILDDIR)/platform/common/dts

DTC_FLAGS +=


#1.$(depfile).pre.tmp -->.sp9863a_1h10_go.dtb.d.pre.tmp
#2.$(dtc-tmp)--->.sp9863a_1h10_go.dtb.dts.tmp
#3.$(depfile).dtc.tmp --->.sp9863a_1h10_go.dtb.d.dtc.tmp

depfile = .$(notdir $<).d
dtc-tmp = .$(notdir $<).dts.tmp

cmd_dtc = $(CPP) -Wp,-MD,$(DTB_TEMP)/$(depfile).pre.tmp -nostdinc  \
          -I$(DTS_S)                \
          -I$(DTS_S)/include   \
          -undef -D__DTS__        \
          -x assembler-with-cpp -o $(DTB_TEMP)/$(dtc-tmp) $< ; \
        $(DTC) -O dtb -o $@ -b 0 \
        -i $(dir $<) $(DTC_FLAGS) \
        -d $(DTB_TEMP)/$(depfile).dtc.tmp $(DTB_TEMP)/$(dtc-tmp) ; \
        cat $(DTB_TEMP)/$(depfile).pre.tmp $(DTB_TEMP)/$(depfile).dtc.tmp > $(DTB_TEMP)/$(depfile)


$(OUTDTB): $(DEST_DTS)
	@rm -rf $(DTB_TEMP) $@
	@mkdir -p $(@D)
	@mkdir -p $(DTB_TEMP)
	@$(cmd_dtc)
