set(COMMON_INCLUDES
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/vendor/tomlc17
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include
)

set(BASE_SOURCES
    ${CMAKE_SOURCE_DIR}/src/util.c
    ${CMAKE_SOURCE_DIR}/src/config.c
    ${CMAKE_SOURCE_DIR}/vendor/tomlc17/tomlc17.c
)

add_executable(test_config
    test_config.c
    ${BASE_SOURCES}
)
target_include_directories(test_config PRIVATE ${COMMON_INCLUDES})
target_link_libraries(test_config PRIVATE Threads::Threads)

add_executable(test_util
    test_util.c
    ${BASE_SOURCES}
)
target_include_directories(test_util PRIVATE ${COMMON_INCLUDES})
target_link_libraries(test_util PRIVATE Threads::Threads)

add_executable(test_git
    test_git.c
    ${BASE_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/git.c
)
target_include_directories(test_git PRIVATE ${COMMON_INCLUDES})
target_link_libraries(test_git PRIVATE Threads::Threads)

add_executable(test_cli
    test_cli.c
    ${BASE_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/cli.c
    ${CMAKE_SOURCE_DIR}/src/git.c
    ${CMAKE_SOURCE_DIR}/src/setup.c
    ${CMAKE_SOURCE_DIR}/src/tui.c
    ${CMAKE_SOURCE_DIR}/src/sync.c
    ${CMAKE_SOURCE_DIR}/src/upgrade.c
    ${CMAKE_SOURCE_DIR}/src/daemon.c
    ${CMAKE_SOURCE_DIR}/src/watcher.c
)
target_include_directories(test_cli PRIVATE ${COMMON_INCLUDES})
target_link_libraries(test_cli PRIVATE Threads::Threads)

add_test(NAME test_config COMMAND test_config)
add_test(NAME test_util COMMAND test_util)
add_test(NAME test_git COMMAND test_git)
add_test(NAME test_cli COMMAND test_cli)
