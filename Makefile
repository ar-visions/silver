PROJECT := silver

MAKEFILE_REALPATH := $(abspath $(lastword $(MAKEFILE_LIST)))
SILVER := $(patsubst %/,%,$(dir $(shell readlink -f $(lastword $(MAKEFILE_LIST)))))

PROJECT_PATH := $(CURDIR)
PROJECT_NAME := $(notdir $(PROJECT_PATH))

BUILD_ROOT ?= $(SILVER)/install/debug

# the vendored ninja — invoked by absolute path so the build never depends on ninja
# being on PATH (no env-var/PATH requirement to build silver).
NINJA := $(SILVER)/install/bin/ninja

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
	echo "$(NINJA) -j8 -v -C $(BUILD_ROOT) -f $(PROJECT_NAME).ninja"
	$(NINJA) -j8 -v -C $(BUILD_ROOT) -f $(PROJECT_NAME).ninja

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
