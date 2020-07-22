# GNU Makefile include for NickelHook plugins

# literals
override nh_parl := (
override nh_parr := )
override nh_comma := ,
override define nh_newline


endef

ifneq ($(firstword $(MAKEFILE_LIST)),$(lastword $(MAKEFILE_LIST)))

ifndef nh_top
CROSS_COMPILE  = arm-nickel-linux-gnueabihf-
MOC            = moc
CC             = $(CROSS_COMPILE)gcc
CXX            = $(CROSS_COMPILE)g++
PKG_CONFIG     = $(CROSS_COMPILE)pkg-config

DESTDIR =

CFLAGS   ?= -O2 -march=armv7-a -mtune=cortex-a8 -mfpu=neon -mfloat-abi=hard -mthumb
CXXFLAGS ?= -O2 -march=armv7-a -mtune=cortex-a8 -mfpu=neon -mfloat-abi=hard -mthumb
LDFLAGS  ?= -Wl,--as-needed

all:
endif

ifdef nh_top
## variable: NickelHook dir
NICKELHOOK ?= $(dir $(lastword $(MAKEFILE_LIST)))

## variable: pkg-config dependencies (will be added to CFLAGS/CXXFLAGS/LDFLAGS)
# override PKGCONF += <dep>
# override PKGCONF += <NAME>,<dep>
# override PKGCONF += <NAME>,<dep>,<--constraint=...>
override PKGCONF += Qt5Core Qt5Gui

## variable: allows pkg-config to be skipped for certain targets
# override SKIPCONFIGURE += <target>
override SKIPCONFIGURE += clean gitignore install koboroot

## variable: flags
# override VARIABLE += <flags>
override CPPFLAGS += -I$(NICKELHOOK)
override CFLAGS   += -std=gnu11 -pthread
override CXXFLAGS += -std=gnu++11 -pthread
override LDFLAGS  += -Wl,--no-undefined -Wl,-rpath,/usr/local/Kobo -Wl,-rpath,/usr/local/Qt-5.2.1-arm/lib -pthread -ldl

## variable: version (don't set this unless you don't use git)
# override VERSION := <version>

## variable: library name
# override LIBRARY := <soname>
$(if $(LIBRARY),,$(error LIBRARY must be set to the soname of the target library (example: libnm.so)))

## variable: sources (*.c, *.cc, *.cpp)
$(if $(SOURCES),,$(error SOURCES must be set to the *.c, *.cc source files))
override SOURCES += $(NICKELHOOK)nh.c

## variable: qt mocs (*.h)
override MOCS += $(NICKELHOOK)nhplugin.h

## variable: installed files
# override KOBOROOT += <source>:<dest>
override KOBOROOT += $(LIBRARY):/usr/local/Kobo/imageformats/$(notdir $(LIBRARY))

## generated files
# override GENERATED += <file>
override OBJECTS_C    := $(filter %.o,$(SOURCES:%.c=%.o))
override OBJECTS_CXX  := $(filter %.o,$(SOURCES:%.cc=%.o))
override OBJECTS_CXX1 := $(filter %.o,$(SOURCES:%.cpp=%.o))
override MOCS_MOC     := $(filter %.moc,$(MOCS:%.h=%.moc))
override OBJECTS_MOC  := $(MOCS_MOC:%=%.o)
override GENERATED    += KoboRoot.tgz $(LIBRARY) $(OBJECTS_C) $(OBJECTS_CXX) $(OBJECTS_CXX1) $(MOCS_MOC) $(OBJECTS_MOC)

## gitignore
# override GITIGNORE += <pattern>
override GITIGNORE += .kdev4/ *.kdev4 .kateconfig .vscode/ .idea/ .clangd/ .cache/ compile_commands.json $(addprefix /,$(GENERATED))

###

ifneq ($(if $(MAKECMDGOALS),$(if $(filter-out $(SKIPCONFIGURE),$(MAKECMDGOALS)),YES,NO),YES),YES)
 $(info -- Skipping configure)
else

# get version info
ifndef VERSION
 override VERSION := $(shell git describe --tags --always --dirty 2>/dev/null)
 $(if $(VERSION),,$(info -- Warning: Could not detect version with git))
endif

# add version info
ifdef VERSION
 $(info -- Detected version: $(VERSION))
 override CPPFLAGS += -DNH_VERSION='"$(VERSION)"'
endif

