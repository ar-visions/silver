PROJECT := silver
CONFIG ?= release
SDK ?= native

.PHONY: all bootstrap build clean debug release

all: build

debug:
	$(MAKE) CONFIG=sdk/$(SDK)/debug build

release:
	$(MAKE) CONFIG=sdk/$(SDK)/release build

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
	ninja -j8 -v -C $(CONFIG)

clean:
ifeq ($(OS),Windows_NT)
	@if exist sdk\native\debug rmdir /S /Q sdk\native\debug\.headers_generated
	@if exist sdk\native\release rmdir /S /Q sdk\native\release\.headers_generated
else
	@rm -rf /sdk/native/debug/.headers_generated
	@rm -rf /sdk/native/debug/.headers_generated
endif
