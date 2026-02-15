## @file
## @copyright 2025 Terry Golubiewski, all rights reserved.

MAKEFLAGS += --warn-undefined-variables

PROJNAME := InsetXml
VERSION  := 0.0.0
#DEBUG ?= 1
USE_DYNAMIC_RTL := false

ifndef PROJDIR
$(error PROJDIR is not defined)
endif

COMPILER ?= gcc
PLATFORM := Posix

ifndef BOOST
export BOOST:=/usr/include
export BOOSTLIB:=/usr/lib
endif

ifndef BOOSTLIB
export BOOSTLIB:=$(BOOST)/lib
endif

ifdef DEBUG
DBGSFX := D
else
DBGSFX :=
endif

PATH_OBJ := obj_$(COMPILER)$(DBGSFX)
CLEAN := obj_* *.stackdump *.exe.manifest
