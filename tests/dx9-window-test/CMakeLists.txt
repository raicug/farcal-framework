add_executable(dx9-window-test
    main.cpp
)

target_compile_features(dx9-window-test PRIVATE cxx_std_20)

target_link_libraries(dx9-window-test
    PRIVATE
        farcal::framework
)
