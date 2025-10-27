set(SOURCE_PATH "${CURRENT_PORT_DIR}/../../..")

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DHELIOS_BUILD_SHARED=ON
        -DHELIOS_BUILD_STATIC=ON
)

vcpkg_cmake_build()

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(
    PACKAGE_NAME HeliosNetwork
)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
