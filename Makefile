include nh/NickelHook.mk

override PKGCONF  += Qt5Widgets
override LIBRARY  := src/libnm.so
override SOURCES  += src/action.c src/action_c.c src/action_cc.cc src/config.c src/generator.c src/generator_c.c src/kfmon.c src/nickelmenu.cc src/util.c
override CFLAGS   += -Wall -Wextra -Werror -fvisibility=hidden
override CXXFLAGS += -Wall -Wextra -Werror -Wno-missing-field-initializers -isystemlib -fvisibility=hidden -fvisibility-inlines-hidden
override KOBOROOT += res/doc:/mnt/onboard/.adds/nm/doc

override SKIPCONFIGURE += strip
strip:
	$(STRIP) --strip-unneeded src/libnm.so
.PHONY: strip

include nh/NickelHook.mk
