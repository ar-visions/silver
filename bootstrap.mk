PROJECT := silver
SDK ?= native

MAKEFILE_REALPATH := $(abspath $(lastword $(MAKEFILE_LIST)))
SILVER := $(patsubst %/,%,$(dir $(shell readlink -f $(lastword $(MAKEFILE_LIST)))))

PROJECT_PATH := $(CURDIR)
PROJECT_NAME := $(notdir $(PROJECT_PATH))

BUILD_ROOT ?= $(SILVER)/platform/$(SDK)/release

export PROJECT_PATH
export PROJECT_NAME

.PHONY: all bootstrap build clean debug release asan

all: build

debug:
	$(MAKE) BUILD_ROOT=$(SILVER)/platform/$(SDK)/debug build

release:
	$(MAKE) BUILD_ROOT=$(SILVER)/platform/$(SDK)/release build

asan:
	$(MAKE) BUILD_ROOT=$(SILVER)/platform/$(SDK)/debug ASAN=--asan build

bootstrap:
ifeq ($(OS),Windows_NT)
	@case "$(BUILD_ROOT)" in *debug) \
		"$(SILVER)/bootstrap.bat" --debug $(ASAN) "$(SDK)";; \
	*) \
		"$(SILVER)/bootstrap.bat" $(ASAN) "$(SDK)";; \
	esac
else
	@case "$(BUILD_ROOT)" in *debug) \
		"$(SILVER)/bootstrap.sh" --debug $(ASAN) "$(SDK)";; \
	*) \
		"$(SILVER)/bootstrap.sh" $(ASAN) "$(SDK)";; \
	esac
endif

build: bootstrap
	echo "ninja -j8 -v -C $(BUILD_ROOT) -f $(PROJECT_NAME).ninja"
	ninja -j8 -v -C $(BUILD_ROOT) -f $(PROJECT_NAME).ninja

clean:
ifeq ($(OS),Windows_NT)
	@if exist $(SILVER)\platform\native\debug rmdir /S /Q $(SILVER)\platform\native\debug\.headers_generated
	@if exist $(SILVER)\platform\native\release rmdir /S /Q $(SILVER)\platform\native\release\.headers_generated
else
	@rm -rf $(SILVER)/platform/native/debug/.headers_generated
	@rm -rf $(SILVER)/platform/native/release/.headers_generated
	@if [ -f "$(BUILD_ROOT)\$(PROJECT_NAME).ninja" ]; then \
		@ninja -j8 -v -C $(BUILD_ROOT) -f $(PROJECT_NAME).ninja clean; \
	fi
endif
