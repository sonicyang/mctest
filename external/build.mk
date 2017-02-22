EXTERNAL_BUILD = $(BUILD)/external
LIBM_BUILD = $(BUILD)/external/libm
LIBM = $(NEWLIB)/newlib/libm

NEWLIB:=$(NEWLIB)

NEWLIB_GITURL ?= git://sourceware.org/git/newlib-cygwin.git

libm_git_fetch = \
	git clone $(NEWLIB_GITURL) $(NEWLIB)

ifeq ($(ARCH),arm)
	NEWLIB_PLATGFORM_COMPILE_FLAGS = $(PLATFORM_COMPILE_FLAGS) -D_LDBL_EQ_DBL=1
	NEWLIB_HOST_FLAGS = --host=$(ARCH)
else
	NEWLIB_PLATGFORM_COMPILE_FLAGS = $(PLATFORM_COMPILE_FLAGS)
	NEWLIB_HOST_FLAGS =
endif

libm_config_cmd = CC=$(CC) AR=$(AR) CFLAGS='$(NEWLIB_PLATGFORM_COMPILE_FLAGS) $(COMMON_COMPILE_FLAGS) -I$(NEWLIB)/newlib/libc/include' $(LIBM)/configure $(NEWLIB_HOST_FLAGS)  --enable-static


libm_configure = \
	cd  $(LIBM_BUILD) && \
	$(call libm_config_cmd) && \
	cd $(ROOT)

$(NEWLIB):
	$(call libm_git_fetch)


$(LIBM_BUILD)/Makefile: $(NEWLIB)
	mkdir -p $(LIBM_BUILD)
	$(call libm_configure)

#FIXME: The first step can create all object files for libm but 
#       it would fail due to recursive building which is not
#       necessary for libm.a. The second setp build the libm.a but
#       it can not create object files only by itself.  Currently, 
#       the workaround is to combine these two setps and ignore the 
#       error in first step.
$(LIBM_BUILD)/libm.a: $(LIBM_BUILD)/Makefile
	-make -C $(LIBM_BUILD)
	AR_FLAGS='rcs' make -C $(LIBM_BUILD) libm.a

.PHONY: libm
libm: $(LIBM_BUILD)/libm.a

.PHONY: libm_clean libm_distclean external_clean
libm_clean:
	-rm -rf $(LIBM_BUILD)
	-echo "libm_clean"

libm_distclean:
	-rm -rf $(NEWLIB)

external_clean:
	-rm -rf $(EXTERNAL_BUILD)

ifeq ($(TARGET), kmod)
ALL_SUB_TARGET += libm
CLEAN_SUB_TARGET += libm_clean external_clean
DISTCLEAN_SUB_TARGET += libm_distclean
endif
