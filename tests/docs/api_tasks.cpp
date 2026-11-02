/**
 * @file api_tasks.cpp
 * @brief The API guide's and Configuration page's task snippets, compiled by the host build (never run).
 */
#include "my_spi.hpp"

#include <cstdio>

using Driver = max22200::MAX22200<MySpi>;
using max22200::DriverStatus;

bool init(MySpi& spi) {
  // --8<-- [start:init]
  Driver driver(spi);
  driver.SetBoardConfig(max22200::BoardConfig(15.0f, false));   // RREF 15 kOhm: IFS = 1000 mA

  const DriverStatus st = driver.Initialize();
  if (st != DriverStatus::OK) {
    // INITIALIZATION_ERROR: Initialize() or Configure() of the port failed.
    // COMMUNICATION_ERROR: a transfer failed, or the chip answered COMER three times.
    std::printf("MAX22200 init failed: %d\n", static_cast<int>(st));
    return false;
  }
  // --8<-- [end:init]
  return true;
}

bool configure_cdr(Driver& driver) {
  // --8<-- [start:configure-cdr]
  max22200::ChannelConfig cfg;                    // defaults: CDR, low side, FMAIN_DIV4, all checks off
  cfg.drive_mode = max22200::DriveMode::CDR;
  cfg.side_mode = max22200::SideMode::LOW_SIDE;
  cfg.hit_setpoint = 600.0f;                      // mA, peak current during the HIT time
  cfg.hold_setpoint = 200.0f;                     // mA, after the HIT time
  cfg.hit_time_ms = 30.0f;                        // rounded to a multiple of 40 / fCHOP
  cfg.chop_freq = max22200::ChopFreq::FMAIN_DIV4; // 25 kHz with FREQM = 0
  cfg.hit_current_check_enabled = true;           // HHF flag if 600 mA is not reached in 30 ms
  cfg.open_load_detection_enabled = true;         // OLF flag if the coil is disconnected

  if (driver.ConfigureChannel(0, cfg) != DriverStatus::OK) {
    return false;   // INVALID_PARAMETER: channel > 7, IFS = 0, or SRC above 50 kHz
  }
  // --8<-- [end:configure-cdr]
  return true;
}

bool configure_vdr(Driver& driver) {
  // --8<-- [start:configure-vdr]
  // High-side load (to GND): VDR only. Setpoints are duty cycle in percent, written as HIT[6:0] = percent.
  auto cfg = max22200::ChannelConfig::makeSolenoidVdr(100.0f, 40.0f, 50.0f);
  cfg.side_mode = max22200::SideMode::HIGH_SIDE;
  if (driver.ConfigureChannel(1, cfg) != DriverStatus::OK) {
    return false;
  }

  max22200::DutyLimits limits;                    // deltaMIN / deltaMAX for this channel's fCHOP and SRC
  if (driver.GetDutyLimits(1, limits) == DriverStatus::OK) {
    std::printf("duty %u..%u %%\n", limits.min_percent, limits.max_percent);
  }
  // --8<-- [end:configure-vdr]
  return true;
}

