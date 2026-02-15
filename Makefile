## @file
## @copyright 2026 Terry Golubiewski, all rights reserved

PROJDIR := $(abspath .)
SWDEV   := $(PROJDIR)/SwDev
include $(SWDEV)/project.mk

SUBDIRS := ext src

.ONESHELL:
SHELL := bash
.SHELLFLAGS := -euo pipefail -c

.PHONY: all clean scour $(SUBDIRS)

all: $(SUBDIRS)

src: ext

$(SUBDIRS):
	$(MAKE) -C "$@"

clean:
	$(foreach d,$(SUBDIRS),$(MAKE) -C "$(d)" clean;)

scour:
	$(foreach d,$(SUBDIRS),$(MAKE) -C "$(d)" scour;)

