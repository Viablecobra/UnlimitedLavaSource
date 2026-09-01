cmake_minimum_required(VERSION 3.22.1)
project(ULavaSource)
set(CMAKE_CXX_STANDARD 20)

add_library(preloader SHARED IMPORTED)
set_target_properties(preloader PROPERTIES
    IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/libs/libpreloader.so)

add_library(ULavaSource SHARED
    main.cpp
)

target_include_directories(ULavaSource PRIVATE
    include
    ${CMAKE_SOURCE_DIR}/../SDK
)

target_link_options(ULavaSource PRIVATE
    -Wl,--strip-debug
    -Wl,--gc-sections
    -Wl,--exclude-libs,ALL
)

target_link_libraries(ULavaSource PRIVATE
    android
    log
    preloader
)