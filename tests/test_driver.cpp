/**
 * @file test_driver.cpp
 * @brief Driver behaviour over the chip model: initialisation, command bytes, channel registers,
 *        channel switching, faults and DPM.
 */
#include "mock_max22200.hpp"
#include "test_harness.hpp"

#include "max22200.hpp"

using max22200::DriverStatus;
using Driver = max22200::MAX22200<MockMax22200>;

namespace {
constexpr uint32_t kStatus = 0;
constexpr uint32_t kFault = 9;
constexpr uint32_t kCfgDpm = 10;

// IFS = 15 kOhm-equivalent: KFS 15k / RREF 15 kOhm = 1000 mA.
max22200::BoardConfig board1A() { return max22200::BoardConfig(15.0f, false); }
} // namespace

TEST_CASE("Command bytes follow Table 9: RB/W, A_BNK[4:1], 8bit/n32") {
  using max22200::CommandReg::build;
  namespace bank = max22200::RegBank;
  CHECK_EQ(build(bank::STATUS, false, false), 0x00);
  CHECK_EQ(build(bank::STATUS, true, false), 0x80);
  CHECK_EQ(build(bank::STATUS, true, true), 0x81);
  CHECK_EQ(build(bank::CFG_CH0, true, false), 0x82);
  CHECK_EQ(build(bank::CFG_CH7, false, false), 0x10);
  CHECK_EQ(build(bank::FAULT, false, false), 0x12);
  CHECK_EQ(build(bank::CFG_DPM, false, false), 0x14);
}

TEST_CASE("Initialize follows the programming flow chart and sets ACTIVE") {
  MockMax22200 chip;
  Driver driver(chip, board1A());
  REQUIRE(driver.Initialize() == DriverStatus::OK);
  CHECK(driver.IsInitialized());
  CHECK(chip.enable);
  CHECK(!chip.cmd);                                 // CMD back low after every command
  CHECK(chip.delay_us_total >= 500u);               // tEN = 0.5 ms before the first access
  CHECK_EQ(chip.configure_mode, 0);                 // SPI mode 0
  CHECK_EQ(chip.configure_hz, 10000000u);
  REQUIRE(chip.commands.size() == 3u);              // read STATUS, write STATUS, read STATUS
  CHECK_EQ(chip.commands[0], 0x00);
  CHECK_EQ(chip.commands[1], 0x80);
  CHECK_EQ(chip.commands[2], 0x00);
  CHECK_EQ(chip.regs[kStatus] & 0x01u, 1u);         // ACTIVE
  CHECK_EQ(chip.regs[kStatus] >> 24, 0u);           // every channel off
  CHECK_EQ((chip.regs[kStatus] >> 18) & 1u, 1u);    // M_COMF kept at its reset value
}

TEST_CASE("Initialize gives up after three COMER replies and releases ENABLE") {
  MockMax22200 chip;
  chip.comer_replies = 100;
  Driver driver(chip, board1A());
  CHECK(driver.Initialize() == DriverStatus::COMMUNICATION_ERROR);
  CHECK(!driver.IsInitialized());
  CHECK(!chip.enable);
  CHECK_EQ(chip.commands.size(), 3u);
}

TEST_CASE("Initialize reports a bus that does not come up") {
  MockMax22200 chip;
  chip.init_ok = false;
  Driver driver(chip);
  CHECK(driver.Initialize() == DriverStatus::INITIALIZATION_ERROR);
  CHECK(chip.commands.empty());
}

TEST_CASE("ConfigureChannel encodes CDR currents and HIT time, LSB byte first on SDI") {
  MockMax22200 chip;
  Driver driver(chip, board1A());
  REQUIRE(driver.Initialize() == DriverStatus::OK);
  const auto cfg = max22200::ChannelConfig::makeSolenoidCdr(500.0f, 200.0f, 10.0f);
  REQUIRE(driver.ConfigureChannel(0, cfg) == DriverStatus::OK);
  CHECK_EQ(chip.commands.back(), 0x82);
  // HOLD 200/1000*127 = 25, HIT 500/1000*127 = 64, HIT_T 10 ms * 25 kHz / 40 = 6, FREQ_CFG 00.
  CHECK_EQ(chip.regs[1], 0x19400600u);

  max22200::ChannelConfig back;
  REQUIRE(driver.GetChannelConfig(0, back) == DriverStatus::OK);
  CHECK(back.isCdr());
  CHECK(back.hit_setpoint > 500.0f && back.hit_setpoint < 505.0f);   // 64/127 * 1000
  CHECK(back.hit_time_ms > 9.5f && back.hit_time_ms < 9.7f);         // 6 * 40 / 25 kHz = 9.6 ms
}

