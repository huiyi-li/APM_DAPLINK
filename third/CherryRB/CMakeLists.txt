# CMSIS/CMakeLists.txt

set(LIB_NAME CherryRB)
add_library(${LIB_NAME} STATIC)

target_include_directories(${LIB_NAME} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_sources(${LIB_NAME} PRIVATE
        chry_ringbuffer.c
)
