# Quickstart

Pull in a solenoid from your MCU: wire seven signals, write a small port class, call four driver functions.

!!! warning "Before you power the load"
    Use a current-limited VM supply for the first run. The example turns channel 0 on at 400 mA as soon as the
    chip is up, and `ChannelConfig` defaults to low-side drive: the coil goes from OUT0 to VM.

## 1. Wire it

| MCU | MAX22200 | Notes |
| --- | --- | --- |
| SPI SCLK, MOSI, CS | SCK, SDI, CSB | SPI mode 0; start at 1 MHz |
| SPI MISO | SDO | Open drain: pull it up to the MCU's I/O voltage |
| GPIO out | ENABLE | High enables the chip; internal pull-down |
| GPIO out | CMD | High while the command byte is sent |
| GPIO in | FAULT | Open drain, low on a fault: pull it up |
| GND | GND, EP | Common ground between the MCU and VM |

Tie TRIGA and TRIGB low (or leave them to their internal pull-downs) unless you trigger channels from pins. Put a
resistor of 15 kΩ to 100 kΩ from IREF to GND: it sets the full-scale current for CDR. The
[Hardware](hardware.md) page has the full pin list, the external parts and the timing.

## 2. Add the driver

Add the repository as a submodule and link the `hf::max22200` CMake target (or add `inc/` to your include path and
build with C++20). [Installation](installation.md) covers CMake and ESP-IDF.

```bash
git submodule add https://github.com/hardfoc/hf-max22200-driver.git external/hf-max22200-driver
```

## 3. Write the port

The port is the only platform code. It runs one blocking SPI transaction per call, drives ENABLE and CMD, reads
FAULT and waits. Replace the `platform_*` calls with your SDK's.

```cpp title="my_spi.hpp"
--8<-- "tests/docs/my_spi.hpp:port"
```

`Transfer()` must hold CS low for the whole buffer and return only after CS is high again: the driver drops CMD
right after it returns. [Porting](porting.md) explains each method.

## 4. Pull in the solenoid

```cpp title="main.cpp"
--8<-- "tests/docs/quickstart.cpp:quickstart"
```

What each step does:

1. `BoardConfig(30.0f, false)` gives the driver the full-scale current from your RREF: IFS = 15 000 × 1 V / RREF,
   so 30 kΩ is 500 mA. Every milliamp value is converted against it.
2. `Initialize()` raises ENABLE, waits 0.5 ms, reads STATUS (clearing the power-up UVM flag) and writes STATUS
   with ACTIVE = 1 and every channel off.
3. `makeSolenoidCdr()` builds a low-side, current-regulated channel: 400 mA until the HIT time ends, then
   150 mA. `ConfigureChannel()` encodes it into CFG_CH0.
4. `EnableChannel()` sets ONCH0 with one 8-bit write. The HIT phase starts, then the chip drops to HOLD by itself.

This example is built by the host CI, so it compiles against the current headers.

## If it does not work

| Symptom | First thing to check |
| --- | --- |
| `Initialize()` returns `COMMUNICATION_ERROR` | CMD wiring and level, SPI mode 0, `Transfer()` blocking |
| Every read is `0xFF` | SDO pull-up missing or too weak, ENABLE low, VM off |
| Initialises, but the output never switches | Load wiring (low side: coil to VM), VM above 4.5 V, ACTIVE in STATUS |
| FAULT stays low | Read STATUS and FAULT ([Troubleshooting](troubleshooting.md)) |

## Next

- [API guide](api-guide.md): CDR and VDR channels, switching, bridges, faults and plunger-movement detection.
- [Drive modes](drive-modes.md): how HIT, HOLD, the HIT time and the chopping frequency map to registers.
- [Examples](examples.md): ESP32 applications for valves and diagnostics.
