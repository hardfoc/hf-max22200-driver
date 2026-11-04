/**
 * @file test_gpio_errors.cpp
 * @brief A control pin the port could not drive is reported. ENABLE: datasheet Figure 6 (ENABLE high,
 * tEN, then SPI) and the shutdown path; CMD: the pin must be high for the Command Register and low
 * for the data phase, so a frame sent with CMD in the wrong state is not the access the driver meant.
 */
#include "mock_max22200.hpp"
#include "test_harness.hpp"

#include "max22200.hpp"

#include <cstring>
#include <type_traits>
#include <utility>

using max22200::DriverStatus;
using Driver = max22200::MAX22200<MockMax22200>;

// The port contract: GpioSet() returns whether the pin was driven.
static_assert(std::is_same_v<decltype(std::declval<max22200::SpiInterface<MockMax22200>&>().GpioSet(
                                 max22200::CtrlPin::ENABLE, max22200::GpioSignal::ACTIVE)),
                             bool>,
              "SpiInterface::GpioSet() must return bool");

TEST_CASE("Initialize fails when ENABLE cannot be raised, before any SPI access") {
  MockMax22200 chip;
  chip.enable_pin_fails = true;
  Driver driver(chip);
  CHECK(driver.Initialize() == DriverStatus::INITIALIZATION_ERROR);
  CHECK(!driver.IsInitialized());
  CHECK(chip.commands.empty());
}

TEST_CASE("A command whose CMD pin cannot be raised is not sent") {
  MockMax22200 chip;
  Driver driver(chip);
  REQUIRE(driver.Initialize() == DriverStatus::OK);
  const auto sent = chip.commands.size();
  chip.cmd_pin_fails = true;
  max22200::StatusConfig status;
  CHECK(driver.ReadStatus(status) == DriverStatus::GPIO_ERROR);
  CHECK_EQ(chip.commands.size(), sent);
}

TEST_CASE("Initialize reports a CMD pin that cannot be driven") {
  MockMax22200 chip;
  chip.cmd_pin_fails = true;
  Driver driver(chip);
  CHECK(driver.Initialize() == DriverStatus::GPIO_ERROR);
  CHECK(!driver.IsInitialized());
  CHECK(chip.commands.empty());
}

TEST_CASE("DisableDevice and EnableDevice report an ENABLE pin that cannot be driven") {
  MockMax22200 chip;
  Driver driver(chip);
  REQUIRE(driver.Initialize() == DriverStatus::OK);
  chip.enable_pin_fails = true;
  CHECK(driver.DisableDevice() == DriverStatus::GPIO_ERROR);
  CHECK(chip.enable); // still enabled: the caller must know
  CHECK(driver.EnableDevice() == DriverStatus::GPIO_ERROR);
}

TEST_CASE("Deinitialize reports an ENABLE pin that cannot be lowered") {
  MockMax22200 chip;
  Driver driver(chip);
  REQUIRE(driver.Initialize() == DriverStatus::OK);
  chip.enable_pin_fails = true;
  CHECK(driver.Deinitialize() == DriverStatus::GPIO_ERROR);
  CHECK(chip.enable);
}

TEST_CASE("GPIO_ERROR has a name") {
  CHECK(std::strcmp(max22200::DriverStatusToStr(DriverStatus::GPIO_ERROR), "GPIO_ERROR") == 0);
}
