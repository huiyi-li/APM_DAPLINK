
add_subdirectory(DAP)

set(CHERRYUSB_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/CherryUSB/core/usbd_core.c
    ${CMAKE_CURRENT_SOURCE_DIR}/CherryUSB/class/cdc/usbd_cdc_acm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/CherryUSB/class/hid/usbd_hid.c
    ${CMAKE_CURRENT_SOURCE_DIR}/CherryUSB/port/dwc2/usb_dc_dwc2.c
)

# 添加源文件到库
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${CHERRYUSB_SRC}
)

# 添加包含目录
target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC
    # CherryUSB包含目录
        "${CMAKE_CURRENT_SOURCE_DIR}/CherryUSB/common"
        "${CMAKE_CURRENT_SOURCE_DIR}/CherryUSB/core"
        "${CMAKE_CURRENT_SOURCE_DIR}/CherryUSB/class/cdc"
        "${CMAKE_CURRENT_SOURCE_DIR}/CherryUSB/class/hid"
        "${CMAKE_CURRENT_SOURCE_DIR}/CherryUSB/class/msc"
        "${CMAKE_CURRENT_SOURCE_DIR}/CherryUSB/port/dwc2"

)

# 添加编译定义
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE

)

