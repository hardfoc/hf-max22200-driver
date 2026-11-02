/**
 * @file quickstart.cpp
 * @brief The Quickstart page's example, compiled by the host build (never run).
 */
#include "my_spi.hpp"

// --8<-- [start:quickstart]
#include "max22200.hpp"

int pull_in_solenoid_on_out0(MySpi& spi) {
  // 1. Board: RREF = 30 kOhm on IREF, so IFS = 15k x 1 V / 30 kOhm = 500 mA.
  max22200::MAX22200<MySpi> driver(spi, max22200::BoardConfig(30.0f, false));

  if (driver.Initialize() != max22200::DriverStatus::OK) {   // 2. ENABLE, STATUS, ACTIVE = 1
    return -1;                                               // wiring, SPI mode, VM
  }

  // 3. Channel 0: low-side CDR, 400 mA for 20 ms to pull in, then 150 mA to hold.
  const auto valve = max22200::ChannelConfig::makeSolenoidCdr(400.0f, 150.0f, 20.0f);
  if (driver.ConfigureChannel(0, valve) != max22200::DriverStatus::OK) {
    return -2;
  }
  return driver.EnableChannel(0) == max22200::DriverStatus::OK ? 0 : -3;   // 4. on
}
// --8<-- [end:quickstart]
