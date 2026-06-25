add_executable(dx11-window-test
    main.cpp
)

target_compile_features(dx11-window-test PRIVATE cxx_std_20)

target_link_libraries(dx11-window-test
    PRIVATE
        farcal::framework
        user32
        d3d11
        dxgi
        dxguid
)
