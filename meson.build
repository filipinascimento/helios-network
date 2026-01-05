project('helios-network', 'c',
  version : '0.4',
  default_options : ['c_std=c17']
)

add_project_arguments(
  '-D_POSIX_C_SOURCE=200809',
  '-D_GNU_SOURCE',
  language : 'c'
)

cvnetwork_src = files(
  'src/native/src/CXDictionary.c',
  'src/native/src/CXDistribution.c',
  'src/native/src/CXIndexManager.c',
  'src/native/src/CXNeighborStorage.c',
  'src/native/src/CXNetwork.c',
  'src/native/src/CXNetworkBXNet.c',
  'src/native/src/CXNetworkXNet.c',
  'src/native/src/CXSet.c',
  'src/native/src/CXSimpleQueue.c',
  'src/native/src/fib/fib.c',
  'src/native/libraries/htslib/bgzf.c',
  'src/native/libraries/htslib/hfile.c',
  'src/native/libraries/htslib/textutils.c',
  'src/native/libraries/htslib/kstring.c',
  'src/native/libraries/htslib/kfunc.c',
  'src/native/libraries/htslib/thread_pool.c',
  'src/native/libraries/htslib/cram/pooled_alloc.c',
  'src/native/libraries/htslib/hts_shim.c',
)

include_dirs = include_directories('src/native/include/helios', 'src/native/libraries/htslib')

if host_machine.system() == 'emscripten'
  zlib_dep = declare_dependency()
else
  zlib_dep = dependency('zlib')
endif

exported_functions = [
  '_malloc','_free','_calloc',
  '_CXNetworkVersionString',
  '_CXNewNetwork','_CXNewNetworkWithCapacity','_CXFreeNetwork',
  '_CXNetworkAddNodes','_CXNetworkRemoveNodes','_CXNetworkAddEdges','_CXNetworkRemoveEdges',
  '_CXNetworkNodeCount','_CXNetworkEdgeCount','_CXNetworkNodeCapacity','_CXNetworkEdgeCapacity',
  '_CXNetworkNodeFreeListCount','_CXNetworkNodeFreeListCapacity','_CXNetworkEdgeFreeListCount','_CXNetworkEdgeFreeListCapacity',
  '_CXNetworkNodeActivityBuffer','_CXNetworkEdgeActivityBuffer','_CXNetworkEdgesBuffer',
  '_CXNetworkWriteActiveNodes','_CXNetworkWriteActiveEdges','_CXNetworkWriteActiveEdgeSegments','_CXNetworkWriteActiveEdgeNodeAttributes','_CXNetworkWriteEdgeNodeAttributesInOrder','_CXNetworkCopyNodeAttributesToEdgeAttributes',
  '_CXNetworkAddDenseNodeAttribute','_CXNetworkAddDenseEdgeAttribute','_CXNetworkRemoveDenseNodeAttribute','_CXNetworkRemoveDenseEdgeAttribute',
  '_CXNetworkMarkDenseNodeAttributeDirty','_CXNetworkMarkDenseEdgeAttributeDirty',
  '_CXNetworkRemoveNodeAttribute','_CXNetworkRemoveEdgeAttribute','_CXNetworkRemoveNetworkAttribute',
  '_CXNetworkUpdateDenseNodeAttribute','_CXNetworkUpdateDenseEdgeAttribute','_CXNetworkUpdateDenseNodeIndexBuffer','_CXNetworkUpdateDenseEdgeIndexBuffer',
  '_CXNetworkDefineDenseColorEncodedNodeAttribute','_CXNetworkDefineDenseColorEncodedEdgeAttribute',
  '_CXNetworkRemoveDenseColorEncodedNodeAttribute','_CXNetworkRemoveDenseColorEncodedEdgeAttribute',
  '_CXNetworkMarkDenseColorEncodedNodeAttributeDirty','_CXNetworkMarkDenseColorEncodedEdgeAttributeDirty',
  '_CXNetworkUpdateDenseColorEncodedNodeAttribute','_CXNetworkUpdateDenseColorEncodedEdgeAttribute',
  '_CXNetworkSetDenseNodeOrder','_CXNetworkSetDenseEdgeOrder',
  '_CXNetworkGetNodeValidRange','_CXNetworkGetEdgeValidRange',
  '_CXNetworkIsDirected',
  '_CXNetworkOutNeighbors','_CXNetworkInNeighbors',
  '_CXNetworkIsNodeActive','_CXNetworkIsEdgeActive',
  '_CXNetworkDefineNodeAttribute','_CXNetworkDefineEdgeAttribute','_CXNetworkDefineNetworkAttribute',
  '_CXNetworkGetNodeAttribute','_CXNetworkGetEdgeAttribute','_CXNetworkGetNetworkAttribute',
  '_CXNetworkGetNodeAttributeBuffer','_CXNetworkGetEdgeAttributeBuffer','_CXNetworkGetNetworkAttributeBuffer',
  '_CXAttributeStride',
  '_CXDenseAttributeBufferVersion','_CXDenseAttributeBufferSourceVersion','_CXAttributeVersion',
  '_CXNetworkNodeTopologyVersion','_CXNetworkEdgeTopologyVersion',
  '_CXNetworkBumpNodeAttributeVersion','_CXNetworkBumpEdgeAttributeVersion','_CXNetworkBumpNetworkAttributeVersion',
  '_CXNodeSelectorCreate','_CXNodeSelectorDestroy','_CXNodeSelectorFillAll','_CXNodeSelectorFillFromArray','_CXNodeSelectorData','_CXNodeSelectorCount',
  '_CXEdgeSelectorCreate','_CXEdgeSelectorDestroy','_CXEdgeSelectorFillAll','_CXEdgeSelectorFillFromArray','_CXEdgeSelectorData','_CXEdgeSelectorCount',
  '_CXNeighborContainerCount','_CXNeighborContainerGetNodes','_CXNeighborContainerGetEdges',
  '_CXNetworkWriteBXNet','_CXNetworkWriteZXNet','_CXNetworkWriteXNet',
  '_CXNetworkReadBXNet','_CXNetworkReadZXNet','_CXNetworkReadXNet',
  '_CXNetworkCompact'
]

