/**
 * @file my_spi.hpp
 * @brief The SPI port shown on the documentation's Porting and Quickstart pages, compiled by the host build.
 *
 * Every platform call goes through the `platform_*` functions declared below; a real port replaces them with
 * its SDK (ESP-IDF, STM32 HAL, Zephyr, Linux spidev). The class is what the driver needs, nothing more.
 */
#pragma once

#include "max22200.hpp"

#include <cstddef>
#include <cstdint>

// Platform hooks: your SDK's calls. Declared only, so the docs build checks the port compiles.
bool platform_spi_init(uint32_t clock_hz, uint8_t mode) noexcept;
bool platform_spi_transfer(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept;  // CS low .. CS high
bool platform_gpio_write(int gpio, bool level) noexcept;  // false on a GPIO error
bool platform_gpio_read(int gpio) noexcept;
void platform_delay_us(uint32_t us) noexcept;

// --8<-- [start:port]
class MySpi : public max22200::SpiInterface<MySpi> {
public:
  // Board wiring: ENABLE and CMD are MCU outputs, FAULT is an input with a pull-up.
  static constexpr int kEnablePin = 4;
  static constexpr int kCmdPin = 5;
  static constexpr int kFaultPin = 6;

  // Cap the clock: 1 MHz for bring-up, at most 10 MHz alone or 5 MHz in a daisy chain.
  explicit MySpi(uint32_t max_clock_hz = 10000000) : max_clock_hz_(max_clock_hz) {}

  bool Initialize() { return true; }

  // The driver calls this once from Initialize(): 10 MHz, mode 0, MSB first.
  bool Configure(uint32_t speed_hz, uint8_t mode, bool msb_first) {
    const uint32_t hz = speed_hz < max_clock_hz_ ? speed_hz : max_clock_hz_;
    return mode == 0 && msb_first && platform_spi_init(hz, mode);
  }

  bool IsReady() const { return true; }

  // One blocking, full-duplex transaction: CS low, `length` bytes, CS high, then return.
  bool Transfer(const uint8_t* tx, uint8_t* rx, std::size_t length) {
    return platform_spi_transfer(tx, rx, length);
  }

  void SetChipSelect(bool) {}   // Transfer() owns CS; the driver never calls this

  void DelayUs(uint32_t us) { platform_delay_us(us); }

  // ENABLE and CMD are active high, FAULT is active low (open drain). Return false when the pin
  // could not be driven: the driver then fails the call with GPIO_ERROR.
  bool GpioSet(max22200::CtrlPin pin, max22200::GpioSignal signal) {
    const bool high = signal == max22200::GpioSignal::ACTIVE;
    if (pin == max22200::CtrlPin::ENABLE) return platform_gpio_write(kEnablePin, high);
    if (pin == max22200::CtrlPin::CMD) return platform_gpio_write(kCmdPin, high);
    return false;  // FAULT is an input
  }

  bool GpioRead(max22200::CtrlPin pin, max22200::GpioSignal& signal) {
    if (pin != max22200::CtrlPin::FAULT) return false;
    signal = platform_gpio_read(kFaultPin) ? max22200::GpioSignal::INACTIVE
                                           : max22200::GpioSignal::ACTIVE;
    return true;
  }

private:
  uint32_t max_clock_hz_;
};
// --8<-- [end:port]
