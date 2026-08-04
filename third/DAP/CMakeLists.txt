
# CMSIS-DAP is configured for SWD only; JTAG, SWO and DAP-UART are disabled.
set(DAP_SOURCE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/DAP.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/DAP_vendor.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/SW_DP.c
)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${DAP_SOURCE}
)

# 添加包含目录
target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/Include"
        "${CMAKE_CURRENT_SOURCE_DIR}/Config"
)

# 添加编译定义
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE

)

