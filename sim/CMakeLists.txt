cmake_minimum_required(VERSION 3.16)
project(daplink_sim C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(LVGL_DIR "${CMAKE_CURRENT_LIST_DIR}/../third/lvgl")
file(REAL_PATH ${LVGL_DIR} LVGL_DIR)
set(LV_BUILD_CONF_DIR ${CMAKE_CURRENT_SOURCE_DIR} CACHE PATH "simulator lv_conf.h" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(${LVGL_DIR} lvgl)
target_compile_options(lvgl PRIVATE -O2)

set(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
file(REAL_PATH ${REPO_ROOT} REPO_ROOT)

add_executable(daplink_sim
    main.c
    sim_fs.c
    sim_flash.c
    sim_init.c
    sim_key.c
    autotest.c
    ${REPO_ROOT}/ui/app_ui.c
    ${REPO_ROOT}/ui/app_algo.c
)

target_include_directories(daplink_sim PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${REPO_ROOT}/ui
)

find_package(SDL2 REQUIRED)
target_link_libraries(daplink_sim PRIVATE lvgl SDL2::SDL2 pthread m)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()
