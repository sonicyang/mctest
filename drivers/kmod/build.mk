
KMOD_REQUIRES := libm motion

KMOD_BUILD := $(BUILD)/drivers

mctest-kobj := mctest.ko
V ?= 1
W ?= 1

# FIXME
MOTION_INTERFACE_SRCS = $(KMOD)/motion_interface.cpp
MOTION_INTERFACE_OBJS = $(patsubst %.cpp, $(KMOD)/%.o, $(notdir $(MOTION_INTERFACE_SRCS)))

deps := $(MOTION_INTERFACE_OBJS:%.o=%.o.d)

$(MOTION_INTERFACE_OBJS): $(MOTION_INTERFACE_SRCS)
	@mkdir -p $(KMOD)
	$(CPP) $(MOTION_CPPFLAGS) -o $@ -MMD -MF $@.d -c $^

$(KMOD_BUILD): $(BUILD)
	mkdir -p $(KMOD_BUILD)

$(KMOD)/$(mctest-kobj): $(KMOD_REQUIRES) $(MOTION_INTERFACE_OBJS)
	cp -rf $(MOTION_BUILD)/*.o $(KMOD)
	cp -rf $(MOTION_ROBOT_BUILD)/*.o $(KMOD)
	cp -rf $(LIBM_BUILD)/libm.a $(KMOD)
	export MOTION_OBJS='$(notdir $(MOTION_OBJS))' && export MOTION_ROBOT_OBJS='$(notdir $(MOTION_ROBOT_OBJS))' ARCH=$(ARCH) && $(MAKE) -C $(KDIR) V=$(V) W=$(W) M=$(KMOD) modules


$(KMOD_BUILD)/$(mctest-kobj): $(KMOD)/$(mctest-kobj) $(KMOD_BUILD)
	cp -rf  $(KMOD)/$(mctest-kobj) $(KMOD_BUILD)/$(mctest-kobj)

.PHONY: kmod
kmod: $(KMOD_BUILD)/$(mctest-kobj)

.PHONY: kmod_clean
kmod_clean:
	$(MAKE) -C $(KDIR) M=$(KMOD) V=$(V) clean
	-rm -rf $(KMOD_BUILD) $(deps)
	-rm -rf $(BUILD)
	-echo "kmod_clean"

ifeq ($(TARGET), kmod)
ALL_SUB_TARGET += kmod
CLEAN_SUB_TARGET += kmod_clean
endif

-include $(deps)
