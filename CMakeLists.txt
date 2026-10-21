cmake_minimum_required(VERSION 3.15)
project(sonic_rs_c C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# On Windows with MSVC, ensure dynamic CRT (/MD) is used in Release & Debug to match Rust's default std CRT
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
    add_compile_options(/W4 /D_CRT_SECURE_NO_WARNINGS)
endif()

find_program(CARGO_EXECUTABLE cargo REQUIRED)

set(SONIC_DIST_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/dist/include")
set(SONIC_DIST_LIB_DIR     "${CMAKE_CURRENT_BINARY_DIR}/dist/lib")

if(WIN32 AND MSVC)
    set(CARGO_RAW_STATIC_LIB "${CMAKE_CURRENT_SOURCE_DIR}/target/release/sonic_rs_c.lib")
    set(SONIC_RS_STATIC_LIB  "${SONIC_DIST_LIB_DIR}/sonic_rs_c.lib")
else()
    set(CARGO_RAW_STATIC_LIB "${CMAKE_CURRENT_SOURCE_DIR}/target/release/libsonic_rs_c.a")
    set(SONIC_RS_STATIC_LIB  "${SONIC_DIST_LIB_DIR}/libsonic_rs_c.a")
endif()

set(SONIC_RS_HEADER    "${SONIC_DIST_INCLUDE_DIR}/sonic_rs.h")

add_custom_command(
    OUTPUT "${SONIC_RS_STATIC_LIB}" "${SONIC_RS_HEADER}"
    COMMAND "${CMAKE_COMMAND}" -E env "RUSTFLAGS=-C target-cpu=native" "${CARGO_EXECUTABLE}" build --release
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${SONIC_DIST_INCLUDE_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${SONIC_DIST_LIB_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/include/sonic_rs.h" "${SONIC_RS_HEADER}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CARGO_RAW_STATIC_LIB}" "${SONIC_RS_STATIC_LIB}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/lib.rs"
            "${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml"
            "${CMAKE_CURRENT_SOURCE_DIR}/include/sonic_rs.h"
    COMMENT "Building sonic-rs-c static library via Cargo and staging dist/include + dist/lib"
)

add_custom_target(sonic_rs_cargo ALL
    DEPENDS "${SONIC_RS_STATIC_LIB}" "${SONIC_RS_HEADER}"
)

add_library(sonic_rs_static STATIC IMPORTED GLOBAL)
add_dependencies(sonic_rs_static sonic_rs_cargo)
set_target_properties(sonic_rs_static PROPERTIES
    IMPORTED_LOCATION "${SONIC_RS_STATIC_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

if(WIN32)
    target_link_libraries(sonic_rs_static INTERFACE ws2_32 userenv advapi32 bcrypt ntdll)
elseif(APPLE)
    target_link_libraries(sonic_rs_static INTERFACE pthread dl m "-framework Security" "-framework CoreFoundation")
elseif(UNIX)
    target_link_libraries(sonic_rs_static INTERFACE pthread dl m)
endif()

add_executable(sonic_example examples/example.c)
target_link_libraries(sonic_example PRIVATE sonic_rs_static)

enable_testing()
add_executable(sonic_test_c_api tests/test_c_api.c)
target_link_libraries(sonic_test_c_api PRIVATE sonic_rs_static)
add_test(NAME sonic_test_c_api COMMAND sonic_test_c_api)
