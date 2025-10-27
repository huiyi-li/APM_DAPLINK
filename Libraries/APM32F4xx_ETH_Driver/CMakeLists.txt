
# APM32F4xx_ETH_Driver目录的CMakeLists.txt
# 这个文件负责编译APM32F4xx以太网驱动库

# 设置库名称
set(LIB_NAME apm32f4xx_eth)

# 创建静态库
add_library(${LIB_NAME} STATIC)

# 添加源文件
file(GLOB SOURCES 
    "src/*.c"
)

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
    USE_HAL_DRIVER
    ENABLE_ETH
)
