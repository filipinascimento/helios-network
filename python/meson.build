project('helios-network-python', 'c', version : '0.10.3', default_options: ['c_std=c17'])

cc = meson.get_compiler('c')

extra_options = [
  '-D_POSIX_C_SOURCE=200809',
  '-D_GNU_SOURCE',
  '-DHTS_DISABLE_BGZF_THREADS',
  '-DZSTD_DISABLE_ASM',
]

host_system = host_machine.system()
if host_system == 'darwin'
  extra_options += ['-D_DARWIN_C_SOURCE']
elif host_system == 'windows'
  if cc.get_argument_syntax() == 'msvc'
    extra_options += ['/DWIN32', '/D__WIN32__']
  else
    extra_options += ['-DWIN32', '-D__WIN32__']
  endif
else
  extra_options += ['-DLinux']
endif

add_project_arguments(extra_options, language: 'c')

py = import('python').find_installation(required: true, pure: false)

include_dirs = include_directories(
  '../src/native/include/helios',
  '../src/native/third_party',
  '../src/native/libraries/htslib',
  '../src/native/libraries/zstd',
  '../src/native/libraries/zstd/common',
  '../src/native/libraries/zstd/decompress',
)

zlib_dep = dependency('zlib')
native_deps = [zlib_dep]
if host_system == 'windows'
  native_deps += cc.find_library('ws2_32', required: true)
endif

cvnetwork_src = files(
  '../src/native/src/CXDictionary.c',
  '../src/native/src/CXDistribution.c',
  '../src/native/src/CXIndexManager.c',
  '../src/native/src/CXNeighborStorage.c',
  '../src/native/src/CXLeiden.c',
  '../src/native/src/CXNetwork.c',
  '../src/native/src/CXNetworkGenerators.c',
  '../src/native/src/CXNetworkMeasurement.c',
  '../src/native/src/CXNetworkQuery.c',
  '../src/native/src/CXNetworkBXNet.c',
  '../src/native/src/CXNetworkGT.c',
  '../src/native/src/CXNetworkInterchange.c',
  '../src/native/src/CXNetworkXNet.c',
  '../src/native/src/CXSortTest.c',
  '../src/native/src/CXSet.c',
  '../src/native/src/CXSimpleQueue.c',
  '../src/native/src/CXZstd.c',
  '../src/native/src/fib/fib.c',
  '../src/native/libraries/htslib/bgzf.c',
  '../src/native/libraries/htslib/hfile.c',
  '../src/native/libraries/htslib/textutils.c',
  '../src/native/libraries/htslib/kstring.c',
  '../src/native/libraries/htslib/kfunc.c',
  '../src/native/libraries/htslib/thread_pool.c',
  '../src/native/libraries/htslib/cram/pooled_alloc.c',
  '../src/native/libraries/htslib/hts_shim.c',
  '../src/native/libraries/zstd/common/debug.c',
  '../src/native/libraries/zstd/common/entropy_common.c',
  '../src/native/libraries/zstd/common/error_private.c',
  '../src/native/libraries/zstd/common/fse_decompress.c',
  '../src/native/libraries/zstd/common/xxhash.c',
  '../src/native/libraries/zstd/common/zstd_common.c',
  '../src/native/libraries/zstd/decompress/huf_decompress.c',
  '../src/native/libraries/zstd/decompress/zstd_ddict.c',
  '../src/native/libraries/zstd/decompress/zstd_decompress.c',
  '../src/native/libraries/zstd/decompress/zstd_decompress_block.c',
)

py.extension_module(
  '_core',
  sources: cvnetwork_src + files('src/helios_network/_core.c'),
  include_directories: include_dirs,
  dependencies: native_deps,
  install: true,
  subdir: 'helios_network'
)

py.install_sources('src/helios_network/__init__.py', subdir: 'helios_network')
py.install_sources('src/helios_network/_conversions.py', subdir: 'helios_network')
py.install_sources('src/helios_network/_node_link_json.py', subdir: 'helios_network')
py.install_sources('src/helios_network/_wrapper.py', subdir: 'helios_network')
py.install_sources('src/helios_network/umap.py', subdir: 'helios_network')
