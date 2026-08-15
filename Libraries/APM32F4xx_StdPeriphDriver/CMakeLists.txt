
# APM32F4xx_StdPeriphDriver目录的CMakeLists.txt
# 这个文件负责编译APM32F4xx标准外设驱动库

# 设置库名称
set(LIB_NAME apm32f4xx_stdperiph)

# 创建静态库
add_library(${LIB_NAME} STATIC)

# 添加源文件
file(GLOB SOURCES 
    "src/*.c"
)

# 明确列出要排除的文件
set(EXCLUDED_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/apm32f4xx_usb_device.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/apm32f4xx_usb_host.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/apm32f4xx_usb.c
        # 该文件 #include "apm32f4Xx_sdio.h" 大小写与文件名不一致,
        # Windows 文件系统大小写不敏感可编译,Linux 上会报错。本项目未使用 SDIO。
        ${CMAKE_CURRENT_SOURCE_DIR}/src/apm32f4xx_sdio.c
)

# 从源文件列表中移除排除的文件
list(REMOVE_ITEM SOURCES ${EXCLUDED_SOURCES})

# 条件编译：根据宏定义添加源文件
target_sources(${LIB_NAME} PRIVATE
    ${SOURCES}
)

# 添加包含目录
target_include_directories(${LIB_NAME} PUBLIC
    # 包含目录
    "inc"
)

# 添加编译定义
target_compile_definitions(${LIB_NAME} PRIVATE
    # 根据项目需求添加编译定义

)
