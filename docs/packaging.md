# Native Packaging Guide

Helios Network now ships metadata that allows you to build and publish the native C core through the two most widely used C/C++ package managers: **vcpkg** and **Conan**. This document explains how to consume the local overlays for development and how to upstream the package definitions.

---

## Prerequisites

- CMake ≥ 3.18
- A C17-capable compiler (MSVC 2019+, Clang ≥ 12, GCC ≥ 10)
- Ninja or Make (optional, but recommended when using CMake presets)
- vcpkg or Conan 2.x, depending on the packaging route

The `CMakeLists.txt` at the repository root builds both static and shared variants of the `helios` library and installs headers under `include/helios`.

```bash
cmake -S . -B build -DHELIOS_BUILD_SHARED=ON -DHELIOS_BUILD_STATIC=OFF
cmake --build build
cmake --install build --prefix install
```

---

## vcpkg

### Local overlay usage

1. Bootstrap vcpkg if you have not already: `./vcpkg/bootstrap-vcpkg.sh` (or `.bat` on Windows).
2. Install the package via the overlay port included in this repository:

```bash
./vcpkg/vcpkg install helios-network --overlay-ports=/path/to/helios-network/packaging/vcpkg
```

The overlay port resides at `packaging/vcpkg/helios-network` and invokes the root CMake project via the helper macros `vcpkg_cmake_configure/build/install`. Headers are copied into `<packages>/helios-network/include/helios`, and both `libhelios.a` and the platform-specific shared library are produced.

### Upstreaming to the official vcpkg registry

1. Fork <https://github.com/microsoft/vcpkg>.
2. Copy `packaging/vcpkg/helios-network` into `ports/helios-network` inside your fork.
3. Replace the local `set(SOURCE_PATH ...)` stanza in `portfile.cmake` with a proper `vcpkg_from_github()` (or `vcpkg_from_gitlab()`) call that references a tagged release of this repository, and record the release SHA512.
4. Update the version database by running `./vcpkg/vcpkg x-add-version helios-network`.
5. Build and test the port on all relevant triplets: `./vcpkg/vcpkg install helios-network:x64-windows` (and similar for Linux/macOS).
6. Run the format helper `./vcpkg/vcpkg format-manifest ports/helios-network/vcpkg.json`.
7. Submit a pull request to upstream vcpkg describing the library, supported triplets, and test results.

---

## Conan

### Local development

1. Ensure Conan 2.x is installed: `pip install conan`.
2. From the repository root run:

```bash
conan create ./packaging/conan --build=missing
```

The bundled recipe supports the standard `shared` and `fPIC` options. By default it produces a static library; toggle `-o helios-network/*:shared=True` to request a shared build.

### Publishing to ConanCenter

1. Fork <https://github.com/conan-io/conan-center-index>.
2. Create a new folder `recipes/helios-network/all` and copy `packaging/conan/conanfile.py` into it as `conanfile.py`. Commonly you will also add a `config.yml` declaring the version list.
3. Adapt the recipe to fetch the official source tarball (for example using `conan.tools.files.get`) instead of relying on `exports_sources`.
4. Provide test packages under `test_v1_package` or `test_package` that link against the installed library; ConanCenter requires at least one positive compile/link test.
5. Run the full ConanCenter hook suite locally: `conan create recipes/helios-network/all --build=missing`.
6. Submit a pull request to ConanCenter that references your release tag and includes the test logs.

Refer to the official contribution guides for detailed policies:

- vcpkg: <https://learn.microsoft.com/vcpkg/contribute/>
- ConanCenter: <https://github.com/conan-io/conan-center-index/blob/master/docs/contributing.md>
