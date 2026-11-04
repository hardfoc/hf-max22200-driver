# Troubleshooting

Symptoms you may see while bringing up a MAX22200, their causes and the fix for each.

Start with the cheapest checks: the return value of the failing call, `GetLastFaultByte()` and a raw STATUS read.
They tell bus problems (no reply, COMER) apart from chip problems (the chip answers but reports a fault). The
`max22200_diagnostic` example prints all three, plus a CFG_CH0 round trip, with every SPI byte.

## Start-up

| Symptom | Cause | Fix |
| --- | --- | --- |
| `Initialize()` returns `INITIALIZATION_ERROR` | Your port's `Initialize()` or `Configure()` returned `false`, or its `GpioSet()` could not raise ENABLE | Check the port: SPI host, pins, mode 0 accepted |
| `Initialize()` returns `COMMUNICATION_ERROR` | `Transfer()` failed, or the chip answered COMER (`0x04`) three times | CMD wiring and level; `Transfer()` blocking until CS is high; one call, one CS frame |
| Every read is `0xFF` or `0x00` | SDO not pulled up, ENABLE low, VM off or below 4.5 V | Pull-up on SDO, ENABLE trace, VM at the chip pins |
| `Initialize()` is `OK` but STATUS reads ACTIVE = 0 or UVM = 1 | VM sagging below UVLO, or V18 not settled | Check VM under load, the 2.2 µF on V18, the charge-pump parts; write ACTIVE again after the supply is stable |
| STATUS has impossible values (reserved CMxy = 11, ONCH set at boot) | The chip kept its registers through an MCU reset | Registers reset only when V18 falls below about 1 V: power-cycle VM |

## Channels

| Symptom | Cause | Fix |
| --- | --- | --- |
| Channel is on in ONCH but the output does not switch | ACTIVE = 0, or less than 2.5 ms (t<sub>WU</sub>) since you set ACTIVE = 1 with `WriteStatus()` | Check `StatusConfig::active`; wait t<sub>WU</sub> after a raw ACTIVE write (`Initialize()` already waits); check your port's `DelayUs()` |
| Low-side channel does nothing, high side works (or the reverse) | The load is wired for the other side | Low side: load to VM. High side: load to GND, VDR only |
| `ConfigureChannel()` returns `INVALID_PARAMETER` | IFS is 0, SRC at 50 kHz or more, or channel above 7 | `SetBoardConfig()`, a lower `chop_freq`, the channel number |
| A current or duty setter returns `INVALID_PARAMETER` | The channel is in the other drive mode: current setters need CDR, duty setters VDR | Configure the mode first with `ConfigureChannelCdr()` / `ConfigureChannelVdr()` |
| Drive mode or side does not change | A channel is on, or TRIGA / TRIGB is high | Switch every channel off and hold both TRIG pins low first |
| Current is off by a constant factor | IFS does not match RREF, or `BoardConfig(r, true)` and `half_full_scale` both set | `BoardConfig(rref_kohm, false)` with the real RREF |
| VDR duty is higher than requested | Driver 1.0.0, which scaled VDR percent by 127 / 100 | Update the driver; see [Drive modes](drive-modes.md#voltage-drive-vdr) |
| HIT time reads back shorter or longer | HIT_T is an 8-bit count of 40 chopping periods | Expected; compare raw register values |
| Another channel turns off when you enable one | STATUS was written outside the driver; the ONCH cache is stale | `ReadStatus()` after any raw STATUS write |

## Faults

| Symptom | Cause | Fix |
| --- | --- | --- |
| FAULT low right after power-up | UVM is set at every power-up | `Initialize()` reads STATUS, which clears it |
| OCP on a channel | Output above 1.1 A: a short, or a coil with too low resistance in VDR | Clear ONCH, read FAULT to clear the flag, fix the load |
| HHF on a channel | HIT current not reached within the HIT time | VM too low for the coil resistance, HIT too high, HIT time too short |
| OLF on a channel | Load disconnected, with `open_load_detection_enabled` set | Wiring; the flag clears when the output turns on |
| DPM on every cycle | No dip seen: the plunger is stuck, or the thresholds are too strict | Hold the plunger to compare; tune with `c21_dpm_tuning_test` |
| DPM never fires, even with the plunger held | Current slope above 700 mA/ms above the start current, or the start current above the dip | Set the start current just below the expected dip; check the slope on a scope |
| `ClearChannelFaults()` clears other channels too | The part is a MAX22200, not a MAX22200A | Expected: the MAX22200 clears every flag on any FAULT read |

A DPM flag means no dip was revealed, so a healthy valve produces no DPM flags. To check that detection works,
hold the plunger still during one cycle: the flag should set for that cycle.
