PROJECT := silver

MAKEFILE_REALPATH := $(abspath $(lastword $(MAKEFILE_LIST)))
SILVER := $(patsubst %/,%,$(dir $(shell readlink -f $(lastword $(MAKEFILE_LIST)))))

PROJECT_PATH := $(CURDIR)
PROJECT_NAME := $(notdir $(PROJECT_PATH))

BUILD_ROOT ?= $(SILVER)/install/debug

export PROJECT_PATH
export PROJECT_NAME

.PHONY: all bootstrap build clean debug release asan

all: build

debug:
	$(MAKE) BUILD_ROOT=$(SILVER)/install/debug build

release:
	$(MAKE) BUILD_ROOT=$(SILVER)/install/release build

asan:
	$(MAKE) BUILD_ROOT=$(SILVER)/install/debug ASAN=1 build

bootstrap:
ifeq ($(OS),Windows_NT)
	@case "$(BUILD_ROOT)" in *debug) \
		"$(SILVER)/bootstrap.bat" --debug $(if $(ASAN),--asan);; \
	*) \
		"$(SILVER)/bootstrap.bat" --release;; \
	esac
else
	@case "$(BUILD_ROOT)" in *debug) \
		"$(SILVER)/bootstrap.sh" --debug $(if $(ASAN),--asan);; \
	*) \
		"$(SILVER)/bootstrap.sh" --release;; \
	esac
endif

build: bootstrap
	echo "ninja -j8 -v -C $(BUILD_ROOT) -f $(PROJECT_NAME).ninja"
	ninja -j8 -v -C $(BUILD_ROOT) -f $(PROJECT_NAME).ninja

# ---- clean ----
clean:
ifeq ($(OS),Windows_NT)
	@if exist $(SILVER)\install\debug rmdir /S /Q $(SILVER)\install\debug\.headers_generated
	@if exist $(SILVER)\install\release rmdir /S /Q $(SILVER)\install\release\.headers_generated
else
	@rm -rf $(SILVER)/install/debug/.headers_generated
	@rm -rf $(SILVER)/install/release/.headers_generated
	@if [ -d "$(SILVER)/install/debug" ]; then rm -f $(SILVER)/install/debug/*.o; fi
	@if [ -d "$(SILVER)/install/release" ]; then rm -f $(SILVER)/install/release/*.o; fi
endif
