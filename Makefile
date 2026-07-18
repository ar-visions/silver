PROJECT := silver

MAKEFILE_REALPATH := $(abspath $(lastword $(MAKEFILE_LIST)))
SILVER := $(patsubst %/,%,$(dir $(shell readlink -f $(lastword $(MAKEFILE_LIST)))))

PROJECT_PATH := $(CURDIR)
PROJECT_NAME := $(notdir $(PROJECT_PATH))

BUILD_ROOT ?= $(SILVER)/install/build

# the vendored ninja — invoked by absolute path so the build never depends on ninja
# being on PATH (no env-var/PATH requirement to build silver).
NINJA := $(SILVER)/install/bin/ninja

export PROJECT_PATH
export PROJECT_NAME

.PHONY: all bootstrap build clean debug release asan

all: release

debug:
	$(MAKE) BUILD_ROOT=$(SILVER)/install/build CONFIG=debug build

release:
	$(MAKE) BUILD_ROOT=$(SILVER)/install/build CONFIG=release build

asan:
	$(MAKE) BUILD_ROOT=$(SILVER)/install/build CONFIG=debug ASAN=1 build

# put `silver` on the user's PATH without any env-var/profile edits: symlink it
# into the first writable directory already on PATH (no sudo). last `make
# install` wins. the binary self-locates its libs/tree from its own path, so no
# IMPORT is needed.
install: build
	@bin_dir="$$HOME/.local/bin"; \
	mkdir -p "$$bin_dir"; \
	ln -sf "$(BUILD_ROOT)/silver" "$$bin_dir/silver"; \
	echo "linked $$bin_dir/silver -> $(BUILD_ROOT)/silver"; \
	ln -sf "$(SILVER)/dbg" "$$bin_dir/dbg"; \
	echo "linked $$bin_dir/dbg -> $(SILVER)/dbg"; \
	case ":$$PATH:" in *":$$bin_dir:"*) ;; *) echo "note: add $$bin_dir to your PATH";; esac

CONFIG ?= debug

bootstrap:
ifeq ($(OS),Windows_NT)
	@"$(SILVER)/bootstrap.bat" --$(CONFIG) $(if $(ASAN),--asan)
else
	@"$(SILVER)/bootstrap.sh" --$(CONFIG) $(if $(ASAN),--asan)
endif

build: bootstrap
	@stamp="$(SILVER)/install/.active-config"; \
	if [ ! -f "$$stamp" ]; then \
		echo "$(CONFIG)" > "$$stamp"; \
	elif [ "$$(cat "$$stamp")" != "$(CONFIG)" ]; then \
		echo "config switch: $$(cat "$$stamp") -> $(CONFIG) (ninja rehashes the cores)"; \
		echo "$(CONFIG)" > "$$stamp"; \
	fi
	echo "$(NINJA) -j8 -v -C $(BUILD_ROOT) -f $(PROJECT_NAME).ninja"
	$(NINJA) -j8 -v -C $(BUILD_ROOT) -f $(PROJECT_NAME).ninja
	@ln -sfn "$(BUILD_ROOT)/silver" "$(SILVER)/install/bin/silver"; \
	echo "silver -> $(BUILD_ROOT)/silver (last built wins)"

# ---- clean ----
clean:
ifeq ($(OS),Windows_NT)
	@if exist $(SILVER)\install\debug rmdir /S /Q $(SILVER)\install\debug\.headers_generated
	@if exist $(SILVER)\install\release rmdir /S /Q $(SILVER)\install\release\.headers_generated
else
	@rm -rf $(SILVER)/install/build/.headers_generated
	@rm -f $(SILVER)/install/build/*.o
endif
