cmake_minimum_required(VERSION 3.20)

project(libnavinput C)

if(NOT WIN32)
    message(FATAL_ERROR "libnavinput is a Windows-only library.")
endif()

if(MSVC)
    set(PSNAV_COMPILER_KIND "msvc")
elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    set(PSNAV_COMPILER_KIND "gcc")
elseif(CMAKE_C_COMPILER_ID STREQUAL "TinyCC")
    set(PSNAV_COMPILER_KIND "tcc")
else()
    message(
        FATAL_ERROR
        "Unsupported C compiler '${CMAKE_C_COMPILER_ID}'. Supported compilers: MSVC, GNU, TinyCC."
    )
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(PSNAV_SOURCES
    src/psnavigator.c
    src/psnavigator_decode.c
    src/psnavigator_winusb.c
)

set(PSNAV_COMPILE_DEFINITIONS PSNAVIGATOR_BUILD)
set(PSNAV_COMPILE_OPTIONS)
set(PSNAV_LINK_LIBRARIES setupapi winusb)
set(PSNAV_LINK_ITEMS)
set(PSNAV_LINK_OPTIONS)

if(PSNAV_COMPILER_KIND STREQUAL "gcc")
    list(APPEND PSNAV_COMPILE_OPTIONS -O2 -Wall -Wextra)
elseif(PSNAV_COMPILER_KIND STREQUAL "msvc")
    list(APPEND PSNAV_COMPILE_DEFINITIONS _CRT_SECURE_NO_WARNINGS)
    list(APPEND PSNAV_COMPILE_OPTIONS
        /W4
        "$<$<CONFIG:Release>:/O2>"
        "$<$<CONFIG:RelWithDebInfo>:/O2>"
        "$<$<CONFIG:MinSizeRel>:/O1>"
    )
elseif(PSNAV_COMPILER_KIND STREQUAL "tcc")
    set(PSNAV_LINK_LIBRARIES)
    list(APPEND PSNAV_LINK_OPTIONS -lsetupapi -lwinusb -lkernel32)
endif()

add_library(navinput SHARED ${PSNAV_SOURCES})

target_include_directories(
    navinput
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_definitions(navinput PRIVATE ${PSNAV_COMPILE_DEFINITIONS})
target_compile_options(navinput PRIVATE ${PSNAV_COMPILE_OPTIONS})
target_link_options(navinput PRIVATE ${PSNAV_LINK_OPTIONS})
target_link_libraries(navinput PRIVATE ${PSNAV_LINK_LIBRARIES} ${PSNAV_LINK_ITEMS})

set_target_properties(
    navinput
    PROPERTIES
        OUTPUT_NAME "libnavinput"
        PREFIX ""
)

if(CMAKE_CONFIGURATION_TYPES)
    foreach(config_name IN LISTS CMAKE_CONFIGURATION_TYPES)
        string(TOUPPER "${config_name}" config_name_upper)
        set_target_properties(
            navinput
            PROPERTIES
                "RUNTIME_OUTPUT_DIRECTORY_${config_name_upper}" "${CMAKE_CURRENT_BINARY_DIR}/bin/${config_name}"
                "ARCHIVE_OUTPUT_DIRECTORY_${config_name_upper}" "${CMAKE_CURRENT_BINARY_DIR}/lib/${config_name}"
                "LIBRARY_OUTPUT_DIRECTORY_${config_name_upper}" "${CMAKE_CURRENT_BINARY_DIR}/bin/${config_name}"
        )
    endforeach()
else()
    set_target_properties(
        navinput
        PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
            ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib"
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
    )
endif()
