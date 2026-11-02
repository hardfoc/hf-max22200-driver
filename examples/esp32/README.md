# MAX22200 ESP32 examples

ESP-IDF applications that run the MAX22200 driver on an ESP32-S3: a test suite, valve and solenoid cycling, a
bring-up diagnostic and plunger-movement tuning. The documentation site's
[Examples page](https://hardfoc.github.io/hf-max22200-driver/latest/examples/) lists them with links to the source.

## Wiring

The pins are set in [`main/esp32_max22200_test_config.hpp`](main/esp32_max22200_test_config.hpp); change them
there for another board.

| MAX22200 | ESP32-S3 | Notes |
| --- | --- | --- |
| SDO | GPIO35 (MISO) | Open drain: the port enables the internal pull-up; add an external one above 1 MHz |
| SDI | GPIO37 (MOSI) | |
| SCK | GPIO36 | SPI mode 0, 1 MHz in these examples |
| CSB | GPIO38 | Driven by the SPI peripheral |
| ENABLE | GPIO2 | Active high |
| CMD | GPIO39 | High for the command byte |
| FAULT | GPIO42 | Open drain, active low; internal pull-up enabled |
| TRIGA, TRIGB | GPIO40, GPIO41 | Active high; driven low (inactive) at start-up, pulsed by the trigger-pin test only |
| GND | GND | Common with the VM supply |

The board configuration assumes RREF = 15 kΩ (IFS = 1000 mA). VM must be 4.5 V to 36 V; the solenoid tests use
24 V with the coil from OUT0 (or every OUTn) to VM. `TestFramework.h` toggles GPIO14 after each test, for a
logic analyser.

## Build and flash

```bash
git submodule update --init scripts      # the HardFOC ESP-IDF build scripts (run in examples/esp32)
. $IDF_PATH/export.sh                     # ESP-IDF v5.5
./scripts/build_app.sh list
./scripts/build_app.sh max22200_comprehensive_test Debug
./scripts/flash_app.sh flash_monitor max22200_comprehensive_test Debug
```

[`app_config.yml`](app_config.yml) lists every app, its source file, the ESP-IDF versions and build types, and
whether CI builds it.

## The apps

| App | What it does |
| --- | --- |
| `max22200_comprehensive_test` | Runs the driver API on the chip: initialisation, raw registers, channel configuration, faults, control and trigger pins, board configuration, unit conversions, duty limits, CDR and VDR, error handling. Prints a pass / fail summary |
| `max22200_solenoid_valve_test` | Configures all eight channels with the same hit-and-hold profile (`C21ValveConfig`), then runs a sequential pattern (each channel 200 ms, 80 ms gap) and a parallel one (all on for 500 ms) with STATUS, FAULT and configuration read-back |
| `max22200_diagnostic` | Bring-up tool: pin levels before and after `Initialize()`, repeated raw STATUS reads, a CFG_CH0 write / read of `0x28500600`, FAULT and STATUS after a clear, then STATUS and FAULT at 5 Hz. Prints every SPI byte |
| `c21_cycle_test` | Cycles a Parker C21 24 V solenoid on channel 0 (CDR, 102 mA hit, 51 mA hold), 2 s on and 2 s off, with 10 Hz STATUS and FAULT telemetry |
| `c21_dpm_tuning_test` | Cycles the C21 with DPM on, samples FAULT at 1 kHz during the HIT window, prints CSV and a summary to tune the DPM start current, threshold and debounce |

## Logging

| Define (in the test configuration header) | Default | Effect |
| --- | --- | --- |
| `ESP32_MAX22200_ENABLE_DETAILED_SPI_LOGGING` | `0` | Logs every SPI transfer as TX / RX hex (on in `max22200_diagnostic`) |
| `ESP32_MAX22200_ENABLE_VERBOSE_BUS_LOGGING` | `1` | Logs bus and GPIO set-up |

The [SPI protocol page](https://hardfoc.github.io/hf-max22200-driver/latest/spi-protocol/) decodes the SPI log;
[Troubleshooting](https://hardfoc.github.io/hf-max22200-driver/latest/troubleshooting/) maps symptoms to causes.

A DPM flag means the expected current dip was not seen: a healthy valve shows no DPM flags. Hold the plunger still
during one cycle to see the flag set.
