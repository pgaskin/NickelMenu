include NickelHook/NickelHook.mk

PLUGIN_DIR = src/plugins

override PKGCONF  += Qt5Widgets
override LIBRARY  := src/libnm.so
override SOURCES  += src/action.c src/action_c.c src/action_cc.cc src/config.c src/generator.c src/generator_c.c src/kfmon.c src/nickelmenu.cc src/util.c
override MOCS     += $(PLUGIN_DIR)/MNGuiInterface.h
override CFLAGS   += -Wall -Wextra -Werror -fvisibility=hidden
override CXXFLAGS += -Wall -Wextra -Werror -Wno-missing-field-initializers -isystemlib -fvisibility=hidden -fvisibility-inlines-hidden

# Find menu configs
override KOBOROOT += $(foreach config,$(shell find res -type f),$(config):$(NM_CONFIG_DIR)/$(config))

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

# Find plugins
PLUGIN_DIRS  = $(foreach dir,$(shell find $(PLUGIN_DIR) -mindepth 1 -maxdepth 1 -type d),$(notdir $(dir)))
PLUGIN_EXPORTS := $(foreach plugin,$(PLUGIN_DIRS),$(PLUGIN_DIR)/$(plugin)/$(plugin).so:/usr/local/Kobo/plugins/$(plugin).so)
override KOBOROOT += $(PLUGIN_EXPORTS)

clean:
	rm -r $(GENERATED)
	for p in $(PLUGIN_DIRS); do $(MAKE) -C $(PLUGIN_DIR)/$$p clean; done

.PHONY: plugins

export CPPFLAGS
export CXXFLAGS
export LDFLAGS
plugins:
	for p in $(PLUGIN_DIRS); do $(MAKE) -C $(PLUGIN_DIR)/$$p; done
	for f in $(PLUGIN_EXPORTS); do echo $$f; done

include NickelHook/NickelHook.mk
