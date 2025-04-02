PROJECT := silver

# use silver base build system
ifeq ($(strip $(SILVER_IMPORT)),)
ifeq ($(wildcard ../silver),)
$(info required: silver base system ... cloning to ../silver)
$(shell git clone https://github.com/ar-visions/silver ../silver)
$(error fetched silver -- please re-run make)
else
$(error SILVER_IMPORT not set)
endif
endif

include $(SILVER_IMPORT)/../base.mk
