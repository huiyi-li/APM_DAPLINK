# CMSIS/CMakeLists.txt

# 无条件添加头文件到全局（不推荐，但简单）
add_library(apm32_cmsis_headers INTERFACE)
target_include_directories(apm32_cmsis_headers INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/Include
)

# 条件编译库
if(ENABLE_CMSIS_SOURCES)
    add_library(apm32_cmsis STATIC
            Core/Source/*.c
            Core/Source/*.S
            Device/Geehy/APM32F4xx/Source/*.c
            Device/Geehy/APM32F4xx/Source/*.S
    )
endif()