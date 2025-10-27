
# APM32_USB_Library目录的CMakeLists.txt
# 这个文件负责编译APM32 USB库相关的源文件

# 设置库名称
set(LIB_NAME apm32_usb_library)

# 创建静态库
add_library(${LIB_NAME} STATIC)

# 添加USB库源文件
file(GLOB SOURCES 
    "src/*.c"
)

# 条件编译：根据宏定义添加源文件
target_sources(${LIB_NAME} PRIVATE
    ${SOURCES}
)

# 添加包含目录
target_include_directories(${LIB_NAME} PUBLIC
    # USB库包含目录
    "inc"
)

# 添加编译定义
target_compile_definitions(${LIB_NAME} PRIVATE
    # 根据项目需求添加编译定义
    USE_HAL_DRIVER
    ENABLE_USB
)

# 条件编译：根据宏定义添加特定功能
if(ENABLE_CDC)
    target_compile_definitions(${LIB_NAME} PRIVATE ENABLE_CDC)
endif()

if(ENABLE_MSC)
    target_compile_definitions(${LIB_NAME} PRIVATE ENABLE_MSC)
endif()

if(ENABLE_HID)
    target_compile_definitions(${LIB_NAME} PRIVATE ENABLE_HID)
endif()
