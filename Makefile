PROJECT := silver

# Get directory of this Makefile
MAKEFILE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

# Set IMPORT if not already set
ifeq ($(origin IMPORT), undefined)
    IMPORT := $(MAKEFILE_DIR)
else ifneq ($(realpath $(IMPORT)), $(realpath $(MAKEFILE_DIR)))
    $(error IMPORT ($(IMPORT)) does not match this Makefile's directory ($(MAKEFILE_DIR)))
endif

include $(IMPORT)/import.mk
