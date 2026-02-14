# @file
# @copyright 2025 Terry Golubiewski, all rights reserved.
# @author Terry Golubiewski

export PROJDIR := $(abspath ..)
SWDEV := $(PROJDIR)/SwDev
include $(SWDEV)/project.mk

# Must use "=" instead of ":=" because $E will be defined below.
INSET_XML_E=InsetXml.$E

TGT1=$(INSET_XML_E)
TARGETS=$(TGT1)

SRC1:=InsetXml.cpp FarmDb.cpp FarmXml.cpp FarmWkt.cpp FarmShp.cpp FarmZip.cpp
SRC1+=FarmGeo.cpp BoundarySwaths.cpp
SOURCE:=$(SRC1)

#SYSINCL:=$(XML)/src $(BOOST) $(addsuffix /include, $(UNITS)/src/core $(UNITS)/src/systems $(GSL))

#SYSINCL:=$(XML)/src $(BOOST) $(SHP) $(ZIP)/lib $(ZIP)/build $(addsuffix /include, $(UNITS)/src/core $(UNITS)/src/systems $(GSL))
SYSINCL:=$(PROJDIR)/ext/build/include

INCLUDE:=$(PROJDIR)

LDFLAGS+= -L$(PROJDIR)/ext/build/lib
LDLIBS+= -lboost_program_options
LDLIBS+= -lpugixml
LDLIBS+= -lshp
LDLIBS+= -lzip
LDLIBS+= -lz

#LDLIBS+= -ladvapi32 -lbcrypt
#CDEFS+= -DZIP_STATIC

#DEBUG=1

include $(SWDEV)/$(COMPILER).mk
include $(SWDEV)/build.mk

CFLAGS += -Wno-error=deprecated-declarations

.PHONY: all clean scour

all: depend $(TARGETS)

$(TGT1): $(OBJ1) $(LIBS)
	$(LINK)
