cmake_minimum_required(VERSION 3.24)

project(farcal LANGUAGES CXX)

set(UI_FRAMEWORK_CHANNEL "local" CACHE STRING "Output channel name used under bin/<channel>/<config>.")

if(CMAKE_CONFIGURATION_TYPES)
    foreach(config IN LISTS CMAKE_CONFIGURATION_TYPES)
        string(TOLOWER "${config}" config_lower)
        string(TOUPPER "${config}" config_upper)

        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config_upper} "${CMAKE_SOURCE_DIR}/bin/${UI_FRAMEWORK_CHANNEL}/${config_lower}")
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${config_upper} "${CMAKE_SOURCE_DIR}/bin/${UI_FRAMEWORK_CHANNEL}/${config_lower}")
        set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${config_upper} "${CMAKE_SOURCE_DIR}/bin/${UI_FRAMEWORK_CHANNEL}/${config_lower}")
    endforeach()
else()
    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
    endif()

    string(TOLOWER "${CMAKE_BUILD_TYPE}" config_lower)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/${UI_FRAMEWORK_CHANNEL}/${config_lower}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/${UI_FRAMEWORK_CHANNEL}/${config_lower}")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/${UI_FRAMEWORK_CHANNEL}/${config_lower}")
endif()

add_subdirectory(framework)
add_subdirectory(tests)