emcc_args = [
  '-O3',
  '-Wall',
  '-Isrc/native/include/helios',
  '-Isrc/native/libraries/htslib',
  '-D_POSIX_C_SOURCE=200809',
  '-DHTS_DISABLE_BGZF_THREADS',
  '-s', 'EXPORT_ES6=1',
  '-s', 'MODULARIZE=1',
  '-s', 'ALLOW_MEMORY_GROWTH=1',
  '-s', 'EXPORT_NAME=HeliosNetwork',
  '-s', 'EXPORTED_FUNCTIONS=[' + ','.join(exported_functions) + ']',
  '-s', 'EXPORTED_RUNTIME_METHODS=["cwrap","ccall","getValue","setValue","UTF8ToString","stringToUTF8","lengthBytesUTF8","HEAP8","HEAPU8","HEAP32","HEAPU32","HEAPF32","HEAPF64","FS"]',
  '-s', 'USE_ZLIB=1',
  '-s', 'ASSERTIONS=1',
  '-s', 'MAXIMUM_MEMORY=4gb',
]

cxnetwork_js = executable(
  'CXNetwork',
  cvnetwork_src,
  include_directories: include_dirs,
  dependencies: [zlib_dep],
  override_options: ['c_std=c17'],
  install: false,
  link_args: ['-Wl,--whole-archive'] + emcc_args,
)

compiled_dir = meson.current_source_dir() / 'compiled'
py = import('python').find_installation(required : true)
copy_script = [py.path(), '-c',
  'import os, shutil, sys; src, dst_dir, name = sys.argv[1:4]; os.makedirs(dst_dir, exist_ok=True); shutil.copy(src, os.path.join(dst_dir, name))'
]

custom_target(
  'copy_mjs',
  input: cxnetwork_js,
  output: 'CXNetwork.mjs',
  command: copy_script + ['@INPUT@', compiled_dir, '@OUTPUT@'],
  build_by_default: true,
)

custom_target(
  'copy_wasm',
  output: 'CXNetwork.wasm',
  command: copy_script + ['CXNetwork.wasm', compiled_dir, 'CXNetwork.wasm'],
  depends: cxnetwork_js,
  build_by_default: true,
)
