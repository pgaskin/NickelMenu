include NickelHook/NickelHook.mk

PLUGIN_SRC_DIR     = src/plugins
PLUGIN_DEST_DIR    = /usr/local/Kobo/plugins

override PKGCONF  += Qt5Widgets
override LIBRARY  := src/libnm.so
override SOURCES  += src/action.c src/action_c.c src/action_cc.cc src/config.c src/generator.c src/generator_c.c src/kfmon.c src/nickelmenu.cc src/util.c
override MOCS     += $(PLUGIN_SRC_DIR)/NPGuiInterface.h
override CFLAGS   += -Wall -Wextra -Werror -fvisibility=hidden
override CXXFLAGS += -Wall -Wextra -Werror -Wno-missing-field-initializers -isystemlib -fvisibility=hidden -fvisibility-inlines-hidden

# Find menu plugin configs
PLUGIN_CONFIGS = $(foreach config,$(shell find res/*_plugin -not -path '*/.*' -type f | sed -e 's,^res/,,'),res/$(config):$(NM_CONFIG_DIR)/$(config))
ifeq ($(PLUGIN_CONFIGS),)
override PLUGIN_CONFIGS = $(foreach config,$(shell find res -not -path '*/.*' -type f | sed -e 's,^res/,,'),res/$(config):$(NM_CONFIG_DIR)/$(config))
endif
override KOBOROOT += $(PLUGIN_CONFIGS)

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
PLUGIN_SRC_DIRS  = $(foreach dir,$(shell find $(PLUGIN_SRC_DIR) -mindepth 1 -maxdepth 1 -type d),$(notdir $(dir)))
PLUGIN_EXPORTS := $(foreach plugin,$(PLUGIN_SRC_DIRS),$(PLUGIN_SRC_DIR)/$(plugin)/$(plugin).so:$(PLUGIN_DEST_DIR)/$(plugin).so)
override KOBOROOT += $(PLUGIN_EXPORTS)

# Find plugin extra resources
PLUGIN_EXTRAS := $(foreach plugin,$(PLUGIN_SRC_DIRS),$(shell find $(PLUGIN_SRC_DIR)/$(plugin)/res/* -not -path '*/.*' -type f))
PLUGIN_EXTRA_EXPORTS := $(foreach extra,$(PLUGIN_EXTRAS),$(extra):$(shell echo $(extra) | sed -e 's,^$(PLUGIN_SRC_DIR)/.*/res/,$(PLUGIN_DEST_DIR)/res/,'))
override KOBOROOT += $(PLUGIN_EXTRA_EXPORTS)

clean:
	rm -r $(GENERATED)
	for p in $(PLUGIN_SRC_DIRS); do $(MAKE) -C $(PLUGIN_SRC_DIR)/$$p clean; done

.PHONY: plugins

export CPPFLAGS
export CXXFLAGS
export LDFLAGS
plugins:
	for p in $(PLUGIN_SRC_DIRS); do $(MAKE) -C $(PLUGIN_SRC_DIR)/$$p; done
	for f in $(PLUGIN_EXPORTS); do echo $$f; done

include NickelHook/NickelHook.mk
