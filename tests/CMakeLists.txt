set(COMMON_INCLUDES
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/vendor/tomlplusplus/include
    ${CMAKE_SOURCE_DIR}/vendor/spdlog/include
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include
)

set(BASE_SOURCES
    ${CMAKE_SOURCE_DIR}/src/util.cpp
    ${CMAKE_SOURCE_DIR}/src/config.cpp
)

add_executable(test_config
    test_config.cpp
    ${BASE_SOURCES}
)
target_include_directories(test_config PRIVATE ${COMMON_INCLUDES})
target_link_libraries(test_config PRIVATE GTest::gtest_main Threads::Threads)

add_executable(test_util
    test_util.cpp
    ${BASE_SOURCES}
)
target_include_directories(test_util PRIVATE ${COMMON_INCLUDES})
target_link_libraries(test_util PRIVATE GTest::gtest_main Threads::Threads)

add_executable(test_git
    test_git.cpp
    ${BASE_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/git.cpp
)
target_include_directories(test_git PRIVATE ${COMMON_INCLUDES})
target_link_libraries(test_git PRIVATE GTest::gtest_main Threads::Threads)

add_executable(test_cli
    test_cli.cpp
    ${BASE_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/cli.cpp
    ${CMAKE_SOURCE_DIR}/src/git.cpp
    ${CMAKE_SOURCE_DIR}/src/setup.cpp
    ${CMAKE_SOURCE_DIR}/src/tui.cpp
    ${CMAKE_SOURCE_DIR}/src/sync.cpp
    ${CMAKE_SOURCE_DIR}/src/upgrade.cpp
    ${CMAKE_SOURCE_DIR}/src/daemon.cpp
    ${CMAKE_SOURCE_DIR}/src/watcher.cpp
)
target_include_directories(test_cli PRIVATE ${COMMON_INCLUDES})
target_link_libraries(test_cli PRIVATE GTest::gtest_main Threads::Threads)

include(GoogleTest)
gtest_discover_tests(test_config)
gtest_discover_tests(test_util)
gtest_discover_tests(test_git)
gtest_discover_tests(test_cli)
