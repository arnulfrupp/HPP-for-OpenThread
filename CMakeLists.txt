#
# Halloween
#

cmake_minimum_required(VERSION 3.13.1)

list(INSERT OVERLAY_CONFIG 0 $ENV{ZEPHYR_BASE}/../nrf/samples/openthread/common/overlay-ot-defaults.conf)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(halloween)

target_sources(app PRIVATE src/main.c)
target_sources(app PRIVATE src/hppVarStorage.c)
target_sources(app PRIVATE src/hppParser.c)
target_sources(app PRIVATE src/hppThread.c)
target_sources(app PRIVATE src/hppZephyr.c)
target_sources_ifdef(CONFIG_HPP_NRF52840 app PRIVATE src/hppNRF52840.c)

#target_sources_ifdef(CONFIG_BT_NUS app PRIVATE src/ble_utils.c)
