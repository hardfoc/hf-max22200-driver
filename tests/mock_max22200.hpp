/**
 * @file mock_max22200.hpp
 * @brief A model of the MAX22200's SPI behaviour, used as the driver's SpiInterface in the host tests.
 *
 * It follows the datasheet (Rev 1, 3/25): a transfer with CMD high is the 8-bit Command Register and
 * returns STATUS[7:0]; with CMD low the transfer is the data phase the last command selected, 8 or 32
 * bits. 32-bit writes arrive LSB byte first (Figure 10), 32-bit reads leave MSB byte first (Figure 11),
 * 8-bit accesses touch bits [31:24] (Figures 12 and 13). Reading FAULT clears the bits whose SDI bit
 * is high (MAX22200A selective clear on read).
 */
#pragma once

#include "max22200_spi_interface.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

class MockMax22200 : public max22200::SpiInterface<MockMax22200> {
public:
  // ---- What a test sets up ----
  bool init_ok = true;          ///< Initialize() result
  bool fault_pin_low = false;   ///< level of the FAULT pin (low = fault)
  int comer_replies = 0;        ///< answer this many command writes with STATUS[7:0] = 0x04 (COMER)

  // ---- What a test inspects ----
  std::array<uint32_t, 11> regs{};   ///< STATUS, CFG_CH0..7, FAULT, CFG_DPM
  std::vector<uint8_t> commands;     ///< every Command Register byte received
  std::vector<uint32_t> command_delay_us;  ///< delay_us_total when each command byte arrived
  bool enable = false;               ///< ENABLE pin, as the driver left it
  bool cmd = false;                  ///< CMD pin
  uint32_t delay_us_total = 0;       ///< sum of DelayUs() calls
  int configure_mode = -1;           ///< SPI mode passed to Configure()
  uint32_t configure_hz = 0;         ///< SPI clock passed to Configure()
  uint8_t last_fault_sdi[4] = {};    ///< SDI bytes of the last FAULT read

  MockMax22200() {
    regs[0] = 0x00040002u;   // power-up STATUS: M_COMF = 1, UVM = 1
  }

  // ---- SpiInterface ----
  bool Initialize() { return init_ok; }
  bool Configure(uint32_t speed_hz, uint8_t mode, bool msb_first) {
    configure_hz = speed_hz;
    configure_mode = mode;
    return msb_first;
  }
  bool IsReady() const { return true; }
  void SetChipSelect(bool) {}
  void DelayUs(uint32_t us) { delay_us_total += us; }

  bool enable_pin_fails = false;  ///< fault injection: GpioSet(ENABLE) fails, the pin keeps its level
  bool cmd_pin_fails = false;     ///< fault injection: GpioSet(CMD) fails, the pin keeps its level

  bool GpioSet(max22200::CtrlPin pin, max22200::GpioSignal signal) {
    const bool active = signal == max22200::GpioSignal::ACTIVE;
    if (pin == max22200::CtrlPin::ENABLE) {
      if (enable_pin_fails) return false;
      enable = active;
    }
    if (pin == max22200::CtrlPin::CMD) {
      if (cmd_pin_fails) return false;
      cmd = active;
    }
    return true;
  }

  bool GpioRead(max22200::CtrlPin pin, max22200::GpioSignal& signal) {
    if (pin != max22200::CtrlPin::FAULT) return false;
    signal = fault_pin_low ? max22200::GpioSignal::ACTIVE : max22200::GpioSignal::INACTIVE;
    return true;
  }

  bool Transfer(const uint8_t* tx, uint8_t* rx, std::size_t len) {
    if (!enable || len == 0) return false;
    if (cmd) {                                   // Command Register write
      if (len != 1) return false;
      commands.push_back(tx[0]);
      command_delay_us.push_back(delay_us_total);
      bank_ = (tx[0] >> 1) & 0x0Fu;
      write_ = (tx[0] & 0x80u) != 0;
      mode8_ = (tx[0] & 0x01u) != 0;
      if (comer_replies > 0) {
        --comer_replies;
        rx[0] = 0x04u;
      } else {
        rx[0] = static_cast<uint8_t>(regs[0] & 0xFFu);
      }
      return true;
    }
    if (bank_ > 0x0Au) return false;
    uint32_t& reg = regs[bank_];
    if (mode8_) {
      if (len != 1) return false;
      if (write_) {
        reg = (reg & 0x00FFFFFFu) | (static_cast<uint32_t>(tx[0]) << 24);
        rx[0] = static_cast<uint8_t>(regs[0] & 0xFFu);
      } else {
        rx[0] = static_cast<uint8_t>(reg >> 24);
      }
      return true;
    }
    if (len != 4) return false;
    if (write_) {
      uint32_t v = static_cast<uint32_t>(tx[0]) | (static_cast<uint32_t>(tx[1]) << 8) |
                   (static_cast<uint32_t>(tx[2]) << 16) | (static_cast<uint32_t>(tx[3]) << 24);
      if (bank_ == 0) {                          // flag bits [7:1] are read-only
        v = (v & 0xFFFFFF01u) | (reg & 0x000000FEu);
      }
      reg = v;
      rx[0] = static_cast<uint8_t>(regs[0] & 0xFFu);
      rx[1] = commands.empty() ? 0 : commands.back();
      rx[2] = 0;
      rx[3] = 0;
      return true;
    }
    for (int i = 0; i < 4; ++i) {
      rx[i] = static_cast<uint8_t>(reg >> (24 - 8 * i));
    }
    if (bank_ == 0) {
      regs[0] &= ~0x00000082u;                   // reading STATUS clears UVM and OVT
    }
    if (bank_ == 9) {                            // selective clear on read
      uint32_t clear = 0;
      for (int i = 0; i < 4; ++i) {
        last_fault_sdi[i] = tx[i];
        clear |= static_cast<uint32_t>(tx[i]) << (24 - 8 * i);
      }
      reg &= ~clear;
    }
    return true;
  }

private:
  uint8_t bank_ = 0;
  bool write_ = false;
  bool mode8_ = false;
};
