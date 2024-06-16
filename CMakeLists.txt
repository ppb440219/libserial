# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

project ("libserial")

add_executable (libserial "test.cpp"  "windows/win_serial.h" "windows/win_serial.cpp" "windows/serial_data.h" "log.h" "serial_factory.h" "serial_intf.h")
target_include_directories(libserial PRIVATE ".")
