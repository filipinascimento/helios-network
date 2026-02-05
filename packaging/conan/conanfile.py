from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout


class HeliosNetworkConan(ConanFile):
    def config_options(self):
        if self.settings.os == "Windows":
            self.options.rm_safe("fPIC")

    name = "helios-network"
    version = "0.6.0"
    license = "MIT"
    url = "https://github.com/helios-graphs/helios-network"
    description = "Helios Network native graph core compiled as a C library"
    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }
    exports_sources = (
        "CMakeLists.txt",
        "cmake/*",
        "src/native/include/**",
        "src/native/src/**",
        "LICENSE",
    )

    def layout(self):
        cmake_layout(self)

    def generate(self):
        toolchain = CMakeToolchain(self)
        toolchain.cache_variables["HELIOS_BUILD_SHARED"] = "ON" if self.options.shared else "OFF"
        toolchain.cache_variables["HELIOS_BUILD_STATIC"] = "OFF" if self.options.shared else "ON"
        if self.options.get_safe("fPIC") is not None:
            toolchain.cache_variables["CMAKE_POSITION_INDEPENDENT_CODE"] = "ON" if self.options.fPIC else "OFF"
        toolchain.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
        self.copy("LICENSE", dst="licenses")

    def package_info(self):
        self.cpp_info.libs = ["helios"]
