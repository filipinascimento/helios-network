.PHONY: main compile clean_compile clean native native-static native-shared native-clean release check-clean sync-version version commit tag push-release

VERSION ?=
TAG ?= v$(VERSION)
MSG ?= Release $(TAG)
TAG_MSG ?= Release $(TAG)

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

NATIVE_CFLAGS := -std=c17 -O3 -Wall -Wextra -pedantic -DNDEBUG -fPIC \
	-Isrc/native/include -Isrc/native/include/helios -Isrc/native/libraries/htslib
LIBS := -lz

EXPORTED_FUNCS := [_malloc,_free,_calloc,_CXNetworkVersionString,_CXNewNetwork,_CXNewNetworkWithCapacity,_CXFreeNetwork,_CXNetworkAddNodes,_CXNetworkRemoveNodes,_CXNetworkAddEdges,_CXNetworkRemoveEdges,_CXNetworkNodeCount,_CXNetworkEdgeCount,_CXNetworkNodeCapacity,_CXNetworkEdgeCapacity,_CXNetworkNodeActivityBuffer,_CXNetworkEdgeActivityBuffer,_CXNetworkEdgesBuffer,_CXNetworkWriteActiveNodes,_CXNetworkWriteActiveEdges,_CXNetworkWriteActiveEdgeSegments,_CXNetworkWriteActiveEdgeNodeAttributes,_CXNetworkWriteEdgeNodeAttributesInOrder,_CXNetworkAddDenseNodeAttribute,_CXNetworkAddDenseEdgeAttribute,_CXNetworkRemoveDenseNodeAttribute,_CXNetworkRemoveDenseEdgeAttribute,_CXNetworkMarkDenseNodeAttributeDirty,_CXNetworkMarkDenseEdgeAttributeDirty,_CXNetworkUpdateDenseNodeAttribute,_CXNetworkUpdateDenseEdgeAttribute,_CXNetworkUpdateDenseNodeIndexBuffer,_CXNetworkUpdateDenseEdgeIndexBuffer,_CXNetworkSetDenseNodeOrder,_CXNetworkSetDenseEdgeOrder,_CXNetworkGetNodeValidRange,_CXNetworkGetEdgeValidRange,_CXNetworkIsDirected,_CXNetworkOutNeighbors,_CXNetworkInNeighbors,_CXNetworkIsNodeActive,_CXNetworkIsEdgeActive,_CXNetworkDefineNodeAttribute,_CXNetworkDefineEdgeAttribute,_CXNetworkDefineNetworkAttribute,_CXNetworkGetNodeAttribute,_CXNetworkGetEdgeAttribute,_CXNetworkGetNetworkAttribute,_CXNetworkGetNodeAttributeBuffer,_CXNetworkGetEdgeAttributeBuffer,_CXNetworkGetNetworkAttributeBuffer,_CXAttributeStride,_CXNodeSelectorCreate,_CXNodeSelectorDestroy,_CXNodeSelectorFillAll,_CXNodeSelectorFillFromArray,_CXNodeSelectorData,_CXNodeSelectorCount,_CXEdgeSelectorCreate,_CXEdgeSelectorDestroy,_CXEdgeSelectorFillAll,_CXEdgeSelectorFillFromArray,_CXEdgeSelectorData,_CXEdgeSelectorCount,_CXNeighborContainerCount,_CXNeighborContainerGetNodes,_CXNeighborContainerGetEdges,_CXNetworkWriteBXNet,_CXNetworkWriteZXNet,_CXNetworkWriteXNet,_CXNetworkReadBXNet,_CXNetworkReadZXNet,_CXNetworkReadXNet,_CXNetworkCompact]
EMCC_FLAGS := \
	-O3 \
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
	-s MAXIMUM_MEMORY=4gb

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