# pkgconf function
override define pkgconf =
 $(if $(filter-out undefined,$(origin $(strip $(1))_CFLAGS) $(origin $(strip $(1))_LIBS)) \
 ,$(info -- Using provided CFLAGS and LIBS for $(strip $(2))) \
 ,$(if $(shell $(PKG_CONFIG) --exists $(strip $(2)) >/dev/null 2>/dev/null && echo y) \
  ,$(info -- Found $(strip $(2)) ($(shell $(PKG_CONFIG) --modversion $(strip $(2)))) with pkg-config) \
   $(eval $(strip $(1))_CFLAGS := $(shell $(PKG_CONFIG) --silence-errors --cflags $(strip $(2)))) \
   $(eval $(strip $(1))_LIBS   := $(shell $(PKG_CONFIG) --silence-errors --libs $(strip $(2)))) \
   $(if $(strip $(3)) \
   ,$(if $(shell $(PKG_CONFIG) $(strip $(3)) $(strip $(2)) >/dev/null 2>/dev/null && echo y) \
    ,$(info .. Satisfies constraint $(strip $(3))) \
    ,$(info .. Does not satisfy constraint $(strip $(3))) \
     $(error Dependencies do not satisfy constraints)) \
   ,) \
  ,$(info -- Could not automatically detect $(strip $(2)) with pkg-config. Please specify $(strip $(1))_CFLAGS and/or $(strip $(1))_LIBS manually) \
   $(error Missing dependencies)))
endef

# transform `pkg` into `PKG,pkg` in `$(PKGCONF)`
override PKGCONF := \
 $(foreach dep,$(PKGCONF) \
 ,$(strip $(if $(findstring $(nh_comma),$(dep)) \
  ,$(dep) \
  ,$(shell echo -n "$(dep)" | tr '[:lower:]' '[:upper:]')$(nh_comma)$(dep))))

# call pkgconf for each item in `$(PKGCONF)`
$(foreach dep,$(PKGCONF) \
,$(call pkgconf \
 ,$(word 1,$(subst $(nh_comma), ,$(dep))) \
 ,$(word 2,$(subst $(nh_comma), ,$(dep))) \
 ,$(word 3,$(subst $(nh_comma), ,$(dep)))))

# add flags
override CFLAGS   += $(foreach dep,$(PKGCONF),$($(word 1,$(subst $(nh_comma), ,$(dep)))_CFLAGS))
override CXXFLAGS += $(foreach dep,$(PKGCONF),$($(word 1,$(subst $(nh_comma), ,$(dep)))_CFLAGS))
override LDFLAGS  += $(foreach dep,$(PKGCONF),$($(word 1,$(subst $(nh_comma), ,$(dep)))_LIBS))

endif

all: $(LIBRARY)

clean:
	rm -f $(GENERATED)

gitignore:
	echo "# make gitignore" > .gitignore
	echo "$(strip $(GITIGNORE))" | tr " " "\n" >> .gitignore

define nh_install =
install:
	$(foreach file,$(1),$(nh_newline)$(shell printf '\011')install -Dm644 $(word 1,$(subst :, ,$(file))) $(DESTDIR)$(word 2,$(subst :, ,$(file))))
endef

$(eval $(call nh_install,$(KOBOROOT)))

koboroot:
	tar cvzf KoboRoot.tgz --show-transformed --owner=root --group=root --mode="u=rwX,go=rX" $(foreach file,$(KOBOROOT),--transform="s,$(word 1,$(subst :, ,$(file))),.$(word 2,$(subst :, ,$(file))),") $(foreach file,$(KOBOROOT),$(word 1,$(subst :, ,$(file))))

.PHONY: all clean gitignore install koboroot

%.so clangd: override CFLAGS   += -fPIC
%.so clangd: override CXXFLAGS += -fPIC
%.so clangd: override LDFLAGS  += -Wl,-soname,$(notdir $@)

$(LIBRARY): $(OBJECTS_C) $(OBJECTS_CXX) $(OBJECTS_CXX1) $(OBJECTS_MOC)

