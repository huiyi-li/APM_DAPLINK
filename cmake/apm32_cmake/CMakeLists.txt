cmake_minimum_required(VERSION 3.21)

project(apm32_cmake)
add_library(apm32_cmake INTERFACE)

# Enable CMake support for ASM and C languages
enable_language(C ASM)

target_compile_definitions(apm32_cmake INTERFACE 
	APM32F40X
    $<$<CONFIG:Debug>:DEBUG>
)

# 定义基础路径变量
set(PROJECT_ROOT "${CMAKE_SOURCE_DIR}")  # 项目根目录
set(LIBRARY_PATH "../../Libraries")           # 第三方库路径

# 接口包含目录
target_include_directories(apm32_cmake INTERFACE
    "${PROJECT_ROOT}/User" 
    "${PROJECT_ROOT}/bsp/inc"
    "${LIBRARY_PATH}/APM32F4xx_StdPeriphDriver/inc"
    "${LIBRARY_PATH}/Device/Geehy/APM32F4xx/Include"
    "${LIBRARY_PATH}/CMSIS/Include"
)

# 定义源文件组
set(C_SOURCES
    "${PROJECT_ROOT}/User/main.c"
    "${PROJECT_ROOT}/User/system_apm32f4xx.c"
    "${PROJECT_ROOT}/User/apm32f4xx_int.c"
    "${PROJECT_ROOT}/bsp/src/bsp_delay.c"
    "${PROJECT_ROOT}/bsp/src/bsp_sdcard.c"
    "${PROJECT_ROOT}/bsp/src/bsp_w25qxx.c"
)

# 自动收集标准外设驱动源文件
file(GLOB APM32_DRIVER_SRC "${LIBRARY_PATH}/APM32F4xx_StdPeriphDriver/src/*.c")

# 汇编文件独立管理
set(ASM_SOURCES
    "${LIBRARY_PATH}/Device/Geehy/APM32F4xx/Source/gcc/startup_apm32f40x.S"
)

# 注册源文件到目标
target_sources(apm32_cmake INTERFACE
    ${C_SOURCES}
    ${APM32_DRIVER_SRC}
    ${ASM_SOURCES}
)

target_link_directories(apm32_cmake INTERFACE
)

target_link_libraries(apm32_cmake INTERFACE
)

# Validate that apm32_cmake code is compatible with C standard
if(CMAKE_C_STANDARD LESS 11)
    message(ERROR "Generated code requires C11 or higher")
endif()


