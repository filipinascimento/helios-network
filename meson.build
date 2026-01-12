project('helios-network', 'c',
  version : '0.4.1',
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
  'src/native/src/CXLeiden.c',
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

py = import('python').find_installation(required : true)
exported_functions_output = run_command(py, files('scripts/exported-functions.py'), '--format', 'lines', check: true).stdout().strip()
exported_functions = exported_functions_output == '' ? [] : exported_functions_output.split('\n')

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
copy_script = [py.path(), '-c',
  'import os, shutil, sys; src, dst_dir, name = sys.argv[1:4]; os.makedirs(dst_dir, exist_ok=True); shutil.copy(src, os.path.join(dst_dir, name))'
]

custom_target(
  'copy_mjs',
  input: cxnetwork_js,
  output: 'CXNetwork.mjs',
  command: copy_script + ['@INPUT@', compiled_dir, '@OUTPUT@'],
  build_always_stale: true,
  build_by_default: true,
)

custom_target(
  'copy_wasm',
  output: 'CXNetwork.wasm',
  command: copy_script + ['CXNetwork.wasm', compiled_dir, 'CXNetwork.wasm'],
  depends: cxnetwork_js,
  build_always_stale: true,
  build_by_default: true,
)
