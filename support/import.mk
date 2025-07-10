CC = gcc
PROJECT ?= silver-import

# Detect project root path (i.e., where Makefile is located)
PROJECT_PATH := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
BUILD_PATH := $(PROJECT_PATH)/debug
IMPORTS ?= A ether

# run through dependencies, getting their -l flags as a result
DEPS := $(shell for script in ./deps/*.sh; do [ -x "$script" ] && "$script"; done | tr '\n' ' ')

CFLAGS := $(CFLAGS) -I. -I./include -I../A/lib -fPIC \
    -Wno-incompatible-pointer-types -Wfatal-errors \
    -std=gnu11 -DMODULE="\"$(PROJECT)\""

# Platform-specific settings
ifeq ($(shell uname -s),Darwin)
    LIB_PRE = lib
    LIB_EXT = dylib
    APP_LDFLAGS := -Wl,-rpath,@executable_path/../lib
    LIB_LDFLAGS := -Wl,-install_name,@rpath/$(LIB_PRE)import.$(LIB_EXT)
else
    LIB_PRE = lib
    LIB_EXT = so
    APP_LDFLAGS := -Wl,-rpath,'$ORIGIN/../lib'
    LIB_LDFLAGS := -Wl,-soname,$(LIB_PRE)import.$(LIB_EXT)
endif

SHARED_OBJS = import.o A.o
SHARED_LIB = lib/$(LIB_PRE)import.$(LIB_EXT)
APP_OBJS = silver-import.o
TARGET = bin/silver-import

# Debug configuration based on DBG environment variable
DBG := $(strip $(DBG))
PROJECT := $(strip $(PROJECT))
DBG_PAD := ,$(DBG),
PROJECT_PAD := ,$(PROJECT),
EXCLUDE_PAD := ,-$(PROJECT),

DEBUG := false
ifneq ($(findstring $(EXCLUDE_PAD),$(DBG_PAD)),)
    DEBUG := false
else ifneq ($(findstring $(PROJECT_PAD),$(DBG_PAD)),)
    DEBUG := true
else ifneq ($(findstring *,$(DBG_PAD)),)
    DEBUG := true
endif

ifeq ($(DEBUG),true)
    CFLAGS := $(CFLAGS) -g2 -O0
else
    CFLAGS := $(CFLAGS) -O2
endif

# Targets
.PHONY: all headers clean install clean-all

all: headers $(SHARED_LIB) $(TARGET)

headers:
	@for D in src app; do \
		PROJECT_PATH="$(PROJECT_PATH)" \
		BUILD_PATH="$(BUILD_PATH)" \
		DIRECTIVE="$$D" \
		PROJECT="$(PROJECT)" \
		IMPORTS="$(IMPORTS)" \
		sh ./support/headers.sh; \
	done

import.o: src/import.c
	$(CC) $(CFLAGS) -c $< -o $@

$(SHARED_LIB): $(SHARED_OBJS)
	@mkdir -p lib
	$(CC) -shared -o $@ $^ $(LIB_LDFLAGS) $(DEPS)

$(TARGET): $(APP_OBJS)
	@mkdir -p bin
	$(CC) -o $@ $^ -Llib -limport $(APP_LDFLAGS) $(DEPS)

A.o: ../A/lib/A.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_PATH)

# Clean everything including LLVM build
clean-all: clean
	rm -rf build/ install/

install:
	@bash ./support/install.sh

# Show current configuration
info:
	@echo "project: $(PROJECT)"
	@echo "debug:   $(DEBUG)"
	@echo "cflags:  $(CFLAGS)"
	@echo "lflags:  $(APP_LDFLAGS)"
	@echo "deps:    $(DEPS)"
	@echo ""
	@./deps.sh info

help:
	@echo "silver Makefile"
	@echo ""
	@echo "targets:"
	@echo "  all        - build everything (dependencies auto-built if needed)"
	@echo "  clean      - clean build artifacts"
	@echo "  clean-all  - clean everything including deps"
	@echo "  info       - show current configuration"
	@echo "  help       - show this help"
	@echo ""
	@echo "environment variables:"
	@echo "  DBG        - debug configuration ( e.g: *,-array,-of,-projects  or  only,two )"