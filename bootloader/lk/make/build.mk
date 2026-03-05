# use linker garbage collection, if requested
ifeq ($(WITH_LINKER_GC),1)
GLOBAL_COMPILEFLAGS += -ffunction-sections -fdata-sections
GLOBAL_LDFLAGS += --gc-sections
endif

ifneq (,$(EXTRA_BUILDRULES))
-include $(EXTRA_BUILDRULES)
endif

$(EXTRA_LINKER_SCRIPTS):

$(OUTBIN): $(OUTELF)
	$(info generating image: $@)
	$(NOECHO)$(SIZE) $<
	$(NOECHO)$(OBJCOPY) -O binary $< $@

$(OUTELF).hex: $(OUTELF)
	$(info generating hex file: $@)
	$(NOECHO)$(OBJCOPY) -O ihex $< $@

# GNU linker's -dT(alias for --default-script) option, is similiar to the
# -T(alias for --script) option expect that processing of the script is
# delayed until after the rest of the command line has been processed.
# llvm linker does not have -dt option, so use -T instead
ifeq ($(BSP_LK_CLANG_COMPILE), true)
LDFLAGS := $(GLOBAL_LDFLAGS) $(ARCH_LDFLAGS) -T $(LINKER_SCRIPT)
else
LDFLAGS := $(GLOBAL_LDFLAGS) $(ARCH_LDFLAGS) -dT $(LINKER_SCRIPT)
endif

$(OUTELF): $(ALLMODULE_OBJS) $(EXTRA_OBJS) $(LINKER_SCRIPT) $(EXTRA_LINKER_SCRIPTS)
	$(info linking $@)
	$(NOECHO)$(SIZE) -t --common $(sort $(ALLMODULE_OBJS)) $(EXTRA_OBJS)
	$(NOECHO)$(LD) $(LDFLAGS) \
		$(addprefix -T,$(EXTRA_LINKER_SCRIPTS)) \
		$(ALLMODULE_OBJS) $(EXTRA_OBJS) $(LIBGCC) -Map=$(OUTELF).map -o $@
	$(NOECHO)$(DEBUG_ADD_FUNCTION_SYMBOL_SCRIPT) $@

$(OUTELF).sym: $(OUTELF)
	$(info generating symbols: $@)
	$(NOECHO)$(OBJDUMP) -t $< | $(CPPFILT) > $@

$(OUTELF).sym.sorted: $(OUTELF)
	$(info generating sorted symbols: $@)
	$(NOECHO)$(OBJDUMP) -t $< | $(CPPFILT) | sort > $@

$(OUTELF).lst: $(OUTELF)
	$(info generating listing: $@)
	$(NOECHO)$(OBJDUMP) $(ARCH_OBJDUMP_FLAGS) -d $< | $(CPPFILT) > $@

$(OUTELF).debug.lst: $(OUTELF)
	$(info generating listing: $@)
	$(NOECHO)$(OBJDUMP) $(ARCH_OBJDUMP_FLAGS) -S $< | $(CPPFILT) > $@

$(OUTELF).dump: $(OUTELF)
	$(info generating objdump: $@)
	$(NOECHO)$(OBJDUMP) -x $< > $@

$(OUTELF).size: $(OUTELF)
	$(info generating size map: $@)
	$(NOECHO)$(NM) -S --size-sort $< > $@

# print some information about the build
$(BUILDDIR)/srcfiles.txt: $(OUTELF)
	$(info generating $@)
	$(NOECHO)echo $(sort $(ALLSRCS)) | tr ' ' '\n' > $@

$(BUILDDIR)/include_paths.txt: $(OUTELF)
	$(info generating $@)
	$(NOECHO)echo $(subst -I,,$(sort $(GLOBAL_INCLUDES))) | tr ' ' '\n' > $@

#include arch/$(ARCH)/compile.mk

