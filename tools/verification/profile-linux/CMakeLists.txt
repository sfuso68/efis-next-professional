cmake_minimum_required(VERSION 3.24)
project(profile_verification LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(EFIS_BUILD_QT_RENDERER OFF CACHE BOOL "" FORCE)
set(EFIS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(/source core-build)
find_package(Qt6 REQUIRED COMPONENTS Core Network)
set(CMAKE_AUTOMOC ON)
add_executable(profile_test
  /source/tests/pfd_aircraft_profile_tests.cpp
  /source/src/renderers/qt/pfd_controller.cpp
  /source/src/renderers/qt/pfd_controller.hpp
  /source/src/setup/operational_profile_model.cpp
  /source/src/setup/operational_profile_model.hpp)
target_include_directories(profile_test PRIVATE /source/src/renderers/qt /source/src/setup)
target_link_libraries(profile_test PRIVATE efis_core Qt6::Core Qt6::Network)
