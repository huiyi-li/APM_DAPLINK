# cmake/gcc-arm-none-eabi.cmake
# 适用于 APM32F4 系列（Cortex-M4 + FPU）

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR cortex-m4)

# 强制编译器 ID
set(CMAKE_C_COMPILER_FORCED TRUE)
set(CMAKE_CXX_COMPILER_FORCED TRUE)
set(CMAKE_C_COMPILER_ID GNU)
set(CMAKE_CXX_COMPILER_ID GNU)

# 工具链前缀
if(NOT DEFINED TOOLCHAIN_PREFIX)
    set(TOOLCHAIN_PREFIX arm-none-eabi-)
endif()

# 编译器和工具
set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++)
set(CMAKE_ASM_COMPILER ${CMAKE_C_COMPILER})  # 复用 C 编译器
set(CMAKE_OBJCOPY ${TOOLCHAIN_PREFIX}objcopy CACHE INTERNAL "")
set(CMAKE_SIZE ${TOOLCHAIN_PREFIX}size CACHE INTERNAL "")
set(CMAKE_OBJDUMP ${TOOLCHAIN_PREFIX}objdump CACHE INTERNAL "")
set(CMAKE_READELF ${TOOLCHAIN_PREFIX}readelf CACHE INTERNAL "")

# 输出后缀
set(CMAKE_EXECUTABLE_SUFFIX ".elf")

# 避免 CMake 尝试链接可执行文件进行编译器测试
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# 跳过编译器工作测试（加速配置）
set(CMAKE_C_COMPILER_WORKS TRUE)
set(CMAKE_CXX_COMPILER_WORKS TRUE)

# 默认目标配置
if(NOT DEFINED TARGET_PROCESSOR)
    set(TARGET_PROCESSOR "cortex-m4")
endif()
if(NOT DEFINED TARGET_FPU)
    set(TARGET_FPU "fpv4-sp-d16")
endif()
if(NOT DEFINED TARGET_FPU_ABI)
    set(TARGET_FPU_ABI "hard")
endif()

# 通用编译标志
set(TARGET_FLAGS "-mcpu=${TARGET_PROCESSOR} -mfpu=${TARGET_FPU} -mfloat-abi=${TARGET_FPU_ABI} -mthumb")
set(COMMON_FLAGS "-Wall -Wextra -Wpedantic -fdata-sections -ffunction-sections")

# 根据构建类型设置优化级别
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(OPT_FLAGS "-O0 -g3")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(OPT_FLAGS "-Os -g0")
elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    set(OPT_FLAGS "-O2 -g2")
else()
    set(OPT_FLAGS "-Os")
endif()

# 初始化编译和链接标志
set(CMAKE_C_FLAGS_INIT "${TARGET_FLAGS} ${COMMON_FLAGS} ${OPT_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${CMAKE_C_FLAGS_INIT} -fno-rtti -fno-exceptions")
set(CMAKE_ASM_FLAGS_INIT "${TARGET_FLAGS} -x assembler-with-cpp")

set(CMAKE_EXE_LINKER_FLAGS_INIT
    "${TARGET_FLAGS} --specs=nano.specs --specs=nosys.specs -Wl,--gc-sections"
)