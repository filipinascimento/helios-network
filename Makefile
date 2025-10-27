NATIVE_SRC := $(wildcard src/native/src/*.c) $(wildcard src/native/src/fib/*.c)
INCLUDE_DIR := src/native/include/helios
EXPORTED_FUNCS := [_malloc,_free,_calloc,_CXNewNetwork,_CXNewNetworkWithCapacity,_CXFreeNetwork,_CXNetworkAddNodes,_CXNetworkRemoveNodes,_CXNetworkAddEdges,_CXNetworkRemoveEdges,_CXNetworkNodeCount,_CXNetworkEdgeCount,_CXNetworkNodeCapacity,_CXNetworkEdgeCapacity,_CXNetworkNodeActivityBuffer,_CXNetworkEdgeActivityBuffer,_CXNetworkEdgesBuffer,_CXNetworkOutNeighbors,_CXNetworkInNeighbors,_CXNetworkIsNodeActive,_CXNetworkIsEdgeActive,_CXNetworkDefineNodeAttribute,_CXNetworkDefineEdgeAttribute,_CXNetworkDefineNetworkAttribute,_CXNetworkGetNodeAttribute,_CXNetworkGetEdgeAttribute,_CXNetworkGetNetworkAttribute,_CXNetworkGetNodeAttributeBuffer,_CXNetworkGetEdgeAttributeBuffer,_CXNetworkGetNetworkAttributeBuffer,_CXAttributeStride,_CXNodeSelectorCreate,_CXNodeSelectorDestroy,_CXNodeSelectorFillAll,_CXNodeSelectorFillFromArray,_CXNodeSelectorData,_CXNodeSelectorCount,_CXEdgeSelectorCreate,_CXEdgeSelectorDestroy,_CXEdgeSelectorFillAll,_CXEdgeSelectorFillFromArray,_CXEdgeSelectorData,_CXEdgeSelectorCount,_CXNeighborContainerCount,_CXNeighborContainerGetNodes,_CXNeighborContainerGetEdges]
EMCC_FLAGS := \
	-O3 \
	--std=c17 \
	-Wall \
	-Isrc/native/include/helios \
	-s EXPORT_ES6=1 \
	-s MODULARIZE=1 \
	-s ALLOW_MEMORY_GROWTH=1 \
	-s EXPORT_NAME=HeliosNetwork \
	-s EXPORTED_FUNCTIONS="$(EXPORTED_FUNCS)" \
	-s EXPORTED_RUNTIME_METHODS='["cwrap","ccall","getValue","setValue","UTF8ToString","stringToUTF8","lengthBytesUTF8","HEAP8","HEAPU8","HEAP32","HEAPU32","HEAPF64"]' \
	-s ASSERTIONS=1 \
	-s MAXIMUM_MEMORY=4gb

main:
	npm run build

compile: clean_compile
	mkdir -p compiled
	emcc $(NATIVE_SRC) -o compiled/CXNetwork.mjs $(EMCC_FLAGS)

clean_compile:
	rm -rf compiled

clean:
	rm -rf compiled
	rm -rf dist