TEST_CASE("ConfigureChannel rejects CDR without IFS, SRC above 50 kHz and channel 8") {
  MockMax22200 chip;
  Driver driver(chip, board1A());
  REQUIRE(driver.Initialize() == DriverStatus::OK);
  const auto before = chip.commands.size();

  max22200::BoardConfig no_ifs;
  no_ifs.full_scale_current_ma = 0;
  driver.SetBoardConfig(no_ifs);
  auto cfg = max22200::ChannelConfig::makeSolenoidCdr(500.0f, 200.0f, 10.0f);
  CHECK(driver.ConfigureChannel(0, cfg) == DriverStatus::INVALID_PARAMETER);

  driver.SetBoardConfig(board1A());
  cfg.slew_rate_control_enabled = true;
  cfg.chop_freq = max22200::ChopFreq::FMAIN_DIV2;   // 50 kHz with FREQM = 0
  CHECK(driver.ConfigureChannel(0, cfg) == DriverStatus::INVALID_PARAMETER);

  CHECK(driver.ConfigureChannel(8, max22200::ChannelConfig{}) == DriverStatus::INVALID_PARAMETER);
  CHECK(driver.EnableChannel(8) == DriverStatus::INVALID_PARAMETER);
  CHECK_EQ(chip.commands.size(), before);           // nothing reached the bus
}

TEST_CASE("Channel on/off is one 8-bit write of STATUS[31:24]") {
  MockMax22200 chip;
  Driver driver(chip, board1A());
  REQUIRE(driver.Initialize() == DriverStatus::OK);
  REQUIRE(driver.EnableChannel(3) == DriverStatus::OK);
  CHECK_EQ(chip.commands.back(), 0x81);
  CHECK_EQ(chip.regs[kStatus] >> 24, 0x08u);
  REQUIRE(driver.EnableChannel(5) == DriverStatus::OK);
  REQUIRE(driver.DisableChannel(3) == DriverStatus::OK);
  CHECK_EQ(chip.regs[kStatus] >> 24, 0x20u);
  CHECK_EQ(chip.regs[kStatus] & 0x01u, 1u);         // the rest of STATUS untouched
  REQUIRE(driver.SetChannelsOn(0x81) == DriverStatus::OK);
  CHECK_EQ(chip.regs[kStatus] >> 24, 0x81u);
}

TEST_CASE("SetFullBridgeState sets ONCHx / ONCHy per Table 7") {
  MockMax22200 chip;
  Driver driver(chip, board1A());
  REQUIRE(driver.Initialize() == DriverStatus::OK);
  REQUIRE(driver.EnableChannel(0) == DriverStatus::OK);
  // Pair 1 is channels 2 (x) and 3 (y).
  REQUIRE(driver.SetFullBridgeState(1, max22200::FullBridgeState::Forward) == DriverStatus::OK);
  CHECK_EQ(chip.regs[kStatus] >> 24, 0x05u);        // ONCHx = 1, channel 0 still on
  REQUIRE(driver.SetFullBridgeState(1, max22200::FullBridgeState::Reverse) == DriverStatus::OK);
  CHECK_EQ(chip.regs[kStatus] >> 24, 0x09u);        // ONCHy = 1
  REQUIRE(driver.SetFullBridgeState(1, max22200::FullBridgeState::Brake) == DriverStatus::OK);
  CHECK_EQ(chip.regs[kStatus] >> 24, 0x0Du);
  CHECK(driver.SetFullBridgeState(4, max22200::FullBridgeState::HiZ) == DriverStatus::INVALID_PARAMETER);
}

TEST_CASE("FAULT is decoded per channel and cleared selectively") {
  MockMax22200 chip;
  Driver driver(chip, board1A());
  REQUIRE(driver.Initialize() == DriverStatus::OK);
  chip.regs[kFault] = (1u << 28) | (1u << 9);       // OCP on channel 4, OLF on channel 1

  max22200::FaultStatus faults;
  REQUIRE(driver.ReadFaultRegister(faults) == DriverStatus::OK);
  CHECK_EQ(chip.commands.back(), 0x12);
  CHECK_EQ(faults.overcurrent_channel_mask, 0x10);
  CHECK_EQ(faults.open_load_fault_channel_mask, 0x02);
  CHECK(faults.hasOvercurrentOnChannel(4));
  CHECK_EQ(faults.getFaultCount(), 2u);

  REQUIRE(driver.ClearChannelFaults(0x10) == DriverStatus::OK);
  CHECK_EQ(chip.last_fault_sdi[0], 0x10);           // SDI high only for channel 4
  CHECK_EQ(chip.regs[kFault], 1u << 9);             // channel 1's open-load flag kept
}

TEST_CASE("GetFaultPinState reads FAULT through the port") {
  MockMax22200 chip;
  Driver driver(chip, board1A());
  bool active = true;
  REQUIRE(driver.GetFaultPinState(active) == DriverStatus::OK);
  CHECK(!active);
  chip.fault_pin_low = true;
  REQUIRE(driver.GetFaultPinState(active) == DriverStatus::OK);
  CHECK(active);
}

TEST_CASE("ConfigureDpm converts mA and ms into CFG_DPM fields") {
  MockMax22200 chip;
  Driver driver(chip, board1A());
  REQUIRE(driver.Initialize() == DriverStatus::OK);
  REQUIRE(driver.ConfigureDpm(300.0f, 50.0f, 0.4f) == DriverStatus::OK);
  // ISTART 300/1000*127 = 38, IPTH 50/1000*127 = 6, TDEB 0.4 ms * 25 kHz = 10 periods.
  CHECK_EQ(chip.regs[kCfgDpm], (38u << 8) | (10u << 4) | 6u);
}

