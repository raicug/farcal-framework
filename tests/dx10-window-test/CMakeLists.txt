add_executable(dx10-window-test
    main.cpp
)

target_compile_features(dx10-window-test PRIVATE cxx_std_20)

target_link_libraries(dx10-window-test
    PRIVATE
        farcal::framework
)
