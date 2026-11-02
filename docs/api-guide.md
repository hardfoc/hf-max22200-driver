# API guide

How to do the common jobs with the driver: initialise, configure channels, switch them, run bridges, read faults
and detect plunger movement.

Every snippet on this page is compiled by the host CI from
[`tests/docs/api_tasks.cpp`](../tests/docs/api_tasks.cpp), with `Driver` standing for
`max22200::MAX22200<MySpi>` and `DriverStatus` for `max22200::DriverStatus`. The
[API reference](api-reference.md) lists every member.

## The shape of the API

One `MAX22200<SpiType>` object per chip. Its calls work at three levels:

| Level | Example | Use it for |
| --- | --- | --- |
| A whole channel | `ConfigureChannel(ch, ChannelConfig)` | Bring-up: every CFG_CHx field in one write |
| One setting in real units | `SetHoldCurrentMa(ch, 120)`, `SetHitTimeMs(ch, 20)` | Changing one value of a configured channel |
| Raw registers | `ReadRegister32(bank, v)`, `WriteRegister8(bank, v)` | Anything the helpers do not cover |

| Area | Calls |
| --- | --- |
| Start and stop | `Initialize()`, `Deinitialize()`, `EnableDevice()`, `DisableDevice()` |
| Channels | `ConfigureChannel()`, `ConfigureChannelCdr()`, `ConfigureChannelVdr()`, `GetChannelConfig()` |
| Switching | `EnableChannel()`, `DisableChannel()`, `SetChannelsOn()`, `SetFullBridgeState()` |
| Real units | `SetHit/HoldCurrentMa()`, `...CurrentA()`, `...CurrentPercent()`, `...DutyPercent()`, `SetHitTimeMs()` |
| STATUS | `ReadStatus()`, `WriteStatus()` |
| Faults | `ReadFaultRegister()`, `ClearChannelFaults()`, `ClearAllFaults()`, `GetFaultPinState()` |
| DPM | `ConfigureDpm()`, `SetDpmEnabledChannels()`, `ReadDpmConfig()`, `WriteDpmConfig()` |

