
# bsp目录的CMakeLists.txt
# 这个文件负责编译板级支持包相关的源文件

# 设置库名称
set(LIB_NAME bsp)

# 创建静态库
add_library(${LIB_NAME} STATIC)

target_include_directories(${LIB_NAME} PUBLIC
        inc
        ${CMAKE_SOURCE_DIR}/third/threadx/common/inc
        ${CMAKE_SOURCE_DIR}/third/threadx/ports/cortex_m4/gnu/inc
)

target_sources(${LIB_NAME} PRIVATE
    src/bsp_button.c
    src/bsp_cdc_uart.c
    src/bsp_delay.c
    src/bsp_led.c
    src/bsp_lcd_bus.c
    src/bsp_printf.c
    src/bsp_sysclk.c
    src/bsp_uart1.c
    src/bsp_w25qxx.c
    src/button.c
)

# 添加编译定义
target_compile_definitions(${LIB_NAME} PRIVATE

)

# W25Q driver is byte-loop heavy; optimize it.
set_source_files_properties(src/bsp_w25qxx.c PROPERTIES COMPILE_OPTIONS -O2)
