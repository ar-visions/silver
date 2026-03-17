PROJECT := silver
SDK ?= native

MAKEFILE_REALPATH := $(abspath $(lastword $(MAKEFILE_LIST)))
SILVER := $(patsubst %/,%,$(dir $(shell readlink -f $(lastword $(MAKEFILE_LIST)))))

PROJECT_PATH := $(CURDIR)
PROJECT_NAME := $(notdir $(PROJECT_PATH))

BUILD_ROOT ?= $(SILVER)/platform/$(SDK)/release

export PROJECT_PATH
export PROJECT_NAME

DOCKER_IMAGE = silver-platform-$(SDK)
BLUE = awk '{printf "\033[34m%s\033[0m\n", $$0}'
DOCKER_RUN = docker run --rm \
	-v $(PROJECT_PATH):/project \
	-v $(SILVER)/platform/$(SDK):/silver/platform/native \
	-w /project \
	$(DOCKER_IMAGE)

.PHONY: all bootstrap build clean debug release docker-build docker-image

all: build

# ---- native vs docker dispatch ----

ifneq (,$(filter native ios,$(SDK)))

debug:
	$(MAKE) BUILD_ROOT=$(SILVER)/platform/$(SDK)/debug build

release:
	$(MAKE) BUILD_ROOT=$(SILVER)/platform/$(SDK)/release build

bootstrap:
ifeq ($(OS),Windows_NT)
	@case "$(BUILD_ROOT)" in *debug) \
		"$(SILVER)/bootstrap.bat" --debug "$(SDK)";; \
	*) \
		"$(SILVER)/bootstrap.bat" "$(SDK)";; \
	esac
else
	@case "$(BUILD_ROOT)" in *debug) \
		"$(SILVER)/bootstrap.sh" --debug "$(SDK)";; \
	*) \
		"$(SILVER)/bootstrap.sh" "$(SDK)";; \
	esac
endif

build: bootstrap
	echo "ninja -j8 -v -C $(BUILD_ROOT) -f $(PROJECT_NAME).ninja"
	ninja -j8 -v -C $(BUILD_ROOT) -f $(PROJECT_NAME).ninja

else
# ---- SDK != native: build through docker ----

debug: docker-image
	@$(DOCKER_RUN) make SDK=native BUILD_ROOT=/silver/platform/native/debug build 2>&1 | $(BLUE)

release: docker-image
	@$(DOCKER_RUN) make SDK=native build 2>&1 | $(BLUE)

build: docker-image
	@$(DOCKER_RUN) make SDK=native build 2>&1 | $(BLUE)

docker-image:
	@if ! docker image inspect $(DOCKER_IMAGE) >/dev/null 2>&1; then \
		docker build -t $(DOCKER_IMAGE) -f platform/$(SDK)/Dockerfile $(SILVER) 2>&1 | $(BLUE); \
	fi

endif

# ---- clean (always local) ----
clean:
ifeq ($(OS),Windows_NT)
	@if exist $(SILVER)\platform\native\debug rmdir /S /Q $(SILVER)\platform\native\debug\.headers_generated
	@if exist $(SILVER)\platform\native\release rmdir /S /Q $(SILVER)\platform\native\release\.headers_generated
else
	@rm -rf $(SILVER)/platform/native/debug/.headers_generated
	@rm -rf $(SILVER)/platform/native/release/.headers_generated
	@if [ -f "$(BUILD_ROOT)/$(PROJECT_NAME).ninja" ]; then \
		ninja -j8 -v -C $(BUILD_ROOT) -f $(PROJECT_NAME).ninja clean; \
	fi
endif
