# Examples

Complete ESP32-S3 applications that exercise the driver on a MAX22200 board, from a full test suite to solenoid
cycling and plunger-movement tuning.

They run on an ESP32-S3 wired to a MAX22200 at 1 MHz SPI (SDO on GPIO35, SDI on GPIO37, SCK on GPIO36, CSB on
GPIO38; ENABLE on GPIO2, FAULT on GPIO42, CMD on GPIO39) with RREF = 15 kΩ. The pins are in
[`esp32_max22200_test_config.hpp`](../examples/esp32/main/esp32_max22200_test_config.hpp) and the port in
[`esp32_max22200_bus.hpp`](../examples/esp32/main/esp32_max22200_bus.hpp). CI builds the apps marked "yes" on each
pull request.

## The apps

<!-- generated: examples -->
| App | What it shows | Source | CI |
| --- | --- | --- | --- |
| `max22200_comprehensive_test` :material-star:{ title="featured" } | Comprehensive MAX22200 driver test suite covering all functionality | [`max22200_comprehensive_test.cpp`](../examples/esp32/main/max22200_comprehensive_test.cpp) | yes |
| `max22200_solenoid_valve_test` :material-star:{ title="featured" } | Solenoid/valve test: all 8 channels, C21 hit/hold, sequential and parallel patterns, full diagnostics | [`max22200_solenoid_valve_test.cpp`](../examples/esp32/main/max22200_solenoid_valve_test.cpp) | no |
| `max22200_diagnostic` | Fault-isolation diagnostic: round-trips CFG_CH0, dumps STATUS+FAULT raw, tries both ACTIVE=1 write paths, then 5 Hz continuous STATUS dump for scope probing | [`max22200_diagnostic.cpp`](../examples/esp32/main/max22200_diagnostic.cpp) | no |
| `c21_cycle_test` | Cycle a Parker C21 24V solenoid on CH0 (102 mA hit / 51 mA hold) with continuous STATUS+FAULT telemetry and DPM detection | [`c21_cycle_test.cpp`](../examples/esp32/main/c21_cycle_test.cpp) | no |
| `c21_dpm_tuning_test` | Cycle C21 with DPM enabled, poll FAULT @ 1 kHz during HIT window, emit CSV stream + summary stats so DPM ISTART/IPTH/TDEB can be tuned against real plunger movement | [`c21_dpm_tuning_test.cpp`](../examples/esp32/main/c21_dpm_tuning_test.cpp) | no |

Build and flash one (from `examples/esp32`, ESP-IDF environment loaded):

```bash
./scripts/build_app.sh max22200_comprehensive_test Release
./scripts/flash_app.sh flash_monitor max22200_comprehensive_test Release
```

Source of this table: [`examples/esp32/app_config.yml`](../examples/esp32/app_config.yml).
<!-- /generated -->

## Set up once

```bash
git submodule update --init examples/esp32/scripts   # the HardFOC ESP-IDF build scripts
cd examples/esp32
. $IDF_PATH/export.sh                                 # ESP-IDF v5.5
./scripts/build_app.sh list                           # every app and build type
```

The scripts read [`app_config.yml`](../examples/esp32/app_config.yml) (the same file this page's table comes
from) and build each app as its own firmware image.

## What to read in an example

| You want to | Read |
| --- | --- |
| See every driver call exercised, with pass / fail output | `max22200_comprehensive_test.cpp` |
| Drive all eight channels with HIT / HOLD patterns | `max22200_solenoid_valve_test.cpp` |
| Find out why a board does not respond: raw STATUS, FAULT and CFG_CH0 round trips | `max22200_diagnostic.cpp` |
| Cycle one solenoid with STATUS and FAULT telemetry | `c21_cycle_test.cpp` |
| Tune DPM start current, threshold and debounce against a real valve | `c21_dpm_tuning_test.cpp` |
| Write an ESP-IDF port | `esp32_max22200_bus.hpp` |

The [example folder's README](../examples/esp32/README.md) has the wiring, the build options and what each app
prints.
