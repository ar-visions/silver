CC = gcc
CXX = g++
PROJECT ?= silver-import
# Detect project root path (i.e., where Makefile is located)
PROJECT_PATH := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
BUILD_PATH := $(PROJECT_PATH)/debug
IMPORTS ?= A ether
# run through dependencies, getting their -l flags as a result
DEPS := $(shell for script in ./deps/*.sh; do [ -x "$$script" ] && "$$script"; done | tr '\n' ' ')
# we need separate CFLAGS for app/src use-cases
# App gets its own include path
APP_CFLAGS := $(CFLAGS) -I$(PROJECT_PATH)/src -I$(BUILD_PATH)/app -I$(BUILD_PATH) -I. -I./include -fPIC \
 -Wno-incompatible-pointer-types -Wfatal-errors \
 -std=gnu11 -DMODULE="\"$(PROJECT)\""
APP_CXXFLAGS := $(CXXFLAGS) -I$(PROJECT_PATH)/src -I$(BUILD_PATH)/app -I$(BUILD_PATH) -I. -I./include -fPIC \
 -Wfatal-errors \
 -std=c++17 -DMODULE="\"$(PROJECT)\""

# Base CFLAGS for src modules (will be extended per module)
BASE_CFLAGS := $(CFLAGS) -I$(PROJECT_PATH)/src -I$(BUILD_PATH) -I. -I./include -fPIC \
 -Wno-incompatible-pointer-types -Wfatal-errors \
 -std=gnu11 -DMODULE="\"$(PROJECT)\""
BASE_CXXFLAGS := $(CXXFLAGS) -I$(PROJECT_PATH)/src -I$(BUILD_PATH) -I. -I./include -fPIC \
 -Wfatal-errors \
 -std=c++17 -DMODULE="\"$(PROJECT)\""
# Platform-specific settings
ifeq ($(shell uname -s),Darwin)
 LIB_PRE = lib
 LIB_EXT = dylib
 APP_LDFLAGS := -Wl,-rpath,@executable_path/../lib
 LIB_LDFLAGS := -Wl,-install_name,@rpath/$(LIB_PRE)$(PROJECT).$(LIB_EXT)
else
 LIB_PRE = lib
 LIB_EXT = so
 APP_LDFLAGS := -Wl,-rpath,'$$ORIGIN/../lib'
 LIB_LDFLAGS := -Wl,-soname,$(LIB_PRE)$(PROJECT).$(LIB_EXT)
endif

# Find all source files dynamically (both .c and .cc)
SRC_C_FILES := $(wildcard src/*.c)
SRC_CC_FILES := $(wildcard src/*.cc)
APP_C_FILES := $(wildcard app/*.c)
APP_CC_FILES := $(wildcard app/*.cc)

# Extract module names from source files
SRC_C_MODULES := $(patsubst src/%.c,%,$(SRC_C_FILES))
SRC_CC_MODULES := $(patsubst src/%.cc,%,$(SRC_CC_FILES))
ALL_SRC_MODULES := $(sort $(SRC_C_MODULES) $(SRC_CC_MODULES))

# Filter src files based on IMPORTS environment variable (default: all)
# This facilitates the "import silver [ import pong ]" syntax
IMPORTS ?= *
ifeq ($(IMPORTS),*)
 FILTERED_SRC_C_FILES := $(SRC_C_FILES)
 FILTERED_SRC_CC_FILES := $(SRC_CC_FILES)
 FILTERED_MODULES := $(ALL_SRC_MODULES)
else
 FILTERED_SRC_C_FILES := $(foreach src,$(IMPORTS),$(wildcard src/$(src).c))
 FILTERED_SRC_CC_FILES := $(foreach src,$(IMPORTS),$(wildcard src/$(src).cc))
 FILTERED_MODULES := $(IMPORTS)
endif

# Generate object file paths in debug directories (each module gets its own dir)
SRC_C_OBJS := $(patsubst src/%.c,$(BUILD_PATH)/src/%/%.o,$(FILTERED_SRC_C_FILES))
SRC_CC_OBJS := $(patsubst src/%.cc,$(BUILD_PATH)/src/%/%.o,$(FILTERED_SRC_CC_FILES))
SRC_OBJS := $(SRC_C_OBJS) $(SRC_CC_OBJS)

APP_C_OBJS := $(patsubst app/%.c,$(BUILD_PATH)/app/%.o,$(APP_C_FILES))
APP_CC_OBJS := $(patsubst app/%.cc,$(BUILD_PATH)/app/%.o,$(APP_CC_FILES))
APP_OBJS := $(APP_C_OBJS) $(APP_CC_OBJS)

# Generate list of target executables (one per app/*.c or app/*.cc file)
TARGETS_C := $(patsubst app/%.c,bin/%,$(APP_C_FILES))
TARGETS_CC := $(patsubst app/%.cc,bin/%,$(APP_CC_FILES))
TARGETS := $(TARGETS_C) $(TARGETS_CC)

SHARED_LIB = lib/$(LIB_PRE)$(PROJECT).$(LIB_EXT)

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

all: headers $(SHARED_LIB) $(TARGETS)

headers:
	@for x in src app; do \
	 PROJECT_PATH="$(PROJECT_PATH)" \
	 BUILD_PATH="$(BUILD_PATH)" \
	 DIRECTIVE="$$x" \
	 PROJECT="$(PROJECT)" \
	 IMPORTS="$(IMPORTS)" \
	 MODULES="$(FILTERED_MODULES)" \
	 sh ./support/headers.sh; \
	 done

# Create debug directories for each module
$(BUILD_PATH)/src/%:
	@mkdir -p $(BUILD_PATH)/src/$*

$(BUILD_PATH)/app:
	@mkdir -p $(BUILD_PATH)/app

# Pattern rule for src/*.c -> debug/src/module-name/module-name.o
# Each module gets its own include directory
$(BUILD_PATH)/src/%/%.o: src/%.c | $(BUILD_PATH)/src/%
	$(CC) -I$(BUILD_PATH)/src/$* -I$(BUILD_PATH)/src $(BASE_CFLAGS) -c $< -o $@

# Pattern rule for src/*.cc -> debug/src/module-name/module-name.o
$(BUILD_PATH)/src/%/%.o: src/%.cc | $(BUILD_PATH)/src/%
	$(CXX) -I$(BUILD_PATH)/src/$* -I$(BUILD_PATH)/src $(BASE_CXXFLAGS) -c $< -o $@

# Pattern rule for app/*.c -> debug/app/*.o
$(BUILD_PATH)/app/%.o: app/%.c | $(BUILD_PATH)/app
	$(CC) -I$(BUILD_PATH)/app/$* -I$(BUILD_PATH)/app $(APP_CFLAGS) -c $< -o $@

# Pattern rule for app/*.cc -> debug/app/*.o
$(BUILD_PATH)/app/%.o: app/%.cc | $(BUILD_PATH)/app
	$(CXX) -I$(BUILD_PATH)/app/$* -I$(BUILD_PATH)/app $(APP_CXXFLAGS) -c $< -o $@

$(SHARED_LIB): $(SRC_OBJS)
	@mkdir -p lib
	$(CC) -shared -o $@ $^ $(LIB_LDFLAGS) $(DEPS)

# Pattern rule to build each target executable from its corresponding app object
# Uses CXX if any C++ objects are present in the library, otherwise CC
bin/%: $(BUILD_PATH)/app/%.o $(SHARED_LIB)
	@mkdir -p bin
	$(if $(SRC_CC_OBJS),$(CXX),$(CC)) -o $@ $< -Llib -l$(PROJECT) $(APP_LDFLAGS) $(DEPS)

clean:
	rm -rf $(BUILD_PATH)

# Clean everything including LLVM build
clean-all: clean
	rm -rf build/ install/

install:
	@bash ./support/install.sh