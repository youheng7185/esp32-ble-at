# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/lapchong/esp/v5.3.1/esp-idf/components/bootloader/subproject"
  "/home/lapchong/cpp/stm32/esp32-ble-at/build/bootloader"
  "/home/lapchong/cpp/stm32/esp32-ble-at/build/bootloader-prefix"
  "/home/lapchong/cpp/stm32/esp32-ble-at/build/bootloader-prefix/tmp"
  "/home/lapchong/cpp/stm32/esp32-ble-at/build/bootloader-prefix/src/bootloader-stamp"
  "/home/lapchong/cpp/stm32/esp32-ble-at/build/bootloader-prefix/src"
  "/home/lapchong/cpp/stm32/esp32-ble-at/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/lapchong/cpp/stm32/esp32-ble-at/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/lapchong/cpp/stm32/esp32-ble-at/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
