ifndef SILVER_IMPORT
$(error SILVER_IMPORT environment variable is not set)
endif

MAKEFILE_PATH := $(abspath $(word $(shell expr $(words $(MAKEFILE_LIST)) - 1),$(MAKEFILE_LIST)))
ifeq ($(MAKEFILE_PATH),)
    MAKEFILE_PATH := $(abspath $(lastword $(MAKEFILE_LIST)))
endif

UNAME 		   = $(shell uname)
ARCH          := $(shell uname -m)
SYS           := $(shell uname -s)
ifeq ($(SYS),Darwin)
    OS := darwin
else ifeq ($(SYS),Linux)
    OS := linux
else
    OS := windows
endif

AI_DICTATION  	   := rune-gen
SRC_ROOT  	  	   := $(patsubst %/,%,$(dir $(MAKEFILE_PATH)))
MAKEFILE_PATH_ROOT := $(abspath $(lastword $(MAKEFILE_LIST)))
SILVER 	           := $(patsubst %/,%,$(dir $(MAKEFILE_PATH_ROOT)))
SILVER_FILE	       := $(SRC_ROOT)/silver

ifeq (,$(findstring $(PROJECT),$(DBG)))
BUILD_DIR := $(SRC_ROOT)/build
else
BUILD_DIR := $(SRC_ROOT)/debug
endif

ifeq ($(UNAME), Darwin)
    SED = gsed
	NPROC = gnproc
else
    SED = sed
	NPROC = nproc
endif

# read the libraries from the target-group arg at $(1)
define IMPORT_script
	$(shell \
		printf "%s\n" "$$(cat $(SRC_ROOT)/silver)" | { \
		found=0; \
		found_arch=1; \
		found_os=1; \
		arch_list="x86_64 arm64"; \
		os_list="darwin windows linux"; \
		while IFS= read -r line; do \
			new_found=0; \
			if echo "$$line" | grep -q "^#"; then \
				continue; \
			fi; \
			for arch in $$arch_list; do \
				if echo "$$line" | grep -q "[[:space:]]*$$arch:"; then \
					if [ "$(ARCH)" = "$$arch" ]; then \
						found_arch=1; \
					else \
						found_arch=0; \
					fi; \
					continue 2; \
				fi; \
			done; \
			for os in $$os_list; do \
				if echo "$$line" | grep -q "[[:space:]]*$$os:"; then \
					if [ "$(OS)" = "$$os" ]; then \
						found_os=1; \
					else \
						found_os=0; \
					fi; \
					continue 2; \
				fi; \
			done; \
			if echo "$$line" | grep -q "^$(1):"; then \
				found=1; \
				new_found=1; \
				found_arch=1; \
				found_os=1; \
				line=$$(echo "$$line" | $(SED) -E 's/^$(1):[[:space:]]*(.*)/ \1/'); \
			elif echo "$$line" | grep -q "^[a-zA-Z][a-zA-Z0-9_]*:"; then \
				found=0; \
			fi; \
			if [ $$found -eq 1 ] && [ $$found_arch -eq 1 ] && [ $$found_os -eq 1 ]; then \
				if [ -n "$$line" ] && echo "$$line" | grep -q "^[[:space:]]"; then \
					for lib in $$line; do \
						echo "$$lib"; \
					done; \
				elif [ $$new_found -eq 0 ]; then \
					break; \
				fi; \
			fi; \
		done
	})
endef

ifeq ($(UNAME),Darwin)
FRAMEWORK_PATHS = /System/Library/Frameworks /Library/Frameworks
define is_framework
$(shell for p in $(FRAMEWORK_PATHS); do \
    if [ -d "$$p/$(1).framework" ]; then \
        echo "yes"; \
        break; \
    fi; \
done)
endef
endif

comma = ,
define process_libs
$(foreach dep,$(filter-out ,$(shell bash -c 'echo "$(call IMPORT_script,$(1))"')), \
	$(if $(filter -%,$(strip $(dep))),, \
		$(if $(findstring .so,$(dep)), \
			$(SILVER_IMPORT)/lib/$(strip $(dep)), \
			$(if $(and $(findstring Darwin,$(UNAME)), $(call is_framework,$(strip $(dep)))), \
				-framework $(strip $(dep)), \
				$(if $(filter @%,$(strip $(dep))), \
					-Wl$(comma)-Bstatic -l$(subst @,,$(dep)) -Wl$(comma)-Bdynamic -l$(subst @,,$(dep)), \
					-l$(dep)\
				)\
			)\
		)\
	)\
)
endef

