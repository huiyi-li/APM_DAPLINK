
# CMSIS-DAP is configured for SWD only; JTAG, SWO and DAP-UART are disabled.
# Built as a separate static library so the SWD bit-banging hot path can be
# compiled with -O2 even in Debug builds, maximizing SWCLK/SWDIO throughput.
add_library(dap_sw STATIC
    Source/DAP.c
    Source/DAP_vendor.c
    Source/SW_DP.c
)

target_include_directories(dap_sw PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Include"
    "${CMAKE_CURRENT_SOURCE_DIR}/Config"
)

target_include_directories(dap_sw PRIVATE
    "${CMAKE_SOURCE_DIR}/Libraries/Device/Geehy/APM32F4xx/Include"
    "${CMAKE_SOURCE_DIR}/Libraries/CMSIS/Include"
    "${CMAKE_SOURCE_DIR}/Libraries/APM32F4xx_StdPeriphDriver/inc"
    "${CMAKE_SOURCE_DIR}/bsp/inc"
)

target_compile_options(dap_sw PRIVATE -O2)

target_compile_definitions(dap_sw PRIVATE
)