TEST_CASE("Deinitialize clears ACTIVE and releases ENABLE") {
  MockMax22200 chip;
  Driver driver(chip, board1A());
  REQUIRE(driver.Initialize() == DriverStatus::OK);
  REQUIRE(driver.EnableChannel(0) == DriverStatus::OK);
  REQUIRE(driver.Deinitialize() == DriverStatus::OK);
  CHECK(!driver.IsInitialized());
  CHECK(!chip.enable);
  CHECK_EQ(chip.regs[kStatus] & 0x01u, 0u);
  CHECK_EQ(chip.regs[kStatus] >> 24, 0u);
}

TEST_CASE("VDR duty is written as percent per Table 3 (40 % -> HIT[6:0] = 40)") {
  MockMax22200 chip;
  Driver driver(chip, board1A());
  REQUIRE(driver.Initialize() == DriverStatus::OK);
  const auto cfg = max22200::ChannelConfig::makeSolenoidVdr(40.0f, 25.0f, 0.0f);
  REQUIRE(driver.ConfigureChannel(0, cfg) == DriverStatus::OK);
  CHECK_EQ((chip.regs[1] >> 16) & 0x7Fu, 40u);      // HIT[6:0]
  CHECK_EQ((chip.regs[1] >> 24) & 0x7Fu, 25u);      // HOLD[6:0]
  CHECK_EQ((chip.regs[1] >> 7) & 1u, 1u);           // VDRnCDR

  max22200::ChannelConfig back;
  REQUIRE(driver.GetChannelConfig(0, back) == DriverStatus::OK);
  CHECK(back.isVdr());
  CHECK(back.hit_duty_percent() > 39.9f && back.hit_duty_percent() < 40.1f);
  CHECK(back.hold_duty_percent() > 24.9f && back.hold_duty_percent() < 25.1f);

  // 100 % and above is full duty; a raw value of 100..127 reads back as 100 %.
  chip.regs[1] = (chip.regs[1] & ~(0x7Fu << 16)) | (110u << 16);
  REQUIRE(driver.GetChannelConfig(0, back) == DriverStatus::OK);
  CHECK(back.hit_duty_percent() > 99.9f && back.hit_duty_percent() < 100.1f);
}

TEST_CASE("Current setters do not switch a VDR channel to CDR, duty setters not CDR to VDR") {
  MockMax22200 chip;
  Driver driver(chip, board1A());
  REQUIRE(driver.Initialize() == DriverStatus::OK);

  REQUIRE(driver.ConfigureChannelVdr(0, 60.0f, 30.0f, 0.0f) == DriverStatus::OK);
  const uint32_t vdr_reg = chip.regs[1];
  CHECK(driver.SetHoldCurrentMa(0, 200) == DriverStatus::INVALID_PARAMETER);
  CHECK(driver.SetHitCurrentMa(0, 200) == DriverStatus::INVALID_PARAMETER);
  CHECK(driver.SetHitCurrentA(0, 0.2f) == DriverStatus::INVALID_PARAMETER);
  CHECK(driver.SetHoldCurrentPercent(0, 20.0f) == DriverStatus::INVALID_PARAMETER);
  CHECK_EQ(chip.regs[1], vdr_reg);                  // VDRnCDR and both duties untouched
  REQUIRE(driver.SetHoldDutyPercent(0, 25.0f) == DriverStatus::OK);
  CHECK_EQ((chip.regs[1] >> 7) & 1u, 1u);
  CHECK_EQ((chip.regs[1] >> 24) & 0x7Fu, 25u);

  REQUIRE(driver.ConfigureChannelCdr(1, 500, 200, 10.0f) == DriverStatus::OK);
  const uint32_t cdr_reg = chip.regs[2];
  CHECK(driver.SetHitDutyPercent(1, 50.0f) == DriverStatus::INVALID_PARAMETER);
  CHECK(driver.SetHoldDutyPercent(1, 50.0f) == DriverStatus::INVALID_PARAMETER);
  CHECK_EQ(chip.regs[2], cdr_reg);
  REQUIRE(driver.SetHoldCurrentMa(1, 100) == DriverStatus::OK);
  CHECK_EQ((chip.regs[2] >> 7) & 1u, 0u);
}

TEST_CASE("Initialize waits tWU = 2.5 ms after ACTIVE = 1 before it returns") {
  MockMax22200 chip;
  Driver driver(chip, board1A());
  REQUIRE(driver.Initialize() == DriverStatus::OK);
  REQUIRE(chip.commands.size() == 3u);
  CHECK(chip.command_delay_us[0] >= 500u);                          // tEN before the first access
  CHECK(chip.command_delay_us[2] - chip.command_delay_us[1] >= 2500u);   // tWU after the ACTIVE write
  CHECK(chip.delay_us_total >= 3000u);
}
