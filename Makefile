ROOT := $(PWD)
ALL_SUB_TARGET :=
CLEAN_SUB_TARGET :=
DISTCLEAN_SUB_TARGET :=

.DEFAULT_GOAL := all

# Architecture
ifeq ($(TARGET), kmod)
    # detect native build for kernel module
    ifndef KDIR
        ifneq ("$(wildcard /lib/modules/$(shell uname -r)/build/.config)","")
            KDIR=/lib/modules/$(shell uname -r)/build
        endif
    endif
endif

ifdef KDIR
CONFIG_FILE = $(realpath $(KDIR))/.config
ARCH_CONFIG = `cat $(CONFIG_FILE)|grep CONFIG_$(1)=y`
ARCH ?= $(if $(shell echo $(call ARCH_CONFIG,X86)),x86,$(if $(shell echo $(call ARCH_CONFIG,ARM)),arm))
endif

# Toolchain
CC=$(CROSS_COMPILE)gcc
CPP=$(CROSS_COMPILE)g++
AR=$(CROSS_COMPILE)ar

COMMON_COMPILE_FLAGS =

ifeq ($(TARGET), kmod)
    COMMON_COMPILE_FLAGS += -fno-pic
    ifeq ($(ARCH), arm)
        PLATFORM_COMPILE_FLAGS += -mno-thumb-interwork -mfpu=neon -marm
        PLATFORM_COMPILE_FLAGS += -march=armv7-a -mfloat-abi=softfp
        PLATFORM_COMPILE_FLAGS += -Uarm -D__LINUX_ARM_ARCH__=7 -D__ARM_PCS_VFP
        PLATFORM_COMPILE_FLAGS += -mapcs -mno-sched-prolog -mlittle-endian
    else
        PLATFORM_COMPILE_FLAGS += -m64 -mcmodel=kernel -mtune=generic -mno-red-zone
        PLATFORM_COMPILE_FLAGS += -mstackrealign
    endif
endif

# utility
TOOLS=$(PWD)/scripts

# build output directory
ifeq ($(TARGET), kmod)
BUILD := $(PWD)/build/kmod
else
BUILD := $(PWD)/build/user
endif
MOTION_BUILD := $(BUILD)/motion
MOTION_ROBOT_BUIlD := $(BUILD)/motion/Robot

$(BUILD):
	@mkdir -p $(BUILD)

# external
EXTERNAL := $(PWD)/external
NEWLIB := $(EXTERNAL)/newlib
include $(EXTERNAL)/build.mk

# motion
MOTION := $(PWD)/motion
#include $(MOTION)/build.mk

# drivers
DRIVERS :=$(PWD)/drivers
include $(DRIVERS)/build.mk

.PHONY: all clean disclean

all: $(ALL_SUB_TARGET)

clean: $(CLEAN_SUB_TARGET)

distclean: clean $(DISTCLEAN_SUB_TARGET)
	-rm -rf build
