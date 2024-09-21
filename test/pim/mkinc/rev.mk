#
# rev.mk
#
# Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
# All Rights Reserved
# contact@tactcomplabs.com
#
# See LICENSE in the top level directory for licensing details
#

# PIM Type (0:none, 1:test, 2:reserved, 3:tclpim)
PIM_TYPE?=3

# REV paths
REVLIBPATH ?= $(PIM_REV_HOME)/build/src
REVPRINT ?= $(PIM_REV_HOME)/scripts/rev-print.py

# Test source code
SRCDIR = ./rev-test-src

# SST Libs
SSTOPTS += --add-lib-path=$(REVLIBPATH)

# Output Directories
OUTDIR = rev-output

# Test Selection
PIM_TESTS += $(notdir $(basename $(wildcard $(SRCDIR)/*.cc)))

# PIM MPI tests
# PIM_MPI_TESTS += 

ifndef MPI_RANKS
  ALL_TESTS = $(PIM_TESTS)
else
  ALL_TESTS = $(PIM_MPI_TESTS)
endif

TLIST ?= $(ALL_TESTS)

# REV Executables
SRCS   := $(basename $(notdir $(wildcard $(SRCDIR)/*.cc)))
EXES   := $(addprefix $(OUTDIR)/bin/,$(addsuffix .exe,$(SRCS)))
DIASMS := $(patsubst %.exe,%.dis,$(EXES))
SECTIONS := $(patsubst %.exe,%.sections,$(EXES))

# Targets
LOGS    := $(addsuffix /run.log,$(addprefix $(OUTDIR)/,$(TLIST)))
REVLOGS := $(addsuffix /run.revlog,$(addprefix $(OUTDIR)/,$(TLIST)))
TARGS   := $(LOGS) $(EXES) $(DIASMS) $(SECTIONS) $(REVLOGS)

# Recipes
all: $(TARGS)
run: $(LOGS)
compile: $(EXES) $(DIASMS)

# Rev toolchain
CC=riscv64-unknown-elf-g++
LD=riscv64-unknown-elf-ld
RVOBJDUMP=riscv64-unknown-elf-objdump
OBJDUMP   = ${RVOBJDUMP} --source -l -dC -Mno-aliases

# Rev headers
INCLUDE  := -I$(PIM_REV_HOME)/common/syscalls
INCLUDE  += -I$(PIM_REV_HOME)/test/include

# PIM headers
INCLUDE  += -I$(PROJHOME)/sstcomp/include

ifdef DEBUG
 CCOPT       ?= -O0 -g -DDEBUG=1
 DEBUG_MODE   = 1
else
 CCOPT       ?= -O2 -g
endif

ifdef DEBUG_MODE
 CCOPT += -DDEBUG_MODE=1
endif

# Configuration
WFLAGS    := -Wall -Wextra -Wuninitialized -pedantic-errors
WFLAGS    += -Wno-unused-function -Wno-unused-parameter
#WFLAGS    += -Werror
# DEBUG_FLAGS := -DDEBUG_MODE
FLAGS     := $(CCOPT) $(WFLAGS) $(DEBUG_FLAGS) -static -lm -fpermissive
ARCH      := rv64g
# ABI       := -mabi=lp64d

# Test Specific Customization
# if REV_EXE is not specified in OPTS it will default to the test name exe file
# $(OUTDIR)/appTest1/run.log: OPTS += ARGS M

# The magical run command
%.log: $(SSTCFG) compile
	@mkdir -p $(@D)
	@$(eval app = $(notdir $(@D)))
	$(eval exe = $(shell if [ -z ${REV_EXE} ]; then echo $(OUTDIR)/bin/$(app).exe; else echo ${REV_EXE}; fi))
	@$(eval statf = $(basename $@).status)
	@$(eval dotfile = $(basename $@).dot)
	@$(eval pdffile = $(basename $@).pdf)
	@rm -f $(statf) $(dotfile) $(pdffile)
	@echo Running $(basename $@)
	$(OPTS) PIM_TYPE=$(PIM_TYPE) OUTPUT_DIRECTORY=$(@D) ARCH=$(ARCH)_zicntr REV_EXE=$(exe) \
 $(MPIOPTS) $(SST) $(SSTOPTS) \
 --output-json=$(@D)/rank.json $(SSTCFG) \
  > $@ && (echo "pass" > $(statf); $(DOT2PDF))

# To run all even if they fail use this instead
#  > $@ && (echo "pass" > $(statf); $(DOT2PDF)) || echo "fail" > $(statf)
# 	@echo "### " $@":" `cat $(statf)`

%.revlog: %.log
	@ $(REVPRINT) -l $< > $@

%.dis: %.exe
	$(OBJDUMP) $< > $@

%.sections: %.exe
	${RVOBJDUMP} -s $< > $@

# NO_LINK = 1
ifndef NO_LINK
$(OUTDIR)/bin/%.exe: $(OUTDIR)/bin/%.o $(SRCDIR)/pim.lds
	$(LD) -o $@ -T $(SRCDIR)/pim.lds $<

$(OUTDIR)/bin/%.o: $(SRCDIR)/%.cc
	@mkdir -p $(@D)
	$(CC) $(FLAGS) -o $@ -g -c $< -march=$(ARCH) $(ABI) $(INCLUDE) $(LIBS)
else
$(OUTDIR)/bin/%.exe: $(SRCDIR)/%.cc
	@mkdir -p $(@D)
	$(CC) $(FLAGS) -g -march=$(ARCH) $(ABI) $(INCLUDE) -o $@ $<
endif


clean:
	rm -rf $(OUTDIR)

help:
	@echo make compile
	@echo make run
	@echo make TLIST="test1 test2 ..."
	@echo Valid TLIST selections are:
	@echo $(ALL_TESTS)
	@echo

.PHONY: run clean help

.SECONDARY:

.PRECIOUS: %.log %.revlog

#-- EOF
