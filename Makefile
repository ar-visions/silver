PROJECT := silver

# use tapestry build system through make
ifeq ($(strip $(TAPESTRY)),)
ifeq ($(wildcard ../tapestry),)
$(info required: tapestry build delegate (invoked by Makefile) -- cloning to ../tapestry)
$(shell git clone https://github.com/ar-visions/tapestry ../tapestry)
$(error fetched tapestry -- please re-run make)
else
TAPESTRY := ../tapestry
endif
endif

include $(TAPESTRY)/bootstrap.mk
