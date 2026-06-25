add_library(framework STATIC
    src/main.cpp
)

add_library(ui::framework ALIAS framework)

target_compile_features(framework PUBLIC cxx_std_20)

target_include_directories(framework
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
