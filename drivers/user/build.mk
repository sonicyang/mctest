
USER_BUILD := $(BUILD)/drivers

LIBS := -lrt -lpthread

.PHONY: user
user: motion
	@mkdir -p $(USER_BUILD)
	$(CPP) $(MOTION_CPPFLAGS) -o $(USER_BUILD)/main.o \
		-MMD -MF $(USER_BUILD)/main.o.d \
		-c $(USER)/main.cpp
	$(CPP) -static -O3 -o $(USER_BUILD)/mctest \
		$(MOTION_OBJS) $(MOTION_ROBOT_OBJS) $(USER_BUILD)/main.o \
		$(LIBS)
.PHONY: user_clean
user_clean:
	-rm -rf $(USER_BUILD)
	-rm -rf $(BUILD)

ifeq ($(TARGET), user)
ALL_SUB_TARGET += user
CLEAN_SUB_TARGET += user_clean
endif

-include $(USER_BUILD)/main.o.d
