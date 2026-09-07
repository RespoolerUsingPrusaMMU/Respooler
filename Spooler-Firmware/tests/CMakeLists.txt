cmake_minimum_required(VERSION 3.20)

project(SpoolerFirmwareTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

find_package(GTest REQUIRED)
include(GoogleTest)

add_executable(
  spooler_rewinder_tests
  src/RewinderTest.cc
  ../src/app/Rewinder.cc
)

# IMPORTANT: fake headers must precede ../inc.  Rewinder.cc is therefore the
# real production state-machine implementation, while Board/EEPROM/Debug are
# deterministic host-side test doubles.
target_include_directories(
  spooler_rewinder_tests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/fakes
    ${CMAKE_CURRENT_SOURCE_DIR}/../inc
)

target_compile_definitions(
  spooler_rewinder_tests
  PRIVATE
    SPOOLER_DEBUG
)

target_compile_options(
  spooler_rewinder_tests
  PRIVATE
    -Wall
    -Wextra
    -Wsign-compare
)

target_link_libraries(
  spooler_rewinder_tests
  PRIVATE
    GTest::gtest_main
)

gtest_discover_tests(spooler_rewinder_tests)
