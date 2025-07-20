.SUFFIXES:
MAKEFLAGS += -rR

CC = clang
CXX = clang++
PROJECT ?= silver-import
# Detect project root path (i.e., where Makefile is located)
PROJECT_PATH := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
BUILD_PATH := $(PROJECT_PATH)/debug

ifeq ($(strip $(IMPORT)),)
$(error IMPORT is not set. steady Dak)
endif

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

# run through dependencies, getting their -l flags as a result
# DEPS := $(shell for script in ./deps/*.sh; do [ -x "$$script" ] && "$$script"; done | tr '\n' ' ')
DEPS := $(shell for f in ./deps/*.sh; do sh $$f; done)

CFLAGS ?= -Wno-writable-strings -fPIC \
	-fmacro-backtrace-limit=0 \
	-Wno-write-strings \
	-Wno-compare-distinct-pointer-types \
	-Wno-deprecated-declarations \
	-Wno-incompatible-pointer-types \
	-Wno-shift-op-parentheses \
	-Wfatal-errors \
	-Wno-incompatible-library-redeclaration \
	-fvisibility=default

# we need separate CFLAGS for app/src use-cases
# App gets its own include path
APP_CFLAGS := $(CFLAGS) -I$(PROJECT_PATH)/src -I$(BUILD_PATH)/app -I$(BUILD_PATH) -I$(IMPORT)/include -fPIC \
 -Wno-incompatible-pointer-types -Wfatal-errors \
 -std=gnu11 -DMODULE="\"$(PROJECT)\""
APP_CXXFLAGS := $(CFLAGS) $(CXXFLAGS) -I$(PROJECT_PATH)/src -I$(BUILD_PATH)/app -I$(BUILD_PATH) -I$(IMPORT)/include -fPIC \
 -Wfatal-errors \
 -std=c++17 -DMODULE="\"$(PROJECT)\""

# Base CFLAGS for src modules (will be extended per module)
BASE_CFLAGS := $(CFLAGS) -I$(PROJECT_PATH)/src -I$(BUILD_PATH) -I$(IMPORT)/include -fPIC \
 -Wno-incompatible-pointer-types -Wno-compare-distinct-pointer-types -Wno-pointer-type-mismatch -Wfatal-errors \
 -std=gnu11 -DMODULE="\"$(PROJECT)\""
BASE_CXXFLAGS := $(CFLAGS) $(CXXFLAGS) -I$(PROJECT_PATH)/src -I$(BUILD_PATH) -I$(IMPORT)/include -fPIC \
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

APP_LDFLAGS := $(APP_LDFLAGS) -L$(IMPORT)/lib
LIB_LDFLAGS := $(LIB_LDFLAGS) -L$(IMPORT)/lib

# Find all source files dynamically (both .c and .cc)
SRC_C_FILES := $(wildcard src/*.c)
SRC_CC_FILES := $(wildcard src/*.cc)
APP_C_FILES := $(wildcard app/*.c)
APP_CC_FILES := $(wildcard app/*.cc)

# Generate object file paths in debug directories (each module gets its own dir)
SRC_C_OBJS := $(patsubst src/%.c,$(BUILD_PATH)/src/%.o,$(SRC_C_FILES))
SRC_CC_OBJS := $(patsubst src/%.cc,$(BUILD_PATH)/src/%.o,$(SRC_CC_FILES))
SRC_OBJS := $(SRC_C_OBJS) $(SRC_CC_OBJS)

APP_C_OBJS := $(patsubst app/%.c,$(BUILD_PATH)/app/%.o,$(APP_C_FILES))
APP_CC_OBJS := $(patsubst app/%.cc,$(BUILD_PATH)/app/%.o,$(APP_CC_FILES))
APP_OBJS := $(APP_C_OBJS) $(APP_CC_OBJS)

# Generate list of target executables (one per app/*.c or app/*.cc file)
TARGETS_C := $(patsubst app/%.c,bin/%,$(APP_C_FILES))
TARGETS_CC := $(patsubst app/%.cc,bin/%,$(APP_CC_FILES))
TARGETS := $(TARGETS_C) $(TARGETS_CC)

SHARED_LIB = lib/$(LIB_PRE)$(PROJECT).$(LIB_EXT)

# Targets
.PHONY: all headers clean install clean-all

all: headers $(SHARED_LIB) $(TARGETS)

headers:
	@for x in src app; do \
	 PROJECT_PATH="$(PROJECT_PATH)" \
	 BUILD_PATH="$(BUILD_PATH)" \
	 DIRECTIVE="$$x" \
	 PROJECT="$(PROJECT)" \
	 sh ./support/headers.sh; \
	 done

# Create debug directories for each module
$(BUILD_PATH)/app/%.o: app/%.c
	@mkdir -p $(BUILD_PATH)/app
	$(CC) -I$(BUILD_PATH)/app/$* -I$(BUILD_PATH)/src $(BASE_CFLAGS) -c $< -o $@

# Pattern rule for app/*.cc -> debug/app/module-name/module-name.o
$(BUILD_PATH)/app/%.o: app/%.cc
	@mkdir -p $(BUILD_PATH)/app
	$(CXX) -I$(BUILD_PATH)/app/$* -I$(BUILD_PATH)/src $(BASE_CXXFLAGS) -c $< -o $@

# Pattern rule for src/*.c -> debug/src/module-name/module-name.o
# Each module gets its own include directory
$(BUILD_PATH)/src/%.o: src/%.c src/%
	@mkdir -p $(BUILD_PATH)/src
	$(CC) -I$(BUILD_PATH)/src/$* -I$(BUILD_PATH)/src $(BASE_CFLAGS) -c $< -o $@

# if there is no corresponding ext-less header, the above rule is not applied
# and this one will be
$(BUILD_PATH)/src/%.o: src/%.c
	@mkdir -p $(BUILD_PATH)/src
	$(CC) -I$(BUILD_PATH)/src/$* -I$(BUILD_PATH)/src $(BASE_CFLAGS) -c $< -o $@

# Pattern rule for src/*.cc -> debug/src/module-name/module-name.o
$(BUILD_PATH)/src/%.o: src/%.cc
	@mkdir -p $(BUILD_PATH)/src
	$(CXX) -I$(BUILD_PATH)/src/$* -I$(BUILD_PATH)/src $(BASE_CXXFLAGS) -c $< -o $@


# we also need an App for each App Obj

$(SHARED_LIB): $(SRC_OBJS)
	@mkdir -p lib
	$(CC) -shared -o $@ $^ $(LIB_LDFLAGS) $(DEPS)

# Pattern rule to build each target executable from its corresponding app object
# Uses CXX if any C++ objects are present in the library, otherwise CC
bin/%: $(BUILD_PATH)/app/%.o $(SHARED_LIB)
	@mkdir -p bin
	$(if $(SRC_CC_OBJS),$(CXX),$(CC)) -o $@ $< $(APP_LDFLAGS) -l$(PROJECT)

clean:
	rm -rf $(BUILD_PATH)

# Clean everything including LLVM build
clean-all: clean
	rm -rf build/ install/

install:
	@bash ./support/install.sh