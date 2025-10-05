PROJECT := silver
CONFIG ?= release

.PHONY: all bootstrap build clean debug release

all: build

debug:
	$(MAKE) CONFIG=debug build

release:
	$(MAKE) CONFIG=release build

bootstrap:
ifeq ($(OS),Windows_NT)
	@if [ "$(CONFIG)" = "debug" ]; then \
		./bootstrap.bat --debug; \
	else \
		./bootstrap.bat; \
	fi
else
	@if [ "$(CONFIG)" = "debug" ]; then \
		./bootstrap.sh --debug; \
	else \
		./bootstrap.sh; \
	fi
endif

build: bootstrap
	ninja -v -C $(CONFIG)

clean:
ifeq ($(OS),Windows_NT)
	@if exist debug rmdir /S /Q debug
	@if exist release rmdir /S /Q release
else
	@rm -rf debug
	@rm -rf release
endif
