MAKEFILE_PATH := $(abspath $(word $(shell expr $(words $(MAKEFILE_LIST)) - 1),$(MAKEFILE_LIST)))
ifeq ($(MAKEFILE_PATH),)
    MAKEFILE_PATH := $(abspath $(lastword $(MAKEFILE_LIST)))
endif

SRC_ROOT           := $(patsubst %/,%,$(dir $(MAKEFILE_PATH)))
MAKEFILE_PATH_ROOT := $(abspath $(lastword $(MAKEFILE_LIST)))
IMPORT             ?= $(patsubst %/,%,$(dir $(MAKEFILE_PATH_ROOT)))
IMPORT_FILE        := $(SRC_ROOT)/build.sf

ifeq (,$(findstring $(PROJECT),$(DBG)))
BUILD_DIR := $(SRC_ROOT)/release
else
BUILD_DIR := $(SRC_ROOT)/debug
endif

IMPORT_BIN := $(IMPORT)/bin/silver-import
DBG     := $(DBG)

.PHONY: all import
all: $(IMPORT_BIN)
	IMPORT=$(IMPORT) DBG=$(DBG) $(IMPORT_BIN) $(IMPORT_FILE)

$(IMPORT_BIN):
	@$(MAKE) -C $(IMPORT)

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)