cmake_minimum_required(VERSION 3.18)

project(HeliosNetwork LANGUAGES C VERSION 0.5.0 DESCRIPTION "Helios Network native graph core")

option(HELIOS_BUILD_SHARED "Build the Helios shared library" ON)
option(HELIOS_BUILD_STATIC "Build the Helios static library" ON)
option(HELIOS_BUILD_TESTS "Build native tests" ON)

if(NOT HELIOS_BUILD_SHARED AND NOT HELIOS_BUILD_STATIC)
  message(FATAL_ERROR "At least one of HELIOS_BUILD_SHARED or HELIOS_BUILD_STATIC must be enabled.")
endif()

find_package(ZLIB REQUIRED)

set(HELIOS_SOURCES
  src/native/src/CXDictionary.c
  src/native/src/CXDistribution.c
  src/native/src/CXIndexManager.c
  src/native/src/CXNeighborStorage.c
  src/native/src/CXLeiden.c
  src/native/src/CXNetwork.c
  src/native/src/CXNetworkBXNet.c
  src/native/src/CXNetworkXNet.c
  src/native/src/CXSortTest.c
  src/native/src/CXSet.c
  src/native/src/CXSimpleQueue.c
  src/native/src/fib/fib.c
  src/native/libraries/htslib/bgzf.c
  src/native/libraries/htslib/hfile.c
  src/native/libraries/htslib/textutils.c
  src/native/libraries/htslib/kstring.c
  src/native/libraries/htslib/kfunc.c
  src/native/libraries/htslib/thread_pool.c
  src/native/libraries/htslib/cram/pooled_alloc.c
  src/native/libraries/htslib/hts_shim.c
)

set(HELIOS_PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/native/include")

set(HELIOS_INSTALL_INCLUDE_DIR "include")
set(HELIOS_INSTALL_CMAKE_DIR "lib/cmake/HeliosNetwork")

set(HELIOS_TARGETS)

function(helios_configure_target target_name)
  target_include_directories(${target_name}
    PUBLIC
      $<BUILD_INTERFACE:${HELIOS_PUBLIC_INCLUDE_DIR}>
      $<INSTALL_INTERFACE:${HELIOS_INSTALL_INCLUDE_DIR}>
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/src/native/libraries/htslib
  )
  target_compile_features(${target_name} PUBLIC c_std_17)
  target_link_libraries(${target_name} PRIVATE ZLIB::ZLIB)
  set_target_properties(${target_name} PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    PUBLIC_HEADER ""
  )
endfunction()

if(HELIOS_BUILD_SHARED)
  add_library(helios_shared SHARED ${HELIOS_SOURCES})
  set_target_properties(helios_shared PROPERTIES
    OUTPUT_NAME helios
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
  )
  helios_configure_target(helios_shared)
  list(APPEND HELIOS_TARGETS helios_shared)
endif()

if(HELIOS_BUILD_STATIC)
  add_library(helios_static STATIC ${HELIOS_SOURCES})
  set_target_properties(helios_static PROPERTIES
    OUTPUT_NAME helios
  )
  helios_configure_target(helios_static)
  list(APPEND HELIOS_TARGETS helios_static)
endif()

if(HELIOS_BUILD_SHARED AND HELIOS_BUILD_STATIC)
  add_library(HeliosNetwork::helios_shared ALIAS helios_shared)
  add_library(HeliosNetwork::helios_static ALIAS helios_static)
elseif(HELIOS_BUILD_SHARED)
  add_library(HeliosNetwork::helios ALIAS helios_shared)
elseif(HELIOS_BUILD_STATIC)
  add_library(HeliosNetwork::helios ALIAS helios_static)
endif()

include(GNUInstallDirs)

foreach(target IN LISTS HELIOS_TARGETS)
  install(TARGETS ${target}
    EXPORT HeliosNetworkTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${HELIOS_INSTALL_INCLUDE_DIR}
  )
endforeach()

install(DIRECTORY ${HELIOS_PUBLIC_INCLUDE_DIR}/helios
  DESTINATION ${HELIOS_INSTALL_INCLUDE_DIR}
)

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/HeliosNetworkConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
  cmake/HeliosNetworkConfig.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/HeliosNetworkConfig.cmake"
  INSTALL_DESTINATION ${HELIOS_INSTALL_CMAKE_DIR}
)

install(EXPORT HeliosNetworkTargets
  FILE HeliosNetworkTargets.cmake
  NAMESPACE HeliosNetwork::
  DESTINATION ${HELIOS_INSTALL_CMAKE_DIR}
)

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/HeliosNetworkConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/HeliosNetworkConfigVersion.cmake"
  DESTINATION ${HELIOS_INSTALL_CMAKE_DIR}
)

install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/licenses/helios-network)

if(HELIOS_BUILD_TESTS)
  enable_testing()
  add_executable(helios_sort_test tests/native/test_sort.c)
  target_include_directories(helios_sort_test PRIVATE ${HELIOS_PUBLIC_INCLUDE_DIR} ${HELIOS_PUBLIC_INCLUDE_DIR}/helios)
  target_compile_features(helios_sort_test PRIVATE c_std_17)
  add_test(NAME helios_sort_test COMMAND helios_sort_test)
endif()
