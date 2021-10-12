include NickelHook/NickelHook.mk

override PKGCONF  += Qt5Widgets
override LIBRARY  := src/libnm.so
override SOURCES  += src/action.c src/action_c.c src/action_cc.cc src/config.c src/generator.c src/generator_c.c src/kfmon.c src/nickelmenu.cc src/util.c
override MOCS     += src/NPGuiInterface.hpp
override CFLAGS   += -Wall -Wextra -Werror -fvisibility=hidden
override CXXFLAGS += -Wall -Wextra -Werror -Wno-missing-field-initializers -isystemlib -fvisibility=hidden -fvisibility-inlines-hidden
override KOBOROOT += res/doc:$(NM_CONFIG_DIR)/doc

override SKIPCONFIGURE += strip
strip:
	$(STRIP) --strip-unneeded src/libnm.so
.PHONY: strip

ifeq ($(NM_UNINSTALL_CONFIGDIR),1)
override CPPFLAGS += -DNM_UNINSTALL_CONFIGDIR
endif

ifeq ($(NM_CONFIG_DIR),)
override NM_CONFIG_DIR := /mnt/onboard/.adds/nm
endif

ifneq ($(NM_CONFIG_DIR),/mnt/onboard/.adds/nm)
$(info -- Warning: NM_CONFIG_DIR is set to a non-default value; this will cause issues with other mods using it!)
endif

override CPPFLAGS += -DNM_CONFIG_DIR='"$(NM_CONFIG_DIR)"' -DNM_CONFIG_DIR_DISP='"$(patsubst /mnt/onboard/%,KOBOeReader/%,$(NM_CONFIG_DIR))"'

include NickelHook/NickelHook.mk
