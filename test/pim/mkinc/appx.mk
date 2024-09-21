#
# appx.mk
#
# Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
# All Rights Reserved
# contact@tactcomplabs.com
#
# See LICENSE in the top level directory for licensing details
#

# SST Libs
SSTOPTS += --add-lib-path=$(PROJHOME)/sstcomp/AppGen

# Output Directories
OUTDIR = appx-output

# Test Selection
PIM_TESTS += AppxTest

#TODO noncacheable regions for Miranda then remove this
export FORCE_NONCACHEABLE_REQS=1

# PIM MPI tests
# PIM_MPI_TESTS += 

ifndef MPI_RANKS
  ALL_TESTS = $(PIM_TESTS)
else
  ALL_TESTS = $(PIM_MPI_TESTS)
endif

TLIST ?= $(ALL_TESTS)

# Targets
LOGS   := $(addsuffix /run.log,$(addprefix $(OUTDIR)/,$(TLIST)))
TARGS  := $(LOGS)

# Recipes
all: $(TARGS)
run: $(LOGS)

# Test Specific Customization
$(OUTDIR)/AppxTest/run.log:             OPTS = APP=AppxTest PIM_TYPE=1

# The magical run command
.PHONY: %.log
%.log: $(SSTCFG)
	@mkdir -p $(@D)
	@$(eval app = $(notdir $(@D)))
	@$(eval statf = $(basename $@).status)
	@$(eval dotfile = $(basename $@).dot)
	@$(eval pdffile = $(basename $@).pdf)
	@rm -f $(statf) $(dotfile) $(pdffile)
	@echo Running $(basename $@)
	$(OPTS) OUTPUT_DIRECTORY=$(@D) \
 $(MPIOPTS) $(SST) $(SSTOPTS) \
 --output-json=$(@D)/rank.json $(SSTCFG) \
 > $@ && (echo "pass" > $(statf); $(DOT2PDF))

 # To run all even if they fail use this instead
 # > $@ && (echo "pass" > $(statf); $(DOT2PDF)) || echo "fail" > $(statf)
 #	@echo "### " $@":" `cat $(statf)`

clean:
	rm -rf $(OUTDIR)

help:
	@echo make run
	@echo make TLIST="test1 test2 ..."
	@echo Valid TLIST selections are:
	@echo $(ALL_TESTS)
	@echo

.PHONY: run clean help

.SECONDARY:

.PRECIOUS: %.log

#-- EOF