bool switch_channels(Driver& driver) {
  // --8<-- [start:switch]
  // Each call is one 8-bit write of STATUS[31:24] (ONCH): two short SPI transfers.
  if (driver.EnableChannel(0) != DriverStatus::OK) return false;
  if (driver.SetChannelsOn(0b0000'0101) != DriverStatus::OK) return false;   // channels 0 and 2 on
  if (driver.DisableAllChannels() != DriverStatus::OK) return false;
  // --8<-- [end:switch]
  return true;
}

bool hold_update(Driver& driver) {
  // --8<-- [start:hold]
  // Change one setting of a running channel: read-modify-write of CFG_CH0 in real units.
  if (driver.SetHoldCurrentMa(0, 120) != DriverStatus::OK) return false;

  uint32_t hold_ma = 0;
  if (driver.GetHoldCurrentMa(0, hold_ma) == DriverStatus::OK) {
    std::printf("hold %lu mA\n", static_cast<unsigned long>(hold_ma));   // quantised to IFS / 127
  }
  // --8<-- [end:hold]
  return true;
}

bool bridge(Driver& driver) {
  // --8<-- [start:bridge]
  // Channels 2 and 3 as one full bridge (a brushed DC motor or a latching valve).
  // Change CMxy only while both channels are off.
  max22200::StatusConfig status;
  if (driver.ReadStatus(status) != DriverStatus::OK) return false;
  status.channels_on_mask &= static_cast<uint8_t>(~0b0000'1100);
  status.channel_pair_mode_32 = max22200::ChannelMode::HBRIDGE;
  if (driver.WriteStatus(status) != DriverStatus::OK) return false;

  // Forward runs from CFG_CH3, reverse from CFG_CH2 (datasheet Table 7).
  if (driver.ConfigureChannelCdr(3, 500, 300, 10.0f) != DriverStatus::OK) return false;
  if (driver.ConfigureChannelCdr(2, 500, 300, 10.0f) != DriverStatus::OK) return false;

  const uint8_t pair = 1;                         // 0: CH0/CH1, 1: CH2/CH3, 2: CH4/CH5, 3: CH6/CH7
  if (driver.SetFullBridgeState(pair, max22200::FullBridgeState::Forward) != DriverStatus::OK) return false;
  return driver.SetFullBridgeState(pair, max22200::FullBridgeState::Brake) == DriverStatus::OK;
  // --8<-- [end:bridge]
}

void faults(Driver& driver) {
  // --8<-- [start:faults]
  bool pin_active = false;                        // FAULT pin, read through the port
  driver.GetFaultPinState(pin_active);

  max22200::StatusConfig status;                  // global flags; reading clears UVM and OVT
  if (driver.ReadStatus(status) == DriverStatus::OK && status.hasFault()) {
    if (status.hasUndervoltage()) std::printf("VM dropped below UVLO\n");
    if (status.hasOvertemperature()) std::printf("thermal shutdown\n");
  }

  max22200::FaultStatus per_channel;              // which channel, which fault
  if (driver.ReadFaultRegister(per_channel) == DriverStatus::OK && per_channel.hasFault()) {
    for (uint8_t ch = 0; ch < 8; ++ch) {
      if (per_channel.hasOvercurrentOnChannel(ch)) {
        std::printf("OCP on OUT%u: channel turned off\n", ch);
        driver.DisableChannel(ch);                // clear ONCH before turning it on again
      }
    }
    driver.ClearChannelFaults(per_channel.channelsWithAnyFault());   // MAX22200A: only these bits
  }

  if (driver.GetLastFaultByte() == max22200::StatusReg::FAULT_BYTE_COMER) {
    std::printf("last command byte hit a communication error\n");
  }
  // --8<-- [end:faults]
}

void on_fault(uint8_t channel, max22200::FaultType type, void* /*user_data*/) {
  std::printf("fault %s on channel %u\n", max22200::FaultTypeToStr(type), channel);   // 0xFF: chip-wide
}

void fault_callback(Driver& driver) {
  // --8<-- [start:fault-callback]
  driver.SetFaultCallback(&on_fault, nullptr);
  max22200::FaultStatus faults;
  driver.ReadFaultRegister(faults);               // calls on_fault() once per set FAULT bit
  // --8<-- [end:fault-callback]
}

bool dpm(Driver& driver) {
  // --8<-- [start:dpm]
  // Per channel: DPM_EN on channels 0 and 1 (low-side only). Do this first: the debounce below is
  // converted with these channels' chopping frequency.
  if (driver.SetDpmEnabledChannels(0b0000'0011) != DriverStatus::OK) return false;
  // Global DPM settings: search for the dip above 150 mA, dip of at least 40 mA lasting 0.4 ms.
  if (driver.ConfigureDpm(150.0f, 40.0f, 0.4f) != DriverStatus::OK) return false;
  // A DPM flag in FAULT[7:0] after a HIT means no dip was seen: the plunger did not move.
  // --8<-- [end:dpm]
  return true;
}

bool raw(Driver& driver) {
  // --8<-- [start:raw]
  uint32_t cfg0 = 0;
  if (driver.ReadRegister32(max22200::RegBank::CFG_CH0, cfg0) != DriverStatus::OK) return false;

  // 8-bit access reaches bits [31:24] only: for CFG_CHx that is HFS and HOLD[6:0].
  const uint8_t hold_raw = 40;                    // 40 / 127 x IFS
  return driver.WriteRegister8(max22200::RegBank::CFG_CH0,
                               static_cast<uint8_t>((cfg0 >> 24) & 0x80u) | hold_raw) == DriverStatus::OK;
  // --8<-- [end:raw]
}

void stats(Driver& driver) {
  // --8<-- [start:stats]
  const max22200::DriverStatistics s = driver.GetStatistics();
  std::printf("%lu transfers, %lu failed (%.1f %% ok)\n", static_cast<unsigned long>(s.total_transfers),
              static_cast<unsigned long>(s.failed_transfers), static_cast<double>(s.getSuccessRate()));
  // --8<-- [end:stats]
}
