cmake_minimum_required(VERSION 3.20)
project(hyprsync LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(WITH_SYSTEMD "Enable systemd sd_notify integration" OFF)
option(BUILD_TESTING "Build tests" ON)

# CalVer version generation (Year.Month.Number)
string(TIMESTAMP HYPRSYNC_VERSION_YEAR "%Y")
string(TIMESTAMP HYPRSYNC_VERSION_MONTH "%m")
string(TIMESTAMP HYPRSYNC_BUILD_DATE "%Y-%m-%d")

string(REGEX REPLACE "^0" "" HYPRSYNC_VERSION_MONTH "${HYPRSYNC_VERSION_MONTH}")

execute_process(
    COMMAND git rev-list --count --since="${HYPRSYNC_VERSION_YEAR}-${HYPRSYNC_VERSION_MONTH}-01" HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE HYPRSYNC_VERSION_NUMBER
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT HYPRSYNC_VERSION_NUMBER OR HYPRSYNC_VERSION_NUMBER STREQUAL "")
    set(HYPRSYNC_VERSION_NUMBER "1")
endif()

execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE HYPRSYNC_GIT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT HYPRSYNC_GIT_COMMIT)
    set(HYPRSYNC_GIT_COMMIT "unknown")
endif()

set(HYPRSYNC_VERSION "${HYPRSYNC_VERSION_YEAR}.${HYPRSYNC_VERSION_MONTH}.${HYPRSYNC_VERSION_NUMBER}")
message(STATUS "HyprSync version: ${HYPRSYNC_VERSION} (${HYPRSYNC_GIT_COMMIT})")

set(SOURCES
    src/main.c
    src/daemon.c
    src/config.c
    src/watcher.c
    src/sync.c
    src/git.c
    src/setup.c
    src/cli.c
    src/util.c
    src/log.c
    src/tui.c
    src/upgrade.c
    vendor/tomlc17/tomlc17.c
)

add_executable(hyprsync ${SOURCES})

target_include_directories(hyprsync PRIVATE
    ${CMAKE_SOURCE_DIR}/vendor/tomlc17
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

configure_file(
    ${CMAKE_SOURCE_DIR}/include/hyprsync/version.h.in
    ${CMAKE_BINARY_DIR}/include/hyprsync/version.h
)
target_include_directories(hyprsync PRIVATE ${CMAKE_BINARY_DIR}/include)

if(WITH_SYSTEMD)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SYSTEMD REQUIRED libsystemd)
    target_link_libraries(hyprsync PRIVATE ${SYSTEMD_LIBRARIES})
    target_include_directories(hyprsync PRIVATE ${SYSTEMD_INCLUDE_DIRS})
    target_compile_definitions(hyprsync PRIVATE WITH_SYSTEMD)
endif()

find_package(Threads REQUIRED)
target_link_libraries(hyprsync PRIVATE Threads::Threads)

target_compile_options(hyprsync PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror=return-type
)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

install(TARGETS hyprsync DESTINATION bin)
install(FILES config/hyprsync.toml.example DESTINATION share/hyprsync)
install(FILES systemd/hyprsync.service DESTINATION lib/systemd/user)
