
file(GLOB CHERRYUSB_SRC
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.c"
)

# 添加源文件到库
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${CHERRYUSB_SRC}
)

# 添加包含目录
target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/Include"
        "${CMAKE_CURRENT_SOURCE_DIR}/Config"
)

# 添加编译定义
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE

)