override nh_cmd_so   = $(CXX) $(CPPFLAGS) $(CXXFLAGS) -shared -o $(1) $(2) $(LDFLAGS)
override nh_cmd_c    = $(CC) $(CPPFLAGS) $(CFLAGS) -c $(2) -o $(1)
override nh_cmd_cc   = $(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $(2) -o $(1)
override nh_cmd_moco = $(CXX) -xc++ $(CPPFLAGS) $(CXXFLAGS) -c $(2) -o $(1)
override nh_cmd_moch = $(MOC) $(2) -o $(1)

$(LIBRARY): %.so:
	$(call nh_cmd_so,$@,$^)
$(OBJECTS_C): %.o: %.c
	$(call nh_cmd_c,$@,$^)
$(OBJECTS_CXX): %.o: %.cc
	$(call nh_cmd_cc,$@,$^)
$(OBJECTS_CXX1): %.o: %.cpp
	$(call nh_cmd_cc,$@,$^)
$(OBJECTS_MOC): %.moc.o: %.moc
	$(call nh_cmd_moco,$@,$^)
$(MOCS_MOC): %.moc: %.h
	$(call nh_cmd_moch,$@,$^)

override nh_clangd_file = {"directory": "$(realpath $(CURDIR))", "file": "$(3)", "command": "$(subst \,\\,$(subst ",\",$(call $(1),$(2),$(3))))"}
override nh_clangd_objs = $(foreach object,$(3),$(nh_comma) $(call nh_clangd_file,nh_cmd_$(1),$(object),$(patsubst %.o,$(2),$(object))))

clangd:
	$(if $(CROSS_COMPILE),$(info note: you probably want to use 'make clangd CROSS_COMPILE= CC=clang-10 CXX=clang++-10 CFLAGS= CXXFLAGS=' instead))
	echo -n "[\n    " > compile_commands.json
	echo -n "$(subst ",\",$(strip \
		$(call nh_clangd_objs,c,%.c,$(OBJECTS_C)) \
		$(call nh_clangd_objs,cc,%.cc,$(OBJECTS_CXX)) \
		$(call nh_clangd_objs,cc,%.cpp,$(OBJECTS_CXX1)) \
		$(call nh_clangd_objs,moco,%,$(OBJECTS_MOC)) \
	))" | tail -c+3 | sed 's/ , /,\n    /g' >> compile_commands.json
	echo -n "\n]" >> compile_commands.json
.PHONY: clangd
endif

override nh_top := true
override nh_lib := true
endif

# generator
ifndef nh_lib
$(if $(NAME),,$(error Please specify NAME (ex: MyMod)))

override nh_namet := $(shell echo "$(NAME)"     | tr -d ' [:lower:]')
override nh_nameu := $(shell echo "$(nh_namet)" | tr '[:upper:]' '[:lower:]')
override nh_names := $(shell echo "$(NAME)"     | tr -d ' ')
override nh_namel := $(shell echo "$(nh_names)" | tr '[:upper:]' '[:lower:]')

$(if $(nh_namet),,$(error NAME must contain at least one uppercase letter, preferably more to be unique))

LIBRARY  ?= lib$(nh_namel).so
NHSOURCE ?= src/$(nh_namel).cc
SOURCES  ?= $(wildcard src/*.c src/*.cc)

override nh_dir    := $(dir $(lastword $(MAKEFILE_LIST)))
override nh_mkdir   = @echo "$@"; mkdir $@
override nh_create  = @echo "$@"; echo -n > $@
override nh_write   = @echo "| $(subst ",\",$(1))"; echo "$(subst ",\",$(1))" >> $@

$(if $(wildcard $(NHSOURCE)),$(info Warning: File named $(NHSOURCE) found in src dir, not generating NickelHook base.))
$(if $(wildcard Makefile),$(info Warning: Makefile already exists, not generating.))

all: Makefile src $(NHSOURCE)
	@echo "Finishing up."
	make gitignore
.PHONY: all

Makefile:
	$(call nh_create)
	$(call nh_write,include $(nh_dir)NickelHook.mk)
	$(call nh_write,)
	$(call nh_write,override LIBRARY  := $(LIBRARY))
	$(call nh_write,override SOURCES  += $(NHSOURCE) $(sort $(filter-out $(NHSOURCE), $(SOURCES))))
	$(call nh_write,override CFLAGS   += -Wall -Wextra -Werror)
	$(call nh_write,override CXXFLAGS += -Wall -Wextra -Werror -Wno-missing-field-initializers)
	$(call nh_write,)
	$(call nh_write,include $(nh_dir)NickelHook.mk)

src:
	$(call nh_mkdir)

$(NHSOURCE): src
	$(call nh_create)
	$(call nh_write,#include <cstddef>)
	$(call nh_write,#include <cstdlib>)
	$(call nh_write,)
	$(call nh_write,#include <NickelHook.h>)
	$(call nh_write,)
	$(call nh_write,static struct nh_info $(nh_names) = {)
	$(call nh_write,    .name           = "$(NAME)"$(nh_comma))
	$(call nh_write,    .desc           = ""$(nh_comma))
	$(call nh_write,    .uninstall_flag = "/mnt/onboard/$(nh_nameu)_uninstall"$(nh_comma))
	$(call nh_write,};)
	$(call nh_write,)
	$(call nh_write,static struct nh_hook $(nh_names)Hook[] = {)
	$(call nh_write,    {0}$(nh_comma))
	$(call nh_write,};)
	$(call nh_write,)
	$(call nh_write,static struct nh_dlsym $(nh_names)Dlsym[] = {)
	$(call nh_write,    {0}$(nh_comma))
	$(call nh_write,};)
	$(call nh_write,)
	$(call nh_write,NickelHook$(nh_parl))
	$(call nh_write,    .init  = nullptr$(nh_comma))
	$(call nh_write,    .info  = &$(nh_names)$(nh_comma))
	$(call nh_write,    .hook  = $(nh_names)Hook$(nh_comma))
	$(call nh_write,    .dlsym = $(nh_names)Dlsym$(nh_comma))
	$(call nh_write,$(nh_parr))
endif
