# @file
# @copyright 2025 Terry Golubiewski, all rights reserved.
# @author Terry Golubiewski

export PROJNAME:=ekman2
export PROJDIR := $(abspath ..)
# SWDEV := $(PROJDIR)/SwDev
include $(SWDEV)/project.mk

APP  :=$(abspath $(HOME)/App)
#BOOST:=$(APP)/boost
UNITS:=$(APP)/mp-units
GSL  :=$(APP)/gsl-lite
XML  :=$(APP)/pugixml

# Must use "=" instead of ":=" because $E will be defined below.
INSET_XML_E=InsetXml.$E

TGT1=$(INSET_XML_E)
TARGETS=$(TGT1)

SRC1:=InsetXml.cpp FarmDb.cpp FarmXml.cpp FarmGeo.cpp BoundarySwaths.cpp
SOURCE:=$(SRC1)

#SYSINCL:=$(XML)/src $(BOOST) $(addsuffix /include, $(UNITS)/src/core $(UNITS)/src/systems $(GSL))

SYSINCL:=$(XML)/src $(addsuffix /include, $(UNITS)/src/core $(UNITS)/src/systems $(GSL))
INCLUDE:=$(PROJDIR)

# Must use "=" because $A Will be defined below.
LIBS=$(XML)/build/libpugixml.$A

#DEBUG=1

SHELL:=/bin/bash
.SHELLFLAGS:=-eu -o pipefail -c
.ONESHELL:

include $(SWDEV)/$(COMPILER).mk
include $(SWDEV)/build.mk

LDLIBS=-lboost_program_options-mt

.PHONY: all clean scour

all: depend $(TARGETS)

$(TGT1): $(OBJ1) $(LIBS)
        $(LINK)