define process_imports
$(foreach dep,$(filter-out ,$(shell bash -c 'echo "$(call IMPORT_script,$(1))"')), \
	$(if $(filter -%,$(strip $(dep))),, \
		$(if $(findstring .so,$(dep)), $(SILVER_IMPORT)/lib/$(strip $(dep)),$(strip $(dep)))
	)\
)
endef

define process_cflags
$(foreach dep,$(filter-out ,$(shell bash -c 'echo "$(call IMPORT_script,$(1))"')), \
	$(if $(filter -%,$(strip $(dep))),$(strip $(dep)),)\
)
endef

PREP_DIRS := $(shell \
	mkdir -p $(BUILD_DIR)/app $(BUILD_DIR)/test $(BUILD_DIR)/lib && \
	if [ -d "$(SRC_ROOT)/res" ]; then \
		rsync --checksum -r $(SRC_ROOT)/res/* $(BUILD_DIR); \
	fi && \
    if [ -d "$(SRC_ROOT)/lib" ]; then \
        find $(SRC_ROOT)/lib -type f -exec sh -c ' \
            src_file="{}"; \
            dest_file="$(BUILD_DIR)/lib/$$(basename {})"; \
			if [ ! -f "$$dest_file" ] || [ "$$src_file" -nt "$$dest_file" ]; then \
				echo "copying $$src_file -> $$dest_file"; \
				cp -p "$$src_file" "$$dest_file"; \
				$(SED) -i "1i\#line 1 \"$$src_file\"" "$$dest_file"; \
			fi \
        ' \; ; \
    fi && \
    if [ -d "$(SRC_ROOT)/app" ]; then \
        find $(SRC_ROOT)/app -type f -exec sh -c ' \
            src_file="{}"; \
            dest_file="$(BUILD_DIR)/app/$$(basename {})"; \
			if [ ! -f "$$dest_file" ] || [ "$$src_file" -nt "$$dest_file" ]; then \
				cp -p "$$src_file" "$$dest_file"; \
				$(SED) -i "1i\#line 1 \"$$src_file\"" "$$dest_file"; \
			fi \
        ' \; ; \
    fi && \
    if [ -d "$(SRC_ROOT)/test" ]; then \
        find $(SRC_ROOT)/test -type f -exec sh -c ' \
            src_file="{}"; \
            dest_file="$(BUILD_DIR)/test/$$(basename {})"; \
			if [ ! -f "$$dest_file" ] || [ "$$src_file" -nt "$$dest_file" ]; then \
				cp -p "$$src_file" "$$dest_file"; \
				$(SED) -i "1i\#line 1 \"$$src_file\"" "$$dest_file"; \
			fi \
        ' \; ; \
    fi \
)

APPS_LIBS          = $(call process_libs,app)
LIB_LIBS 	       = $(call process_libs,lib)
TEST_LIBS          = $(call process_libs,test)
LIB_CFLAGS         = $(call process_cflags,lib)
APPS_CFLAGS        = $(call process_cflags,app)
TEST_CFLAGS        = $(call process_cflags,test)
APPS_IMPORTS       = $(call process_imports,app)
LIB_IMPORTS        = $(call process_imports,lib)

LIB_IMPORTS       := $(subst $(newline), ,$(strip $(LIB_IMPORTS)))
APPS_IMPORTS      := $(subst $(newline), ,$(strip $(APPS_IMPORTS)))
IMPORTS            = $(LIB_IMPORTS) # todo: transition to one set of IMPORTS per build instance, apps will be another instance
ifeq ($(strip $(IMPORTS)),)
	IMPORTS = $(APPS_IMPORTS)
endif

release		      ?= 0
APP			      ?= 0
CC 			       = $(SILVER_IMPORT)/bin/clang
CXX			       = $(SILVER_IMPORT)/bin/clang++
MAKEFLAGS         += --no-print-directory
LIB_INCLUDES       = -I$(BUILD_DIR)/lib  -I$(SILVER_IMPORT)/include
APP_INCLUDES       = -I$(BUILD_DIR)/app  -I$(BUILD_DIR)/lib -I$(SILVER_IMPORT)/include
TEST_INCLUDES      = -I$(BUILD_DIR)/test -I$(BUILD_DIR)/lib -I$(SILVER_IMPORT)/include
# default behavior of -l cannot be set back once you do -Wl,-Bdynamic or -Wl,-Bstatic -- there is no -Wl,-Bdynamic-first
# to create sanity lets establish -Wl,-Bdynamic first
LDFLAGS 	       = -L$(BUILD_DIR) -L$(SILVER_IMPORT)/lib -Wl,-rpath,$(SILVER_IMPORT)/lib -Wl,-Bdynamic
SRCAPP_DIR 	       = $(BUILD_DIR)/app
SRCLIB_DIR 	       = $(BUILD_DIR)/lib
TEST_DIR           = $(BUILD_DIR)/test
TEST_SRCS 	       = $(wildcard $(TEST_DIR)/*.c)    $(wildcard $(TEST_DIR)/*.cc)
TEST_OBJS 		   = $(patsubst $(TEST_DIR)/%.c,    $(BUILD_DIR)/test/%.o, $(wildcard $(TEST_DIR)/*.c)) \
           			 $(patsubst $(TEST_DIR)/%.cc,   $(BUILD_DIR)/test/%.o, $(wildcard $(TEST_DIR)/*.cc))
LIB_SRCS 	       = $(wildcard $(SRCLIB_DIR)/*.c)  $(wildcard $(SRCLIB_DIR)/*.cc)
LIB_OBJS 		   = $(patsubst $(SRCLIB_DIR)/%.c,  $(BUILD_DIR)/lib/%.o, $(wildcard $(SRCLIB_DIR)/*.c)) \
           			 $(patsubst $(SRCLIB_DIR)/%.cc, $(BUILD_DIR)/lib/%.o, $(wildcard $(SRCLIB_DIR)/*.cc))
APP_SRCS 	       = $(wildcard $(SRCAPP_DIR)/*.c)  $(wildcard $(SRCAPP_DIR)/*.cc)
APP_OBJS 		   = $(patsubst $(SRCAPP_DIR)/%.c,  $(BUILD_DIR)/app/%.o, $(wildcard $(SRCAPP_DIR)/*.c)) \
           			 $(patsubst $(SRCAPP_DIR)/%.cc, $(BUILD_DIR)/app/%.o, $(wildcard $(SRCAPP_DIR)/*.cc))
#REFLECT_TARGET    = $(BUILD_DIR)/$(PROJECT)-reflect
LIB_TARGET   	   = $(if $(strip $(LIB_OBJS)),$(BUILD_DIR)/lib$(PROJECT).so)
APP_TARGETS	 	   = $(patsubst $(SRCAPP_DIR)/%.c, $(BUILD_DIR)/%, $(wildcard $(SRCAPP_DIR)/*.c)) \
					 $(patsubst $(SRCAPP_DIR)/%.cc, $(BUILD_DIR)/%, $(wildcard $(SRCAPP_DIR)/*.cc))
TARGET_FLAGS 	   = -shared
ALL_TARGETS  	   = run-build $(IMPORT_HEADER) $(LIB_TARGET) $(APP_TARGETS) $(BUILD_DIR)/$(PROJECT)-flat $(BUILD_DIR)/$(PROJECT)-includes tests
APP_FLAGS    	   = 
PROJECT_HEADER_R   = $(if $(wildcard $(BUILD_DIR)/lib/$(PROJECT)),lib/$(PROJECT),$(if $(wildcard $(BUILD_DIR)/app/$(PROJECT)),app/$(PROJECT),))
PROJECT_HEADER     = $(BUILD_DIR)/$(PROJECT_HEADER_R)
IMPORT_APP_HEADER  = $(BUILD_DIR)/app/import
IMPORT_LIB_HEADER  = $(BUILD_DIR)/lib/import

# test should use lib's import; we dont have a 'test' for an app because thats shell-level testing
IMPORT_TEST_HEADER = $(BUILD_DIR)/test/import
IMPORT_HEADER     := $(if $(wildcard $(SRC_ROOT)/lib),$(IMPORT_LIB_HEADER),$(IMPORT_APP_HEADER))

METHODS_HEADER 	   = $(BUILD_DIR)/$(PROJECT_HEADER_R)-methods
INIT_HEADER        = $(BUILD_DIR)/$(PROJECT_HEADER_R)-init
INTERN_HEADER      = $(BUILD_DIR)/$(PROJECT_HEADER_R)-intern
PUBLIC_HEADER      = $(BUILD_DIR)/$(PROJECT_HEADER_R)-public
upper 		       = $(shell echo $(1) | tr '[:lower:]' '[:upper:]')
LINKER 			   = $(if $(filter %.cc %.cpp,$(LIB_SRCS)), $(CXX), $(CC))
UPROJECT 	       = $(call upper,$(PROJECT))
CXXFLAGS 		   = $(if $(filter 1,$(release)),,-g) -fPIC -DMODULE="\"$(PROJECT)\"" -std=c++17 -Wno-writable-strings
CFLAGS 		   	   = $(if $(filter 1,$(release)),,-g) -fPIC -fno-exceptions \
	-D_GNU_SOURCE -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS \
	-Wno-write-strings -Wno-compare-distinct-pointer-types -Wno-deprecated-declarations \
	-Wno-incompatible-pointer-types -Wfatal-errors -std=gnu11 -DMODULE="\"$(PROJECT)\"" \
	-Wno-incompatible-library-redeclaration -fvisibility=default
ifeq ($(ASAN),1)
CFLAGS := $(CFLAGS) -fsanitize=address
LDFLAGS := $(LDFLAGS) -fsanitize=address
endif
SRC_TRANSLATION   := $(SRC_ROOT)/translation/A-translation.c
BUILD_TRANSLATION := $(BUILD_DIR)/A-translation

define newline


endef

ifneq ($(release),0)
	ALL_TARGETS += verify
endif

all: $(ALL_TARGETS)

$(IMPORT_HEADER): $(PROJECT_HEADER) $(SILVER_FILE) $(SILVER)/build.mk
	@echo "imports = $(IMPORTS)"
	@bash $(SILVER)/base/headers.sh $(IMPORT_HEADER) $(PROJECT_HEADER) $(PROJECT) $(UPROJECT) $(IMPORTS)

.PRECIOUS: *.o
.SUFFIXES:
.PHONY: all clean install run-import tests verify process_c_files clean $(IMPORT_HEADER) $(SILVER)/build.mk

$(IMPORT_HEADER): | run-import


define process_src
	@for file in $(SRC_ROOT)/$(1)/*.c; do \
		if [ -f "$$file" ]; then \
			echo "A-translation '$$file' '$(BUILD_DIR)/$(1)/$$(basename $$file)'"; \
			$(SILVER_IMPORT)/bin/A-translation "$$file" "$(BUILD_DIR)/$(1)/$$(basename $$file)"; \
		fi \
	done; \
	for file in $(SRC_ROOT)/$(1)/*.ai; do \
		if [ -f "$$file" ]; then \
			base_name=$$(basename "$$file" .ai); \
			if [[ $$base_name =~ ^.*\..*$$ ]]; then \
				target_ext=$${base_name##*.}; \
				target_base=$${base_name%.*}; \
				target_file="$(BUILD_DIR)/$(1)/$$target_base.$$target_ext"; \
				echo "Processing AI file '$$file' -> '$$target_file'"; \
				export LD_LIBRARY_PATH=$$SILVER_IMPORT; \
				$(BUILD_DIR)/A-generate -i "$$file" -o "$$target_file" || exit 1; \
				unset LD_LIBRARY_PATH; \
				if [ $$? -eq 0 ] && [ -f "$$target_file" ]; then \
					echo "Successfully generated $$target_file"; \
				else \
					echo "Error: Failed to process $$file"; \
					exit 1; \
				fi \
			fi \
		fi \
	done
endef

process_c_files:
	$(call process_src,lib)
	$(call process_src,app)
	$(call process_src,test)

# Fix Makefile behavior with this; otherwise its On Error Resume Next -- style functionality.  not exactly what most people want
.NOTPARALLEL:


define RUN_IMPORT_SCRIPT
	@echo "$(PROJECT): import"
	@BUILT_PROJECTS="$(BUILT_PROJECTS)" bash $(SILVER)/base/import.sh --b $(BUILD_DIR) --i $(SRC_ROOT)/silver || { echo "import script failed"; exit 1; }
endef

# Add this call to the import script before building
$(LIB_TARGET): | run-build
$(APP_TARGET): | run-build

run-build:
	$(RUN_IMPORT_SCRIPT)

$(BUILD_DIR)/$(PROJECT)-includes:
	@echo "#include <$(PROJECT)>" > $@.tmp.c
	@$(CC) $(CFLAGS) $(LIB_INCLUDES) $(APP_INCLUDES) -E -dD -C -P $@.tmp.c | \
		awk '/\/\/\/ start-trim/ {in_trim=1; next} /\/\/\/ end-trim/ {in_trim=0; next} in_trim {print}' > $@
	@rm $@.tmp.c

$(BUILD_DIR)/$(PROJECT)-flat:
	@echo "#include <$(PROJECT)>" > $@.tmp.c
	@$(CC) $(CFLAGS) $(LIB_INCLUDES) $(APP_INCLUDES) -E -dD -C -P $@.tmp.c | \
	awk '/\/\/\/ start-trim/ {print; in_trim=1; next} /\/\/\/ end-trim/ {in_trim=0; print; next} !in_trim {print}' > $@
	@rm $@.tmp.c

# compiling
# ---------------------------------------------
# lib (todo: allow -flags to be placed in import)
$(BUILD_DIR)/lib/%.o: $(BUILD_DIR)/lib/%.c $(PROJECT_HEADER)
	$(CC) $(CFLAGS) $(LIB_INCLUDES) $(LIB_CFLAGS) -c $< -o $@
$(BUILD_DIR)/lib/%.o: $(BUILD_DIR)/lib/%.cc $(PROJECT_HEADER)
	$(CXX) $(CXXFLAGS) $(LIB_INCLUDES) $(LIB_CFLAGS) -c $< -o $@

# app
$(BUILD_DIR)/app/%.o: $(BUILD_DIR)/app/%.c $(PROJECT_HEADER)
	$(CC) $(CFLAGS) $(APP_INCLUDES) -c $< -o $@
$(BUILD_DIR)/app/%.o: $(BUILD_DIR)/app/%.cc $(PROJECT_HEADER)
	$(CXX) $(CXXFLAGS) $(APP_INCLUDES) -c $< -o $@

# test
$(BUILD_DIR)/test/%.o: $(BUILD_DIR)/test/%.c $(PROJECT_HEADER)
	$(CC) $(CFLAGS) $(TEST_INCLUDES) -c $< -o $@
$(BUILD_DIR)/test/%.o: $(BUILD_DIR)/test/%.cc $(PROJECT_HEADER)
	$(CXX) $(CXXFLAGS) $(TEST_INCLUDES) -c $< -o $@

# linking
# ---------------------------------------------
ifneq ($(strip $(LIB_TARGET)),)
$(LIB_TARGET): $(LIB_OBJS)
	$(LINKER) $(TARGET_FLAGS) $(LDFLAGS) -o $@ $^ $(LIB_LIBS)
	@touch $(BUILD_DIR)/.built
	@echo "$(PROJECT): lib-built: $@"
endif

$(BUILD_DIR)/%: $(BUILD_DIR)/app/%.o $(LIB_TARGET)
	$(LINKER) $(LDFLAGS) -o $@ $^ $(LIB_TARGET) $(APPS_LIBS);
	@touch $(BUILD_DIR)/.built
	@echo "$(PROJECT): app-built: $@"

$(BUILD_DIR)/test/%: $(BUILD_DIR)/test/%.o $(LIB_TARGET)
	$(LINKER) $(LDFLAGS) -o $@ $^ $(LIB_TARGET) $(TEST_LIBS)
	@touch $(BUILD_DIR)/.built
	@echo "$(PROJECT): test-built: $@"

#ifneq ($(strip $(LIB_TARGET)),)
#$(REFLECT_TARGET): $(SRC_ROOT)/../A/meta/A-reflect.c $(LIB_TARGET)
#	$(CC) $(CFLAGS) $(INCLUDES) $< -o $@ -L$(BUILD_DIR)/lib $(LDFLAGS) -l$(PROJECT) $(LIB_LIBS)
#endif

TEST_TARGETS := $(patsubst $(TEST_DIR)/%.c,$(BUILD_DIR)/test/%,$(TEST_SRCS))

# build all tests individually
tests: $(TEST_TARGETS)

verify: $(TEST_TARGETS)
	@for test_exec in $(TEST_TARGETS); do \
		echo "test: $$test_exec..."; \
		$$test_exec; \
		if [ $$? -ne 0 ]; then \
			echo "$$test_exec failed"; \
			exit 1; \
		fi; \
	done
	@echo "all tests passed."


# override the default goal
.DEFAULT_GOAL := install

# install targets and run reflect with integrity check
install: all

	@if [ -f $(BUILD_DIR)/.built ]; then \
		echo "$(PROJECT): installing"; \
		mkdir -p $(SILVER_IMPORT)/lib; \
		mkdir -p $(SILVER_IMPORT)/include; \
		mkdir -p $(SILVER_IMPORT)/bin; \
		if [ -n "$(strip $(LIB_TARGET))" ]; then \
			if [ -f $(BUILD_DIR)/$(PROJECT)-includes ]; then \
				install -m 644 $(LIB_TARGET)  $(SILVER_IMPORT)/lib/; \
			fi \
		fi; \
		if [ -n "$(strip $(APP_TARGETS))" ]; then \
			install -m 755 $(APP_TARGETS) $(SILVER_IMPORT)/bin/; \
		fi; \
		if [ -f "$(SRCLIB_DIR)/$(PROJECT)" ]; then \
			install -m 644 $(SRCLIB_DIR)/$(PROJECT) $(SILVER_IMPORT)/include/; \
		fi; \
		if [ -f "$(METHODS_HEADER)" ]; then \
			install -m 644 $(METHODS_HEADER) $(SILVER_IMPORT)/include/; \
		fi; \
		if [ -f "$(INIT_HEADER)" ]; then \
			install -m 644 $(INIT_HEADER) $(SILVER_IMPORT)/include/; \
		fi; \
		if [ -f "$(PUBLIC_HEADER)" ]; then \
			install -m 644 $(PUBLIC_HEADER) $(SILVER_IMPORT)/include/; \
		fi; \
		if [ -f "$(INTERN_HEADER)" ]; then \
			install -m 644 $(INTERN_HEADER) $(SILVER_IMPORT)/include/; \
		fi; \
		if [ -f $(SRCLIB_DIR)/A-reserve ]; then \
			install -m 644 $(SRCLIB_DIR)/A-reserve $(SILVER_IMPORT)/include/; \
		fi; \
		if [ -f $(BUILD_DIR)/$(PROJECT)-includes ]; then \
			install -m 644 $(BUILD_DIR)/$(PROJECT)-includes $(SILVER_IMPORT)/include/; \
			install -m 644 $(BUILD_DIR)/$(PROJECT)-flat $(SILVER_IMPORT)/include/; \
		fi; \
		cd $(BUILD_DIR); # && ./$(PROJECT)-reflect || true \
		if [ -f $(BUILD_DIR)/lib$(PROJECT).m ]; then \
			install -m 644 $(BUILD_DIR)/lib$(PROJECT).m $(SILVER_IMPORT)/lib/; \
		fi; \
		rm -rf $(BUILD_DIR)/.built; \
	else \
		echo "$(PROJECT): no-change"; \
	fi

clean:
	rm -rf $(BUILD_DIR)/silver-token $(BUILD_DIR)/lib$(PROJECT).so $(BUILD_DIR)/$(PROJECT) $(BUILD_DIR)/app $(BUILD_DIR)/lib $(BUILD_DIR)/test $(METHODS_HEADER) \
		   $(INIT_HEADER) $(INTERN_HEADER) $(IMPORT_LIB_HEADER) $(IMPORT_APP_HEADER) $(IMPORT_TEST_HEADER) \
		   $(REFLECT_TARGET) $(BUILD_DIR)/*-flat $(BUILD_DIR)/*-includes $(BUILD_DIR)/*.tmp.c