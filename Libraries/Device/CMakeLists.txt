# CMSIS/CMakeLists.txt

# 无条件添加头文件到全局（不推荐，但简单）
add_library(apm32_device_headers STATIC)

target_include_directories(apm32_device_headers PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Geehy/APM32F4xx/Include
)

target_sources(apm32_device_headers PRIVATE
        Geehy/APM32F4xx/Source/system_apm32f4xx.c
)

# 条件编译库
if(ENABLE_CMSIS_DEVICE)
    add_library(apm32_cmsis STATIC
            Core/Source/*.c
            Core/Source/*.S
            Device/Geehy/APM32F4xx/Source/*.c
            Device/Geehy/APM32F4xx/Source/*.S
    )
endif()