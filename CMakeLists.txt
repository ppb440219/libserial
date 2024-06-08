# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

project ("serial-port-lib")

add_executable (serial-port-lib "serial_port_lib.cpp" "serial_port_lib.h" "windows/win_serial.h" "windows/win_serial.cpp" "windows/serial_data.h")

