KMOD = $(DRIVERS)/kmod
USER = $(DRIVERS)/user

include $(KMOD)/build.mk
include $(USER)/build.mk
