/**
 * @file ports.cpp
 * @brief Instantiates the driver over the documented SPI port, so every member compiles against it.
 */
#include "my_spi.hpp"

template class max22200::MAX22200<MySpi>;
