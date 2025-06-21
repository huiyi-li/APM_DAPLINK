cmake_minimum_required(VERSION 3.21)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME firmware)

# Include toolchain file
include("cmake/gcc-arm-none-eabi.cmake")

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/apm32_cmake)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)
# 自动收集标准外设驱动源文件
file(GLOB THREADX_SRC "${CMAKE_SOURCE_DIR}/third/threadx/common/src/*.c")
file(GLOB THREADX_ASM "${CMAKE_SOURCE_DIR}/third/threadx/ports/cortex_m4/gnu/src/*.S")
# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    ${THREADX_SRC}
    ${THREADX_ASM}
    ${CMAKE_SOURCE_DIR}/bsp/src/bsp_sysclk.c
    ${CMAKE_SOURCE_DIR}/bsp/src/bsp_printf.c
    ${CMAKE_SOURCE_DIR}/User/tx_initialize_low_level.S
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    ${CMAKE_SOURCE_DIR}/third/threadx/common/inc
    ${CMAKE_SOURCE_DIR}/third/threadx/ports/cortex_m4/gnu/inc
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    apm32_cmake

    # Add user defined libraries
)

# Generate hex file
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex ${CMAKE_PROJECT_NAME}.elf ${CMAKE_PROJECT_NAME}.hex
    COMMENT "Generating HEX file"
)

# Generate bin file
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary ${CMAKE_PROJECT_NAME}.elf ${CMAKE_PROJECT_NAME}.bin
    COMMENT "Generating BIN file"
)

# Custom target to generate hex and bin files
add_custom_target(genfiles
    DEPENDS ${CMAKE_PROJECT_NAME}
    COMMAND ${CMAKE_OBJCOPY} -O ihex ${CMAKE_PROJECT_NAME}.elf ${CMAKE_PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary ${CMAKE_PROJECT_NAME}.elf ${CMAKE_PROJECT_NAME}.bin
    COMMENT "Generating HEX and BIN files"
)

# Copy compile_commands.json to the project directory
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_BINARY_DIR}/compile_commands.json ${CMAKE_SOURCE_DIR}/compile_commands.json
    COMMENT "Copying compile_commands.json to project directory"
)

