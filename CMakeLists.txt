cmake_minimum_required (VERSION 3.8)

project (libserial)

set(SOURCE_LIST "serial_factory.h" "serial_intf.h" "serial_data.h" "serial_err.h" "log.h")

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	set(SOURCE_LIST  "windows/win_serial.h" "windows/win_serial.cpp")
endif()

add_executable (serial_tester "test_main.cpp" )

target_link_libraries(serial_tester libserial)