Every call that reaches the chip returns `DriverStatus` and never throws. Check it: anything but `OK` means the
value was rejected or the transfer failed ([Architecture](architecture.md#errors-without-exceptions)).

## Initialise

```cpp
--8<-- "tests/docs/api_tasks.cpp:init"
```

`Initialize()` follows the datasheet's programming flow (Figure 6): it calls your port's `Initialize()` and
`Configure(10 MHz, mode 0, MSB first)`, raises ENABLE, waits 0.5 ms, reads STATUS to clear the power-up UVM flag,
writes STATUS with ACTIVE = 1, every channel off and COMER masked (other masks and pair modes as read: their reset
values after power-up), waits 2.5 ms (t<sub>WU</sub>, ACTIVE = 1 to outputs active), then reads STATUS again. A
COMER reply restarts the sequence, up to three times.

Set the board's full-scale current before you configure a CDR channel. `BoardConfig()` without arguments assumes
1000 mA (RREF = 15 kΩ); a wrong IFS scales every current by the same error. Because `Initialize()` waits
t<sub>WU</sub>, the outputs switch as soon as it returns; it takes at least 3 ms.

`Deinitialize()`, also called by the destructor, turns every channel off, clears ACTIVE and lowers ENABLE.

## Configure a channel

Configure in this order: pair modes in STATUS (if any), then each channel, then switch channels on. Drive mode and
side change only while every channel is off.

=== "Current drive (CDR)"

    ```cpp
    --8<-- "tests/docs/api_tasks.cpp:configure-cdr"
    ```

=== "Voltage drive (VDR)"

    ```cpp
    --8<-- "tests/docs/api_tasks.cpp:configure-vdr"
    ```

`ChannelConfig` holds every CFG_CHx field; [Configuration](configuration.md#channelconfig) lists them. The
presets `makeSolenoidCdr(hit_ma, hold_ma, hit_ms)` and `makeSolenoidVdr(hit_pct, hold_pct, hit_ms)` fill a low-side
channel at `FMAIN_DIV4`; `ConfigureChannelCdr()` and `ConfigureChannelVdr()` do the same in one call with the other
fields as arguments.

## Switch channels

```cpp
--8<-- "tests/docs/api_tasks.cpp:switch"
```

All switching writes STATUS[31:24], the ONCH byte, with an 8-bit access. The driver keeps the last ONCH value, so
`EnableChannel(2)` leaves the other channels as they were. A channel whose `trigger_from_pin` is set follows its
TRIG pin instead of ONCH.

## Change a setting while running

```cpp
--8<-- "tests/docs/api_tasks.cpp:hold"
```

Each `Set...()` call reads CFG_CHx, changes one field and writes all 32 bits back. HOLD current, chopping frequency
and SRC can change while the channel is on; drive mode and side cannot. The `...A()` variants take amperes, the
`...CurrentPercent()` variants a fraction of IFS. The setters never change the drive mode: the current setters
work on a CDR channel and the duty setters on a VDR channel, and on a channel in the other mode they return
`INVALID_PARAMETER` without writing anything. Select the mode with `ConfigureChannel()`,
`ConfigureChannelCdr()` or `ConfigureChannelVdr()`.

## Run a full bridge

```cpp
--8<-- "tests/docs/api_tasks.cpp:bridge"
```

| `FullBridgeState` | OUTx / OUTy | Settings from |
| --- | --- | --- |
| `HiZ` (ONCHy = 0, ONCHx = 0) | Hi-Z / Hi-Z | |
| `Forward` (0, 1) | VM / regulated | CFG_CHy |
| `Reverse` (1, 0) | regulated / VM | CFG_CHx |
| `Brake` (1, 1) | GND / GND | |

x is the even channel of the pair, y the odd one. In `PARALLEL` mode the even channel's configuration drives both
outputs and the odd channel's is ignored; switch both channels on together.

## Handle faults

```cpp
--8<-- "tests/docs/api_tasks.cpp:faults"
```

| Signal | Meaning | What to do |
| --- | --- | --- |
| `UVM` in STATUS | VM fell below about 4 V; always set at power-up | Reading STATUS clears it |
| `OVT` in STATUS | Thermal shutdown at 145 °C; outputs off until it cools | Reading STATUS clears it |
| `COMER` in STATUS or `GetLastFaultByte()` = `0x04` | The chip ignored a transfer with a wrong clock count | Check `Transfer()` lengths and CS |
| OCP bit in FAULT | Output above 1.1 A; that channel turned off | Clear ONCH, clear the flag, find the short |
| HHF bit in FAULT | HIT current not reached in the HIT time (if `hit_current_check_enabled`) | Coil resistance, VM, HIT level |
| OLF bit in FAULT | Load disconnected (if `open_load_detection_enabled`); clears when the output turns on | Wiring |
| DPM bit in FAULT | No current dip in the HIT phase: the plunger did not move | Stuck valve, DPM thresholds |
| FAULT pin low | Any unmasked fault above | Read STATUS and FAULT |

STATUS bits OCP, OLF, HHF and DPM summarise FAULT; reading STATUS does not clear them. Mask a fault from the FAULT
pin with the `..._masked` fields of `StatusConfig` and `WriteStatus()`. The fault and state callbacks
(`SetFaultCallback()`, `SetStateChangeCallback()`) are stored but never called by the driver; call them from your
own polling code if you use them.

## Detect plunger movement

```cpp
--8<-- "tests/docs/api_tasks.cpp:dpm"
```

DPM looks for the dip a moving plunger causes in the HIT current. The search starts above the start current and
ends at the HIT level or the end of the HIT time; a dip counts when it is deeper than the threshold and longer
than the debounce time. The settings are global; DPM_EN is per channel and low-side only. The datasheet asks for a
current slope below 700 mA/ms above the start current. The debounce is a count of chopping periods
(DPM_TDEB / f<sub>CHOP</sub>), and f<sub>CHOP</sub> is per channel: `ConfigureDpm(start, threshold, debounce)` converts
it with the `chop_freq` of the channels that have DPM enabled, so enable DPM first. With no DPM channel it uses
`FMAIN_DIV4`; DPM channels at different frequencies return `INVALID_PARAMETER`. Pass the frequency as a fourth
argument to choose it yourself.

## Use a register directly

```cpp
--8<-- "tests/docs/api_tasks.cpp:raw"
```

Banks are `max22200::RegBank::STATUS`, `CFG_CH0` to `CFG_CH7`, `FAULT` and `CFG_DPM`; bit positions are in the
`StatusReg`, `CfgChReg`, `FaultReg` and `CfgDpmReg` namespaces. An 8-bit access reaches bits [31:24] only. A raw
STATUS write bypasses the driver's cache: call `ReadStatus()` afterwards.

## Count transfers

```cpp
--8<-- "tests/docs/api_tasks.cpp:stats"
```

Only `total_transfers` and `failed_transfers` are counted; `fault_events`, `state_changes` and `uptime_ms` stay 0.
