set(TEST_SOURCES
    test_config.cpp
    test_util.cpp
)

foreach(test_source ${TEST_SOURCES})
    get_filename_component(test_name ${test_source} NAME_WE)

    add_executable(${test_name}
        ${test_source}
        ${CMAKE_SOURCE_DIR}/src/util.cpp
        ${CMAKE_SOURCE_DIR}/src/config.cpp
    )

    target_include_directories(${test_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/vendor/tomlplusplus/include
        ${CMAKE_SOURCE_DIR}/vendor/spdlog/include
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_BINARY_DIR}/include
    )

    target_link_libraries(${test_name} PRIVATE
        GTest::gtest_main
        Threads::Threads
    )

    include(GoogleTest)
    gtest_discover_tests(${test_name})
endforeach()
