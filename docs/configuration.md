# Configuration

Every field of the four configuration structs, the register bits it maps to and its default.

The structs are in [`inc/max22200_types.hpp`](../inc/max22200_types.hpp); register bit positions are in
[`inc/max22200_registers.hpp`](../inc/max22200_registers.hpp). [Drive modes](drive-modes.md) explains what the
channel settings do on the output.

## BoardConfig

The board's full-scale current and optional safety limits. Pass it to the constructor or `SetBoardConfig()`.

| Field | Meaning |
| --- | --- |
| `full_scale_current_ma` | Default 1000. IFS in mA. `BoardConfig(rref_kohm, hfs)` sets it to KFS / RREF |
| `max_current_ma` | Default 0 (none). Upper clamp for the `Set...Current...()` calls |
| `max_duty_percent` | Default 0 (none). Upper clamp for `SetHitDutyPercent()` and `SetHoldDutyPercent()` |

The limits apply to the per-setting calls only, not to `ConfigureChannel()`. Construct with `hfs = false` and use
each channel's `half_full_scale` for HFS; passing `true` halves IFS for every channel and HFS halves it again.

## ChannelConfig

One CFG_CHx register (datasheet Table 11). Defaults are the chip's reset values: CDR, low side, SPI-triggered,
every check off.

| Field | Meaning |
| --- | --- |
| `half_full_scale` | Bit 31 (HFS). IFS / 2 and doubled low-side RON for this channel; low side only |
| `hold_setpoint` | Bits 30:24 (HOLD). CDR: mA. VDR: duty percent |
| `trigger_from_pin` | Bit 23 (TRGnSPI). `true`: the TRIG pin switches the channel instead of ONCH |
| `hit_setpoint` | Bits 22:16 (HIT). CDR: mA. VDR: duty percent |
| `hit_time_ms` | Bits 15:8 (HIT_T). HIT time in ms; 0 = none, negative = continuous |
| `drive_mode` | Bit 7 (VDRnCDR). `DriveMode::CDR` or `VDR`; change only with every channel off |
| `side_mode` | Bit 6 (HSnLS). `SideMode::LOW_SIDE` (load to VM) or `HIGH_SIDE` (load to GND, VDR only) |
| `chop_freq` | Bits 5:4 (FREQ_CFG). `ChopFreq::FMAIN_DIV4` (default), `FMAIN_DIV3`, `FMAIN_DIV2`, `FMAIN` |
| `slew_rate_control_enabled` | Bit 3 (SRC). Slow edges; low side, below 50 kHz only |
| `open_load_detection_enabled` | Bit 2 (OL_EN). OLF flag when the load is disconnected |
| `plunger_movement_detection_enabled` | Bit 1 (DPM_EN). DPM on this channel; low side only |
| `hit_current_check_enabled` | Bit 0 (HHF_EN). HHF flag when the HIT current is not reached in the HIT time |


`toRegister(ifs_ma, freqm)` and `fromRegister(value, ifs_ma, freqm)` convert between the struct and the raw
register; the driver calls them with the board IFS and the cached FREQM. Accessors such as `isCdr()`,
`hasHitTime()` and `isContinuousHit()` read the fields by intent.

## StatusConfig

The STATUS register (datasheet Table 10). Read it with `ReadStatus()`, change it, write it with `WriteStatus()`.

| Field | Meaning |
| --- | --- |
| `channels_on_mask` | Bits 31:24 (ONCH). Bit n switches channel n on |
| `overtemperature_masked` ... `undervoltage_masked` | Bits 23:17 (M_*). `true`: that fault does not pull FAULT low. `communication_error_masked` resets to `true` |
| `master_clock_80khz` | Bit 16 (FREQM). `false`: 100 kHz oscillator, `true`: 80 kHz |
| `channel_pair_mode_76` ... `_10` | Bits 15:8 (CMxy). `ChannelMode::INDEPENDENT`, `PARALLEL` or `HBRIDGE`; change only with both channels off |
| `overtemperature` ... `undervoltage` | Bits 7:1. Fault flags, read-only; reading STATUS clears OVT and UVM |
| `active` | Bit 0 (ACTIVE). `true`: normal operation; `false`: low power, outputs Hi-Z |


## FaultStatus

The FAULT register (datasheet Table 12), one bitmask per fault type, bit n for channel n.

| Field | Meaning |
| --- | --- |
| `overcurrent_channel_mask` | Bits 31:24 (OCP). The output exceeded the OCP threshold; the channel is off |
| `hit_not_reached_channel_mask` | Bits 23:16 (HHF). The HIT current was not reached in the HIT time |
| `open_load_fault_channel_mask` | Bits 15:8 (OLF). The load is disconnected |
| `plunger_movement_fault_channel_mask` | Bits 7:0 (DPM). No valid current dip was seen in the HIT phase |

`hasFault()`, `hasOvercurrentOnChannel(ch)`, `channelsWithAnyFault()` and `getFaultCount()` summarise it.

## DpmConfig

The CFG_DPM register, global for every channel with DPM enabled. `ConfigureDpm(start_ma, threshold_ma,
debounce_ms)` fills it from real units, converting the debounce with the chopping frequency of the DPM channels;
`ConfigureDpm(start_ma, threshold_ma, debounce_ms, chop_freq)` takes the frequency explicitly.

| Field | Meaning |
| --- | --- |
| `plunger_movement_start_current` | Bits 14:8. Start current = value × IFS / 127 |
| `plunger_movement_debounce_time` | Bits 7:4. Minimum dip length = value / fCHOP |
| `plunger_movement_current_threshold` | Bits 3:0. Minimum dip depth = value × IFS / 127 |

## Recommended setups

| Load | Mode | Notes |
| --- | --- | --- |
| Solenoid valve or relay to VM | CDR, low side | `makeSolenoidCdr()`; enable HHF and DPM for diagnostics |
| Load to GND | VDR, high side | No CDR, HFS, SRC or DPM on the high side |
| Coil under half of IFS | CDR, `half_full_scale` | Twice the current resolution |
| Brushed DC motor, latching valve | `HBRIDGE` pair | `SetFullBridgeState()`; different settings per direction |
| Load above 1 A | `PARALLEL` pair | Wire both outputs together; the even channel's settings apply |

## Next

[API guide](api-guide.md) shows these structs in use; [Porting](porting.md) writes the SPI port.
