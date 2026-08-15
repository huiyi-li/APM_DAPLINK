cmake_minimum_required(VERSION 3.15)

# 必须在 project() 之前设置工具链！
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/cmake/gcc-arm-none-eabi.cmake")

# 设置构建类型（必须在 project 前或通过 cache 设置）
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()

# 设置项目名称和版本
project(APM32PRJ VERSION 1.0.0 LANGUAGES C CXX ASM)

# 设置C标准
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# 启用编译命令导出
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 设置目标处理器架构（仅用于显示）
set(ARCHITECTURE "cortex-m4")
set(FPU_TYPE "fpv4-sp-d16")
set(FPU_ABI "hard")

# 添加编译定义
add_compile_definitions(
    APM32F40X
)

# 创建可执行文件（必须先定义目标！）
add_executable(${CMAKE_PROJECT_NAME})

option(ENABLE_CMSIS "Enable CMSIS support" ON)
option(ENABLE_STD "Enable std support" ON)
option(ENABLE_DEVICE "Enable std support" ON)
# 添加其他目录的规则
add_subdirectory(${CMAKE_SOURCE_DIR}/Libraries)
add_subdirectory(third)
add_subdirectory(${CMAKE_SOURCE_DIR}/bsp)
add_subdirectory(${CMAKE_SOURCE_DIR}/third/CherryRB)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_SOURCE_DIR}/third/lvgl)
# LVGL is big; build it optimized even in Debug builds to fit 512KB flash.
target_compile_options(lvgl PRIVATE -O2)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE apm32_cmsis_headers)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE apm32_device_headers)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE apm32f4xx_stdperiph)
target_link_libraries(bsp PRIVATE apm32_device_headers)
target_link_libraries(bsp PRIVATE apm32_cmsis_headers)
target_link_libraries(bsp PRIVATE apm32f4xx_stdperiph)
target_link_libraries(bsp PRIVATE CherryRB)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE bsp)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE lvgl)

set(THREADX_ARCH "cortex_m4")        # 根据实际MCU架构调整
set(THREADX_TOOLCHAIN "gnu")         # 根据实际工具链调整
set(TX_USER_FILE "${CMAKE_SOURCE_DIR}/User/tx_user.h")
add_subdirectory(third/threadx)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE threadx)

# FileX (FAT filesystem on the W25Q128 SPI flash)
set(FX_USER_FILE "${CMAKE_SOURCE_DIR}/User/fx_user.h")
add_subdirectory(third/filex)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE filex)
target_compile_options(filex PRIVATE -O2)

# 设置操作系统抽象层
set(CONFIG_CHERRYUSB_OSAL "threadx")

# 添加源文件
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/User/main.c
    ${CMAKE_SOURCE_DIR}/User/system_apm32f4xx.c
    ${CMAKE_SOURCE_DIR}/User/apm32f4xx_int.c
    ${CMAKE_SOURCE_DIR}/User/dap_main.c
    ${CMAKE_SOURCE_DIR}/User/display_port.c
    ${CMAKE_SOURCE_DIR}/User/user_usb_init.c
    ${CMAKE_SOURCE_DIR}/drivers/display/st7789.c
    ${CMAKE_SOURCE_DIR}/drivers/display/lcd_text.c
    ${CMAKE_SOURCE_DIR}/User/lvgl_port.c
    ${CMAKE_SOURCE_DIR}/User/lvgl_demo.c
    ${CMAKE_SOURCE_DIR}/User/fx_spi_flash_driver.c
    ${CMAKE_SOURCE_DIR}/User/filex_demo.c
)

# APM32F407 vector table and ThreadX low-level initialization.
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/Libraries/Device/Geehy/APM32F4xx/Source/gcc/startup_apm32f40x.S
    ${CMAKE_SOURCE_DIR}/User/tx_initialize_low_level.S
)

# 添加包含目录
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/third/CherryUSB/core"
    "${CMAKE_CURRENT_SOURCE_DIR}/User"
    "${CMAKE_CURRENT_SOURCE_DIR}/third/DAP/Config"
    "${CMAKE_CURRENT_SOURCE_DIR}/third/DAP/Include"
    "${CMAKE_CURRENT_SOURCE_DIR}/third/CherryUSB/common"
    "${CMAKE_CURRENT_SOURCE_DIR}/third/CherryUSB/class/cdc"
    "${CMAKE_CURRENT_SOURCE_DIR}/third/CherryUSB/class/msc"
    "${CMAKE_CURRENT_SOURCE_DIR}/third/CherryUSB/class/hid"
    "${CMAKE_CURRENT_SOURCE_DIR}/third/CherryRB"
    "${CMAKE_CURRENT_SOURCE_DIR}/drivers/display"
)

# 设置链接脚本（项目特定）
set(LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/Libraries/Device/Geehy/APM32F4xx/Source/gcc/APM32F4xxxE_FLASH.ld")
set_property(TARGET ${CMAKE_PROJECT_NAME} APPEND PROPERTY LINK_DEPENDS ${LINKER_SCRIPT})
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    -T ${LINKER_SCRIPT}
    -Wl,-Map=${CMAKE_PROJECT_NAME}.map
)

# 链接标准库
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
    c
    m
    # 如果使用 C++，取消注释以下两行：
    # stdc++
    # supc++
)

# 设置输出目录
set(CMAKE_EXECUTABLE_OUTPUT_PATH "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}/lib")

# 生成 hex 文件
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${CMAKE_PROJECT_NAME}> $<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/${CMAKE_PROJECT_NAME}.hex
    COMMENT "Generating HEX file"
)

# 生成 bin 文件
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${CMAKE_PROJECT_NAME}> $<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/${CMAKE_PROJECT_NAME}.bin
    COMMENT "Generating BIN file"
)

# 显示内存使用
add_custom_target(size
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
    DEPENDS ${CMAKE_PROJECT_NAME}
    COMMENT "Memory usage"
)

# 复制 compile_commands.json 到项目根目录
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_SOURCE_DIR}/compile_commands.json
    COMMENT "Copying compile_commands.json to project directory"
)

# 显示构建信息
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Target processor: ${ARCHITECTURE}")
message(STATUS "FPU type: ${FPU_TYPE}")
message(STATUS "FPU ABI: ${FPU_ABI}")
message(STATUS "Linker script: ${LINKER_SCRIPT}")
