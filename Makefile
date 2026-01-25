.PHONY: main compile clean_compile clean native native-static native-shared native-clean native-test test-native release check-clean sync-version version commit tag push-release

VERSION ?=
TAG ?= v$(VERSION)
MSG ?= Release $(TAG)
TAG_MSG ?= Release $(TAG)
MODE ?= release   # or debug

CC ?= cc
AR ?= ar
RANLIB ?= ranlib

NATIVE_SRC := $(wildcard src/native/src/*.c) $(wildcard src/native/src/fib/*.c)
HTSLIB_SRC := \
	src/native/libraries/htslib/bgzf.c \
	src/native/libraries/htslib/hfile.c \
	src/native/libraries/htslib/textutils.c \
	src/native/libraries/htslib/kstring.c \
	src/native/libraries/htslib/kfunc.c \
	src/native/libraries/htslib/thread_pool.c \
	src/native/libraries/htslib/cram/pooled_alloc.c \
	src/native/libraries/htslib/hts_shim.c
NATIVE_SRC += $(HTSLIB_SRC)
INCLUDE_DIR := src/native/include/helios
NATIVE_BUILD_DIR := build/native
NATIVE_OBJ := $(patsubst src/native/%.c,$(NATIVE_BUILD_DIR)/%.o,$(NATIVE_SRC))

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
SHARED_EXT := dylib
SHARED_LDFLAGS := -dynamiclib -current_version 1.0 -compatibility_version 1.0
else
SHARED_EXT := so
SHARED_LDFLAGS := -shared
endif


ifeq ($(MODE),debug)
  # print that debug mode is enabled
  $(info Building in debug mode with debug symbols)
  EMCC_OPT_FLAGS := -O0 -gsource-map --profiling
  EMCC_DEBUG_FLAGS := \
      -s ASSERTIONS=2 \
      -s STACK_OVERFLOW_CHECK=2 \
      -s SAFE_HEAP=1 \
      --source-map-base=compiled/ \
      --emit-symbol-map
else
  EMCC_OPT_FLAGS := -O3
  EMCC_DEBUG_FLAGS :=
endif

NATIVE_CFLAGS := -std=c17 -O3 -Wall -Wextra -pedantic -DNDEBUG -fPIC \
	-Isrc/native/include -Isrc/native/include/helios -Isrc/native/libraries/htslib
LIBS := -lz

PYTHON ?= python3
EXPORTED_FUNCS := [$(shell $(PYTHON) scripts/exported-functions.py --format make)]
EMCC_FLAGS := \
    $(EMCC_OPT_FLAGS) \
	--std=c17 \
	-Wall \
	-Isrc/native/include/helios \
	-Isrc/native/libraries/htslib \
	-DHTS_DISABLE_BGZF_THREADS \
	-D_POSIX_C_SOURCE=200809 \
	-s EXPORT_ES6=1 \
	-s MODULARIZE=1 \
	-s ALLOW_MEMORY_GROWTH=1 \
	-s EXPORT_NAME=HeliosNetwork \
	-s EXPORTED_FUNCTIONS="$(EXPORTED_FUNCS)" \
	-s EXPORTED_RUNTIME_METHODS='["cwrap","ccall","getValue","setValue","UTF8ToString","stringToUTF8","lengthBytesUTF8","HEAP8","HEAPU8","HEAP32","HEAPU32","HEAPF32","HEAPF64","FS"]' \
	-s ASSERTIONS=1 \
	-s USE_ZLIB=1 \
	-s MAXIMUM_MEMORY=4gb \
    $(EMCC_DEBUG_FLAGS)

main:
	npm run build

compile: clean_compile
	mkdir -p compiled
	emcc $(NATIVE_SRC) -o compiled/CXNetwork.mjs $(EMCC_FLAGS)

native: native-static native-shared

native-static: $(NATIVE_BUILD_DIR)/libhelios.a

native-shared: $(NATIVE_BUILD_DIR)/libhelios.$(SHARED_EXT)

$(NATIVE_BUILD_DIR)/%.o: src/native/%.c
	@mkdir -p $(dir $@)
	$(CC) $(NATIVE_CFLAGS) -c $< -o $@

$(NATIVE_BUILD_DIR)/libhelios.a: $(NATIVE_OBJ)
	@mkdir -p $(dir $@)
	rm -f $@
	$(AR) rcs $@ $^
	$(RANLIB) $@

$(NATIVE_BUILD_DIR)/libhelios.$(SHARED_EXT): $(NATIVE_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(SHARED_LDFLAGS) -o $@ $^ $(LIBS)

clean_compile:
	rm -rf compiled

native-clean:
	rm -rf $(NATIVE_BUILD_DIR)

native-test: tests/native/test_sort.c
	$(CC) -std=c17 -Isrc/native/include -Isrc/native/include/helios $< -o /tmp/helios_test_sort
	/tmp/helios_test_sort

test-native: native-test

clean:
	rm -rf compiled
	rm -rf dist
	rm -rf $(NATIVE_BUILD_DIR)

release: check-clean sync-version commit tag

check-clean:
	@if ! git diff --quiet --cached || ! git diff --quiet; then \
		echo 'Working tree must be clean before running release. Commit or stash changes first.'; \
		git status --short; \
		exit 1; \
	fi

sync-version:
	@if [ -z "$(VERSION)" ]; then \
		echo 'Set VERSION=X.Y.Z (e.g. make release VERSION=0.2.1)'; \
		exit 1; \
	fi
	npm run sync-version -- --version $(VERSION)

version:
	node sync-version.cjs

commit:
	@if [ -z "$(VERSION)" ]; then \
		echo 'Set VERSION=X.Y.Z (e.g. make release VERSION=0.2.1)'; \
		exit 1; \
	fi
	@git add package.json package-lock.json meson.build CMakeLists.txt src/native/include/helios/CXNetwork.h packaging/vcpkg/helios-network/vcpkg.json packaging/conan/conanfile.py
	@if git diff --cached --quiet; then \
		echo 'No version changes staged; did sync-version run?'; \
		exit 1; \
	fi
	@git commit -m "$(MSG)"

tag:
	@if [ -z "$(VERSION)" ]; then \
		echo 'Set VERSION=X.Y.Z (e.g. make release VERSION=0.2.1)'; \
		exit 1; \
	fi
	@git tag -a $(TAG) -m "$(TAG_MSG)"

push-release:
	@if [ -z "$(VERSION)" ]; then \
		echo 'Set VERSION=X.Y.Z (e.g. make release VERSION=0.2.1)'; \
		exit 1; \
	fi
	git push
	git push origin $(TAG)
