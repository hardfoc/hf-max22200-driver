# Testing and CI

How the driver is checked: host unit tests against a model of the chip, compiled documentation, ESP32 builds and
static analysis, on every pull request.

## Host unit tests

The tests build the real driver against a mock port that models the MAX22200's SPI protocol: a CMD-high byte is
the command and returns STATUS[7:0], CMD-low transfers are 8 or 32 bits in the datasheet's byte order, reading
STATUS clears UVM and OVT, and reading FAULT clears the bits whose SDI bit is high. Faults and COMER replies can be
injected.

```bash
cmake -S . -B build-host -DHF_MAX22200_BUILD_TESTS=ON
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```

| File | Covers |
| --- | --- |
| [`tests/test_driver.cpp`](../tests/test_driver.cpp) | Command bytes, initialisation and COMER retries, CDR register encoding, switching, full-bridge states, faults, DPM, shutdown |
| [`tests/mock_max22200.hpp`](../tests/mock_max22200.hpp) | The chip model and port |
| [`tests/test_harness.hpp`](../tests/test_harness.hpp) | A dependency-free test registry and check macros |

## Documentation snippets

Every C++ block on this site is included from [`tests/docs/`](../tests/docs/my_spi.hpp), which the host build
compiles (target `hf_max22200_doc_snippets`) with the driver instantiated over the documented port. A change to
the API that breaks a page breaks the build first.

## CI workflows

| Workflow | What it checks |
| --- | --- |
| Host unit tests | GCC and Clang build and run the tests and the doc snippets |
| ESP32 examples build | The apps marked for CI in `examples/esp32/app_config.yml`, for each listed ESP-IDF version and build type |
| C++ lint, C++ analysis | clang-format, clang-tidy and cppcheck |
| Docs | Strict site build, figure and generated-table drift, Doxygen, link check, Lighthouse budget |
| Markdown lint, YAML lint, link check | Repository text files |

The workflows call the shared, versioned workflows of
[hf-general-ci-tools](https://github.com/hardfoc/hf-general-ci-tools) and
[hf-espidf-ci-tools](https://github.com/hardfoc/hf-espidf-ci-tools).

## On hardware

The [ESP32 examples](examples.md) are the hardware tests. `max22200_comprehensive_test` prints a pass / fail
summary over the serial monitor; run `max22200_diagnostic` first on a new board.
