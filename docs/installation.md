# Installation

Add the driver to a CMake or ESP-IDF project. It is a header-only C++20 library with no dependencies.

## Requirements

- A C++20 compiler: GCC 10+, Clang 12+ or a current MSVC.
- No RTOS, heap or exceptions: every call returns a `max22200::DriverStatus` and never throws.

## Get the source

A git submodule keeps the driver pinned to a version you chose:

```bash
git submodule add https://github.com/hardfoc/hf-max22200-driver.git external/hf-max22200-driver
git submodule update --init external/hf-max22200-driver
```

Only `inc/`, `src/`, `cmake/` and `CMakeLists.txt` are needed to build. The examples have their own submodule
(`examples/esp32/scripts`); you do not need it to use the driver.

## CMake

```cmake
add_subdirectory(external/hf-max22200-driver)
target_link_libraries(my_app PRIVATE hf::max22200)
```

`hf::max22200` is an alias of the `hf_max22200` interface target. It carries the include directories (`inc/` and
the generated version header) and requires C++20 (`cxx_std_20`), so your target does not need to set either.

| Option | Default | Effect |
| --- | --- | --- |
| `HF_MAX22200_ENABLE_WARNINGS` | `OFF` | Adds `-Wall -Wextra -Wpedantic` to targets that link the driver |
| `HF_MAX22200_BUILD_TESTS` | `OFF` | Builds the host unit tests in `tests/` ([Testing and CI](testing.md)) |

### Installed package

`cmake --install` copies the headers, the generated `max22200_version.h` and the template implementation
`max22200.ipp` into `<prefix>/include`, and a CMake package into `<prefix>/lib/cmake/hf_max22200`. A project
then finds it by name and links the same target:

```bash
cmake -S external/hf-max22200-driver -B build-max22200
cmake --install build-max22200 --prefix /opt/hardfoc
```

```cmake
find_package(hf_max22200 REQUIRED)   # with CMAKE_PREFIX_PATH=/opt/hardfoc
target_link_libraries(my_app PRIVATE hf::max22200)
```

## ESP-IDF

The ESP-IDF component wrapper is
[`examples/esp32/components/hf_max22200/`](../examples/esp32/components/hf_max22200/CMakeLists.txt). Copy that
folder into your project's `components/` (or point `EXTRA_COMPONENT_DIRS` at it) and set the `HF_MAX22200_ROOT`
cache variable to the driver checkout; the wrapper stops with a clear message when the path is wrong. Your
component then declares `REQUIRES hf_max22200`.

## Without CMake

Add `inc/` to the include path and generate `max22200_version.h` from
[`inc/max22200_version.h.in`](../inc/max22200_version.h.in) once (replace the four `@...@` fields with the
version). `max22200.hpp` includes `src/max22200.ipp` by relative path, so keep `inc/` and `src/` side by side.

## How the build is organised

| File | Role |
| --- | --- |
| [`cmake/hf_max22200_build_settings.cmake`](../cmake/hf_max22200_build_settings.cmake) | Target name, include directories, version: the single source of truth |
| [`CMakeLists.txt`](../CMakeLists.txt) | Interface library for host builds and CI |
| [`examples/esp32/components/hf_max22200/CMakeLists.txt`](../examples/esp32/components/hf_max22200/CMakeLists.txt) | ESP-IDF component over the same settings |

The version is set once in the settings file and written into `max22200_version.h` at configure time. Read it at
run time:

```cpp
const char* v = max22200::MAX22200<MySpi>::GetDriverVersion();   // "1.0.0"
```

## Next

[Quickstart](quickstart.md) wires the chip and drives a solenoid; [Porting](porting.md) writes the SPI interface
for your MCU.
