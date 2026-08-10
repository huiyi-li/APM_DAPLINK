
# APM32F4xx目录的CMakeLists.txt
# 这个文件负责编译APM32F4xx设备特定的源文件

# 设置库名称
set(LIB_NAME apm32f4xx_device)

# 创建静态库
add_library(${LIB_NAME} STATIC)

# 添加源文件
file(GLOB SOURCES 
    "Source/*.c"
    "Source/*.S"
)

# Match the Keil project: use the application-owned system clock source
# instead of the SDK example copy from this device library.
list(FILTER SOURCES EXCLUDE REGEX ".*/system_apm32f4xx\\.c$")

# 添加源文件到库
target_sources(${LIB_NAME} PRIVATE
    ${SOURCES}
)

# 添加包含目录
target_include_directories(${LIB_NAME} PUBLIC
    # 包含目录
    "Include"
)

# 添加编译定义
target_compile_definitions(${LIB_NAME} PRIVATE
    # 根据项目需求添加编译定义
    USE_HAL_DRIVER
)
