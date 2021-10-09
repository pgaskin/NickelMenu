CROSS_COMPILE = arm-nickel-linux-gnueabihf-
MOC           = moc
CC            = $(CROSS_COMPILE)gcc
CXX           = $(CROSS_COMPILE)g++
PKG_CONFIG    = $(CROSS_COMPILE)pkg-config
STRIP         = $(CROSS_COMPILE)strip
AS            = $(CROSS_COMPILE)as
# NOTE: Point to the GCC wrappers for sane LTO handling.
#       But don't use LTO, it's terrible on GCC 4.9
AR            = $(CROSS_COMPILE)gcc-ar
RANLIB        = $(CROSS_COMPILE)gcc-ranlib
NM            = $(CROSS_COMPILE)gcc-nm
LD            = $(CROSS_COMPILE)ld
READELF       = $(CROSS_COMPILE)readelf
OBJCOPY       = $(CROSS_COMPILE)objcopy
OBJDUMP       = $(CROSS_COMPILE)objdump
STRINGS       = $(CROSS_COMPILE)strings

override LIBRARY  := libnmexampleplugin.so
override SOURCES  := NMExamplePlugin.cc NMDialog.cc
override MOCS     := NMExamplePlugin.h NMDialog.h
override CXXFLAGS += -fPIC

override OBJECTS_CXX  := $(filter %.o,$(SOURCES:%.cc=%.o))
override MOCS_MOC     := $(filter %.moc,$(MOCS:%.h=%.moc))
override OBJECTS_MOC  := $(MOCS_MOC:%=%.o)
override GENERATED    := $(LIBRARY) $(OBJECTS_CXX) $(MOCS_MOC) $(OBJECTS_MOC)

$(LIBRARY): $(OBJECTS_CXX) $(OBJECTS_MOC)

override nh_cmd_so   = $(CXX) $(CPPFLAGS) $(CXXFLAGS) -shared -o $(1) $(2) $(LDFLAGS)
override nh_cmd_cc   = $(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $(2) -o $(1)
override nh_cmd_moco = $(CXX) -xc++ $(CPPFLAGS) $(CXXFLAGS) -c $(2) -o $(1)
override nh_cmd_moch = $(MOC) $(2) -o $(1)

all: $(LIBRARY)

clean:
	rm -f $(GENERATED)

$(LIBRARY): %.so:
	$(call nh_cmd_so,$@,$^)
$(OBJECTS_CXX): %.o: %.cc
	$(call nh_cmd_cc,$@,$^)
$(OBJECTS_MOC): %.moc.o: %.moc
	$(call nh_cmd_moco,$@,$^)
$(MOCS_MOC): %.moc: %.h
	$(call nh_cmd_moch,$@,$^)