
# Libraries目录的CMakeLists.txt
# 这个文件负责编译所有库文件，并根据宏定义条件编译

# 设置库名称
set(LIB_NAME apm32_cmsis_libs)

# 创建静态库
#add_library(${LIB_NAME} STATIC)

# 如果启用了以太网功能，添加以太网驱动源文件
if(ENABLE_ETH)
    add_subdirectory(APM32F4xx_ETH_Driver)
endif()

if(ENABLE_STD)
    add_subdirectory(APM32F4xx_StdPeriphDriver)
    target_link_libraries(apm32f4xx_stdperiph apm32_device_headers)
    target_link_libraries(apm32f4xx_stdperiph apm32_cmsis_headers)
endif()

# 添加子目录
if(ENABLE_CMSIS)
    # 头文件总是通过 include_directories 添加
    if(ENABLE_CMSIS_SOURCES)
        add_subdirectory(CMSIS)
        # 后续可链接 apm32_cmsis
    else()
        # 仍然进入 CMSIS 目录以执行 include_directories
        add_subdirectory(CMSIS)
    endif()
endif()

# 添加子目录
if(ENABLE_DEVICE)
    add_subdirectory(Device)
    target_link_libraries(apm32_device_headers apm32_cmsis_headers)
endif()




